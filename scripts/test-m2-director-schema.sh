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

bash "${ROOT}/scripts/test-m2-contracts-schema.sh"

mysql_cmd=(
  mysql
  --protocol=tcp
  --host="${MYSQL_HOST}"
  --port="${MYSQL_PORT}"
  --user="${MYSQL_USER}"
  --batch
  --skip-column-names
  "${MYSQL_DATABASE}"
)

sql() {
  "${mysql_cmd[@]}" -e "$1"
}

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
  present="$(sql "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema = '${MYSQL_DATABASE}' AND table_name = '${table}';")"
  assert_eq "1" "${present}" "table ${table} exists"
done

echo "[FURY] seed Director graphs"
sql "INSERT INTO fury_director_graph (graph_key, scope_key, display_name, campaign_node_key, enabled) VALUES ('golden.director', 'golden.scope', 'Golden Director', 'golden.campaign', 1), ('golden.director.other', 'golden.scope', 'Golden Other', 'golden.campaign', 1);"

start_event_hash="UNHEX(SHA2('golden:director:start', 256))"
sql "INSERT INTO fury_event (event_type, actor_kind, actor_guid, account_id, household_id, source_system, dedupe_key, payload) VALUES ('golden.director.start', 1, 1, 1001, 1, 'golden', ${start_event_hash}, JSON_OBJECT());"
start_event_id="$(sql "SELECT id FROM fury_event WHERE dedupe_key=${start_event_hash};")"

echo "[FURY] exclusive active scope"
sql "INSERT IGNORE INTO fury_director_run (household_id, graph_key, scope_key, status, phase_key, started_event_id, last_event_id) VALUES (1, 'golden.director', 'golden.scope', 2, 'rumours', ${start_event_id}, ${start_event_id});"
sql "INSERT IGNORE INTO fury_director_run (household_id, graph_key, scope_key, status, phase_key, started_event_id, last_event_id) VALUES (1, 'golden.director', 'golden.scope', 2, 'rumours', ${start_event_id}, ${start_event_id});"
sql "INSERT IGNORE INTO fury_director_run (household_id, graph_key, scope_key, status, phase_key, started_event_id, last_event_id) VALUES (1, 'golden.director.other', 'golden.scope', 2, 'rumours', ${start_event_id}, ${start_event_id});"
assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_director_run WHERE household_id=1 AND scope_key='golden.scope' AND status IN (1,2,3);")" "duplicate/different graph cannot create second active run in scope"

run_id="$(sql "SELECT id FROM fury_director_run WHERE household_id=1 AND scope_key='golden.scope' AND status IN (1,2,3) LIMIT 1;")"

phase_event_hash="UNHEX(SHA2('golden:director:phase', 256))"
sql "INSERT INTO fury_event (event_type, actor_kind, actor_guid, account_id, household_id, source_system, dedupe_key, payload) VALUES ('golden.director.phase', 5, NULL, NULL, 1, 'golden.system', ${phase_event_hash}, JSON_OBJECT());"
phase_event_id="$(sql "SELECT id FROM fury_event WHERE dedupe_key=${phase_event_hash};")"

echo "[FURY] optimistic phase transition"
sql "UPDATE fury_director_run SET phase_key='invasion', last_event_id=${phase_event_id}, revision=revision+1 WHERE id=${run_id} AND household_id=1 AND revision=0 AND status IN (1,2);"
assert_eq "invasion" "$(sql "SELECT phase_key FROM fury_director_run WHERE id=${run_id};")" "phase update applies"
assert_eq "1" "$(sql "SELECT revision FROM fury_director_run WHERE id=${run_id};")" "phase update advances revision"

sql "UPDATE fury_director_run SET phase_key='bad-stale-write', revision=revision+1 WHERE id=${run_id} AND household_id=1 AND revision=0 AND status IN (1,2);"
assert_eq "invasion" "$(sql "SELECT phase_key FROM fury_director_run WHERE id=${run_id};")" "stale revision cannot overwrite phase"
assert_eq "1" "$(sql "SELECT revision FROM fury_director_run WHERE id=${run_id};")" "stale revision cannot advance revision"

