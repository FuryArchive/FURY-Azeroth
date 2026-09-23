#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MYSQL_HOST="${MYSQL_HOST:-127.0.0.1}"
MYSQL_PORT="${MYSQL_PORT:-3306}"
MYSQL_USER="${MYSQL_USER:-root}"
MYSQL_PASSWORD="${MYSQL_PASSWORD:-fury}"
MYSQL_DATABASE="${MYSQL_DATABASE:-acore_fury}"

export MYSQL_HOST MYSQL_PORT MYSQL_USER MYSQL_PASSWORD MYSQL_DATABASE
export MYSQL_PWD="${MYSQL_PASSWORD}"

bash "${ROOT}/scripts/test-m1-schema.sh"

mysql_cmd=(
  mysql --protocol=tcp
  --host="${MYSQL_HOST}"
  --port="${MYSQL_PORT}"
  --user="${MYSQL_USER}"
  --batch --skip-column-names
  "${MYSQL_DATABASE}"
)

sql() { "${mysql_cmd[@]}" -e "$1"; }

assert_eq() {
  local expected="$1"
  local actual="$2"
  local message="$3"

  if [[ "${actual}" != "${expected}" ]]; then
    echo "[FURY][FAIL] ${message}: expected=${expected} actual=${actual}" >&2
    exit 1
  fi

  echo "[FURY][PASS] ${message}"
}

for table in fury_director_graph fury_director_run; do
  assert_eq "1" "$(sql "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema='${MYSQL_DATABASE}' AND table_name='${table}';")" "table ${table} exists"
done

echo "[FURY] T15 -> T16 migration path"
sql "DROP TABLE IF EXISTS fury_director_score_award; DROP TABLE IF EXISTS fury_director_score_component; DROP TABLE fury_director_run; DROP TABLE fury_director_graph;"
"${mysql_cmd[@]}" < "${ROOT}/modules/mod-fury/data/sql/fury/updates/2026_09_22_04_director.sql"
for table in fury_director_graph fury_director_run; do
  assert_eq "1" "$(sql "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema='${MYSQL_DATABASE}' AND table_name='${table}';")" "migration creates ${table}"
done

assert_eq "2" "$(sql "SELECT COUNT(*) FROM information_schema.statistics WHERE table_schema='${MYSQL_DATABASE}' AND table_name='fury_director_run' AND index_name='uq_fury_director_active_scope';")" "exclusive active-scope index has both key columns"

household_id="$(sql "SELECT id FROM fury_household WHERE slug='alpha' LIMIT 1;")"
sql "INSERT IGNORE INTO fury_campaign_node
  (node_key, era, ordinal, display_name, required_power_band, grants_power_band, enabled)
  VALUES ('golden.director.node',1,900,'Golden Director Node',0,0,1);"

sql "INSERT INTO fury_director_graph
  (graph_key, scope_key, display_name, campaign_node_key, enabled)
  VALUES
  ('golden.director','golden.scope','Golden Director','golden.director.node',1),
  ('golden.director.other','golden.scope','Golden Director Other','golden.director.node',1);"

sql "INSERT INTO fury_event
  (event_type, actor_kind, household_id, source_system, dedupe_key, payload)
  VALUES
  ('golden.director.start',1,${household_id},'golden.director',
   UNHEX(SHA2('golden-director-start',256)),JSON_OBJECT());"
start_event_id="$(sql "SELECT id FROM fury_event WHERE dedupe_key=UNHEX(SHA2('golden-director-start',256));")"

echo "[FURY] duplicate start cannot create two active runs in one scope"
sql "INSERT IGNORE INTO fury_director_run
  (household_id, graph_key, scope_key, status, phase_key, started_event_id, last_event_id)
  VALUES
  (${household_id},'golden.director','golden.scope',2,'rumours',${start_event_id},${start_event_id});"
sql "INSERT IGNORE INTO fury_director_run
  (household_id, graph_key, scope_key, status, phase_key, started_event_id, last_event_id)
  VALUES
  (${household_id},'golden.director','golden.scope',2,'rumours',${start_event_id},${start_event_id});"
sql "INSERT IGNORE INTO fury_director_run
  (household_id, graph_key, scope_key, status, phase_key, started_event_id, last_event_id)
  VALUES
  (${household_id},'golden.director.other','golden.scope',2,'rumours',${start_event_id},${start_event_id});"
assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_director_run WHERE household_id=${household_id} AND scope_key='golden.scope' AND status IN (1,2,3);")" "one active graph exists per household scope"

run_id="$(sql "SELECT id FROM fury_director_run WHERE household_id=${household_id} AND scope_key='golden.scope' AND status IN (1,2,3) LIMIT 1;")"

sql "INSERT INTO fury_event
  (event_type, actor_kind, household_id, source_system, dedupe_key, payload)
  VALUES
  ('golden.director.phase',5,${household_id},'golden.director',
   UNHEX(SHA2('golden-director-phase',256)),JSON_OBJECT());"
