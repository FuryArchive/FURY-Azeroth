#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

export MYSQL_HOST="${MYSQL_HOST:-127.0.0.1}"
export MYSQL_PORT="${MYSQL_PORT:-3306}"
export MYSQL_USER="${MYSQL_USER:-root}"
export MYSQL_PASSWORD="${MYSQL_PASSWORD:-fury}"
export MYSQL_DATABASE="${MYSQL_DATABASE:-acore_fury}"
export MYSQL_PWD="${MYSQL_PASSWORD}"

bash "${ROOT}/scripts/test-m2-director-policy.sh"
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
  local label="$3"
  if [[ "${actual}" != "${expected}" ]]; then
    echo "[FURY][FAIL] ${label}: expected=${expected} actual=${actual}" >&2
    exit 1
  fi
  echo "[FURY][PASS] ${label}"
}

household="$(sql "SELECT id FROM fury_household WHERE slug='alpha' LIMIT 1;")"

sql "INSERT IGNORE INTO fury_campaign_node
  (node_key,era,ordinal,display_name,required_power_band,grants_power_band,enabled)
  VALUES ('golden.recovery.node',1,950,'Golden Recovery',0,0,1);"

sql "INSERT IGNORE INTO fury_director_graph
  (graph_key,scope_key,display_name,campaign_node_key,enabled)
  VALUES
  ('golden.recovery','golden.recovery.scope','Golden Recovery',
   'golden.recovery.node',1);"

sql "INSERT INTO fury_event
  (event_type,actor_kind,household_id,source_system,dedupe_key,payload)
  VALUES
  ('golden.recovery.start',1,${household},'golden.t33',
   UNHEX(SHA2('t33-start',256)),JSON_OBJECT());"
start_event="$(sql "SELECT id FROM fury_event
  WHERE dedupe_key=UNHEX(SHA2('t33-start',256));")"

sql "INSERT INTO fury_director_run
  (household_id,graph_key,scope_key,status,phase_key,
   external_runtime_id,started_event_id,last_event_id,revision)
  VALUES
  (${household},'golden.recovery','golden.recovery.scope',2,'invasion',
   42,${start_event},${start_event},3);"
run="$(sql "SELECT id FROM fury_director_run
  WHERE household_id=${household}
    AND graph_key='golden.recovery'
  ORDER BY id DESC LIMIT 1;")"

sql "INSERT INTO fury_event
  (event_type,actor_kind,household_id,subject_type,subject_id,
   source_system,correlation_key,dedupe_key,payload)
  VALUES
  ('director.recovery.adopt_replacement',4,${household},
   'director_run',${run},'fury.recovery','golden.recovery',
   UNHEX(SHA2('t33-adopt',256)),JSON_OBJECT());"
recovery_event="$(sql "SELECT id FROM fury_event
  WHERE dedupe_key=UNHEX(SHA2('t33-adopt',256));")"

echo "[FURY] guarded runtime replacement"
sql "UPDATE fury_director_run
  SET external_runtime_id=99,
      last_event_id=${recovery_event},
      revision=revision+1
  WHERE id=${run}
    AND household_id=${household}
    AND revision=3
    AND status IN (1,2,3)
    AND ((external_runtime_id=42) OR
         (external_runtime_id IS NULL AND 42=0));"

assert_eq "99:4" "$(sql "SELECT CONCAT(external_runtime_id,':',revision)
  FROM fury_director_run WHERE id=${run};")"   "replacement runtime binds once"

# Replay of the same recovery mutation cannot advance revision twice.
sql "UPDATE fury_director_run
  SET external_runtime_id=99,
      last_event_id=${recovery_event},
      revision=revision+1
  WHERE id=${run}
    AND household_id=${household}
    AND revision=3
    AND status IN (1,2,3)
    AND ((external_runtime_id=42) OR
         (external_runtime_id IS NULL AND 42=0));"

assert_eq "99:4" "$(sql "SELECT CONCAT(external_runtime_id,':',revision)
  FROM fury_director_run WHERE id=${run};")"   "replayed replacement is revision-idempotent"

# A stale recovery that expects the old runtime cannot overwrite the adopted one.
sql "UPDATE fury_director_run
  SET external_runtime_id=123,
      last_event_id=${recovery_event},
      revision=revision+1
  WHERE id=${run}
    AND household_id=${household}
    AND revision=4
    AND status IN (1,2,3)
    AND ((external_runtime_id=42) OR
         (external_runtime_id IS NULL AND 42=0));"

assert_eq "99:4" "$(sql "SELECT CONCAT(external_runtime_id,':',revision)
  FROM fury_director_run WHERE id=${run};")"   "stale expected runtime cannot steal binding"

SOURCE="${ROOT}/modules/mod-fury/src/content/defias/DefiasRecoveryService.cpp"
ADAPTER="${ROOT}/modules/mod-fury/src/integrations/LivingWorldAdapter.cpp"
DIRECTOR="${ROOT}/modules/mod-fury/src/director/DirectorService.cpp"

grep -Fq 'RestartMissingRuntime' "${SOURCE}"
grep -Fq 'AdoptReplacementRuntime' "${SOURCE}"
grep -Fq 'StartInvasion' "${SOURCE}"
grep -Fq 'RecoverRuntime' "${SOURCE}"
grep -Fq 'HandleOrphanRuntime' "${SOURCE}"
grep -Fq 'AbortRuntime' "${SOURCE}"
grep -Fq '[FURY Recovery]' "${SOURCE}"
grep -Fq 'FailRuntime' "${ADAPTER}"
grep -Fq 'director.runtime.recovered' "${DIRECTOR}"

echo "[FURY][PASS] T33 Director/Living World recovery gate passed"