echo "[FURY] external runtime attach is stable"
runtime_event_hash="UNHEX(SHA2('golden:director:runtime', 256))"
sql "INSERT INTO fury_event (event_type, actor_kind, actor_guid, account_id, household_id, source_system, dedupe_key, payload) VALUES ('golden.director.runtime', 5, NULL, NULL, 1, 'golden.system', ${runtime_event_hash}, JSON_OBJECT());"
runtime_event_id="$(sql "SELECT id FROM fury_event WHERE dedupe_key=${runtime_event_hash};")"
sql "UPDATE fury_director_run SET external_runtime_id=7001, last_event_id=${runtime_event_id}, revision=revision+1 WHERE id=${run_id} AND household_id=1 AND revision=1 AND status IN (1,2,3) AND (external_runtime_id IS NULL OR external_runtime_id=7001);"
assert_eq "7001" "$(sql "SELECT external_runtime_id FROM fury_director_run WHERE id=${run_id};")" "runtime id attaches"
assert_eq "2" "$(sql "SELECT revision FROM fury_director_run WHERE id=${run_id};")" "runtime attach advances revision"

sql "UPDATE fury_director_run SET external_runtime_id=9999, revision=revision+1 WHERE id=${run_id} AND household_id=1 AND revision=2 AND status IN (1,2,3) AND (external_runtime_id IS NULL OR external_runtime_id=9999);"
assert_eq "7001" "$(sql "SELECT external_runtime_id FROM fury_director_run WHERE id=${run_id};")" "different runtime cannot replace attached runtime"
assert_eq "2" "$(sql "SELECT revision FROM fury_director_run WHERE id=${run_id};")" "runtime conflict leaves revision unchanged"

echo "[FURY] terminal run frees exclusive scope"
resolve_event_hash="UNHEX(SHA2('golden:director:resolve', 256))"
sql "INSERT INTO fury_event (event_type, actor_kind, actor_guid, account_id, household_id, source_system, dedupe_key, payload) VALUES ('golden.director.resolve', 5, NULL, NULL, 1, 'golden.system', ${resolve_event_hash}, JSON_OBJECT());"
resolve_event_id="$(sql "SELECT id FROM fury_event WHERE dedupe_key=${resolve_event_hash};")"
sql "UPDATE fury_director_run SET status=4, outcome_key='success', resolved_event_id=${resolve_event_id}, last_event_id=${resolve_event_id}, completed_at=CURRENT_TIMESTAMP(6), revision=revision+1 WHERE id=${run_id} AND household_id=1 AND revision=2 AND status IN (1,2,3);"
assert_eq "4" "$(sql "SELECT status FROM fury_director_run WHERE id=${run_id};")" "run resolves"
assert_eq "success" "$(sql "SELECT outcome_key FROM fury_director_run WHERE id=${run_id};")" "outcome persists"

sql "INSERT IGNORE INTO fury_director_run (household_id, graph_key, scope_key, status, phase_key, started_event_id, last_event_id) VALUES (1, 'golden.director.other', 'golden.scope', 2, 'rumours', ${start_event_id}, ${start_event_id});"
assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_director_run WHERE household_id=1 AND scope_key='golden.scope' AND status IN (1,2,3);")" "scope becomes available after terminal state"

echo "[FURY] restart/re-apply preserves Director runtime"
SQL_BASE="${ROOT}/modules/mod-fury/data/sql/fury/base"
while IFS= read -r file; do
  "${mysql_cmd[@]}" < "${file}"
done < <(find "${SQL_BASE}" -maxdepth 1 -type f -name '*.sql' -print | sort)

assert_eq "2" "$(sql "SELECT COUNT(*) FROM fury_director_run WHERE household_id=1 AND scope_key='golden.scope';")" "schema re-apply preserves Director history"
assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_director_run WHERE household_id=1 AND scope_key='golden.scope' AND status IN (1,2,3);")" "active Director run survives schema re-apply"

echo "[FURY] M2 Director schema golden gate passed"