phase_event_id="$(sql "SELECT id FROM fury_event WHERE dedupe_key=UNHEX(SHA2('golden-director-phase',256));")"

echo "[FURY] optimistic revision transition"
sql "UPDATE fury_director_run
  SET phase_key='invasion', last_event_id=${phase_event_id}, revision=revision+1
  WHERE id=${run_id} AND household_id=${household_id} AND revision=0 AND status IN (1,2);"
assert_eq "invasion" "$(sql "SELECT phase_key FROM fury_director_run WHERE id=${run_id};")" "phase update applies"
assert_eq "1" "$(sql "SELECT revision FROM fury_director_run WHERE id=${run_id};")" "phase update advances revision"

sql "UPDATE fury_director_run
  SET phase_key='stale-write', revision=revision+1
  WHERE id=${run_id} AND household_id=${household_id} AND revision=0 AND status IN (1,2);"
assert_eq "invasion" "$(sql "SELECT phase_key FROM fury_director_run WHERE id=${run_id};")" "stale revision cannot overwrite phase"
assert_eq "1" "$(sql "SELECT revision FROM fury_director_run WHERE id=${run_id};")" "stale revision cannot advance revision"

sql "INSERT INTO fury_event
  (event_type, actor_kind, household_id, source_system, dedupe_key, payload)
  VALUES
  ('golden.director.runtime',5,${household_id},'golden.director',
   UNHEX(SHA2('golden-director-runtime',256)),JSON_OBJECT());"
runtime_event_id="$(sql "SELECT id FROM fury_event WHERE dedupe_key=UNHEX(SHA2('golden-director-runtime',256));")"

sql "UPDATE fury_director_run
  SET external_runtime_id=7001, last_event_id=${runtime_event_id}, revision=revision+1
  WHERE id=${run_id} AND household_id=${household_id} AND revision=1
    AND status IN (1,2,3) AND external_runtime_id IS NULL;"
assert_eq "7001" "$(sql "SELECT external_runtime_id FROM fury_director_run WHERE id=${run_id};")" "runtime binding persists"
assert_eq "2" "$(sql "SELECT revision FROM fury_director_run WHERE id=${run_id};")" "runtime binding advances revision"

sql "UPDATE fury_director_run
  SET external_runtime_id=9999, revision=revision+1
  WHERE id=${run_id} AND household_id=${household_id} AND revision=2
    AND status IN (1,2,3) AND external_runtime_id IS NULL;"
assert_eq "7001" "$(sql "SELECT external_runtime_id FROM fury_director_run WHERE id=${run_id};")" "attached runtime cannot be replaced"

echo "[FURY] active Director state survives schema re-apply/restart"
"${mysql_cmd[@]}" < "${ROOT}/modules/mod-fury/data/sql/fury/base/70_director.sql"
assert_eq "invasion" "$(sql "SELECT phase_key FROM fury_director_run WHERE id=${run_id};")" "phase survives schema re-apply"
assert_eq "7001" "$(sql "SELECT external_runtime_id FROM fury_director_run WHERE id=${run_id};")" "runtime binding survives schema re-apply"
assert_eq "2" "$(sql "SELECT revision FROM fury_director_run WHERE id=${run_id};")" "revision survives schema re-apply"
assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_director_run WHERE id=${run_id} AND status IN (1,2,3);")" "active run survives schema re-apply"

sql "INSERT INTO fury_event
  (event_type, actor_kind, household_id, source_system, dedupe_key, payload)
  VALUES
  ('golden.director.resolve',5,${household_id},'golden.director',
   UNHEX(SHA2('golden-director-resolve',256)),JSON_OBJECT());"
resolve_event_id="$(sql "SELECT id FROM fury_event WHERE dedupe_key=UNHEX(SHA2('golden-director-resolve',256));")"

echo "[FURY] terminal outcome frees exclusive scope"
sql "UPDATE fury_director_run
  SET status=4, outcome_key='success', resolved_event_id=${resolve_event_id},
      last_event_id=${resolve_event_id}, completed_at=CURRENT_TIMESTAMP(6),
      revision=revision+1
  WHERE id=${run_id} AND household_id=${household_id} AND revision=2
    AND status IN (1,2,3);"
assert_eq "4" "$(sql "SELECT status FROM fury_director_run WHERE id=${run_id};")" "run resolves"
assert_eq "success" "$(sql "SELECT outcome_key FROM fury_director_run WHERE id=${run_id};")" "outcome persists"

sql "INSERT IGNORE INTO fury_director_run
  (household_id, graph_key, scope_key, status, phase_key, started_event_id, last_event_id)
  VALUES
  (${household_id},'golden.director.other','golden.scope',2,'rumours',${start_event_id},${start_event_id});"
assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_director_run WHERE household_id=${household_id} AND scope_key='golden.scope' AND status IN (1,2,3);")" "scope is reusable after terminal outcome"

echo "[FURY][PASS] T16 Director schema/restart golden gate passed"
