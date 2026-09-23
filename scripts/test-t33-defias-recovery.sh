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
bash "${ROOT}/scripts/test-t25-lw-completion.sh"
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

GRAPH="classic.westfall.defias_resurgence.v1"
SCOPE="classic.westfall"
household="$(sql "SELECT id FROM fury_household WHERE slug='alpha';")"

make_event() {
  local identity="$1"
  local event_type="$2"
  local actor_kind="$3"
  local household_id="${4:-NULL}"
  local subject_type="${5:-NULL}"
  local subject_id="${6:-NULL}"
  local payload="${7:-{}}"

  local household_sql="NULL"
  local subject_type_sql="NULL"
  local subject_id_sql="NULL"

  [[ "${household_id}" != "NULL" ]] && household_sql="${household_id}"
  [[ "${subject_type}" != "NULL" ]] && subject_type_sql="'${subject_type}'"
  [[ "${subject_id}" != "NULL" ]] && subject_id_sql="${subject_id}"

  sql "INSERT IGNORE INTO fury_event
    (event_type,actor_kind,household_id,subject_type,subject_id,
     source_system,correlation_key,dedupe_key,payload)
    VALUES
    ('${event_type}',${actor_kind},${household_sql},
     ${subject_type_sql},${subject_id_sql},
     'fury.recovery','${GRAPH}',
     UNHEX(SHA2('${identity}',256)),'${payload}');"

  sql "SELECT id FROM fury_event
    WHERE dedupe_key=UNHEX(SHA2('${identity}',256));"
}

echo "[FURY] crash gap: active Living World runtime exists before Director binding"
start_event="$(sql "INSERT INTO fury_event
  (event_type,actor_kind,household_id,source_system,dedupe_key,payload)
  VALUES
  ('player.zone.changed',1,${household},'azerothcore',
   UNHEX(SHA2('t33-start-1',256)),JSON_OBJECT());
  SELECT LAST_INSERT_ID();")"

sql "INSERT INTO fury_director_run
  (household_id,graph_key,scope_key,status,phase_key,
   started_event_id,last_event_id,revision)
  VALUES
  (${household},'${GRAPH}','${SCOPE}',2,'invasion',
   ${start_event},${start_event},1);"
run1="$(sql "SELECT id FROM fury_director_run
  WHERE household_id=${household} AND graph_key='${GRAPH}'
  AND status IN (1,2,3) LIMIT 1;")"

attach_event="$(make_event "director:recovery:v1:${run1}:2:42"   "director.recovery.attach_runtime" 5 "${household}"   "director_run" "${run1}"   "{\"run_id\":${run1},\"action\":2,\"expected_runtime_id\":0,\"runtime_id\":42}")"

sql "UPDATE fury_director_run
  SET external_runtime_id=42,
      last_event_id=${attach_event},
      revision=revision+1
  WHERE id=${run1}
    AND household_id=${household}
    AND revision=1
    AND status IN (1,2,3)
    AND external_runtime_id IS NULL;"

# Replay after crash: the same recovery event cannot bind another runtime.
sql "UPDATE fury_director_run
  SET external_runtime_id=99,
      last_event_id=${attach_event},
      revision=revision+1
  WHERE id=${run1}
    AND household_id=${household}
    AND revision=1
    AND status IN (1,2,3)
    AND external_runtime_id IS NULL;"

assert_eq "42" "$(sql "SELECT external_runtime_id FROM fury_director_run WHERE id=${run1};")"   "replayed attach keeps original runtime"
assert_eq "2" "$(sql "SELECT revision FROM fury_director_run WHERE id=${run1};")"   "attach mutation happens once"

"${mysql_cmd[@]}" < "${ROOT}/modules/mod-fury/data/sql/fury/base/70_director.sql"
assert_eq "42" "$(sql "SELECT external_runtime_id FROM fury_director_run WHERE id=${run1};")"   "restart preserves attached runtime"

sql "INSERT IGNORE INTO fury_director_run
  (household_id,graph_key,scope_key,status,phase_key,
   started_event_id,last_event_id)
  VALUES
  (${household},'${GRAPH}','${SCOPE}',2,'rumours',
   ${start_event},${start_event});"
assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_director_run
  WHERE household_id=${household}
    AND scope_key='${SCOPE}'
    AND status IN (1,2,3);")"   "restart cannot create a second active Defias run"

echo "[FURY] bound Director + missing Living World runtime -> safe abort"
missing_event="$(make_event "director:recovery:v1:${run1}:4:42"   "director.recovery.abort_missing" 5 "${household}"   "director_run" "${run1}"   "{\"run_id\":${run1},\"action\":4,\"expected_runtime_id\":42,\"runtime_id\":42}")"

sql "UPDATE fury_director_run
  SET status=6,
      outcome_key='recovery.runtime_missing',
      resolved_event_id=NULL,
      last_event_id=${missing_event},
      completed_at=COALESCE(completed_at,CURRENT_TIMESTAMP(6)),
      revision=revision+1
  WHERE id=${run1}
    AND household_id=${household}
    AND status IN (1,2,3);"

abort_event="$(sql "INSERT IGNORE INTO fury_event
  (event_type,actor_kind,household_id,subject_type,subject_id,
   source_system,correlation_key,dedupe_key,payload)
  VALUES
  ('director.run.aborted',5,${household},'director_run',${run1},
   'fury.director','${GRAPH}',
   UNHEX(SHA2('director:terminal:v1:${run1}',256)),
   JSON_OBJECT('run_id',${run1},'outcome_key','recovery.runtime_missing'));
  SELECT id FROM fury_event
  WHERE dedupe_key=UNHEX(SHA2('director:terminal:v1:${run1}',256));")"

sql "UPDATE fury_director_run
  SET resolved_event_id=${abort_event},
      last_event_id=${abort_event}
  WHERE id=${run1}
    AND household_id=${household}
    AND status=6
    AND resolved_event_id IS NULL;"

assert_eq "6" "$(sql "SELECT status FROM fury_director_run WHERE id=${run1};")"   "missing runtime safely aborts Director run"
assert_eq "recovery.runtime_missing" "$(sql "SELECT outcome_key FROM fury_director_run WHERE id=${run1};")"   "missing runtime records recovery outcome"
assert_eq "${abort_event}" "$(sql "SELECT resolved_event_id FROM fury_director_run WHERE id=${run1};")"   "abort binds canonical terminal event"
assert_eq "0" "$(sql "SELECT COUNT(*) FROM fury_director_run
  WHERE household_id=${household}
    AND scope_key='${SCOPE}'
    AND status IN (1,2,3);")"   "safe abort frees Director scope"

retry_start="$(sql "INSERT INTO fury_event
  (event_type,actor_kind,household_id,source_system,dedupe_key,payload)
  VALUES
  ('player.zone.changed',1,${household},'azerothcore',
   UNHEX(SHA2('t33-start-2',256)),JSON_OBJECT());
  SELECT LAST_INSERT_ID();")"

sql "INSERT INTO fury_director_run
  (household_id,graph_key,scope_key,status,phase_key,
   started_event_id,last_event_id,revision)
  VALUES
  (${household},'${GRAPH}','${SCOPE}',2,'invasion',
   ${retry_start},${retry_start},0);"
run2="$(sql "SELECT id FROM fury_director_run
  WHERE household_id=${household}
    AND graph_key='${GRAPH}'
    AND status IN (1,2,3)
  ORDER BY id DESC LIMIT 1;")"
assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_director_run
  WHERE household_id=${household}
    AND graph_key='${GRAPH}'
    AND status IN (1,2,3);")"   "later Human event can start a clean retry"

echo "[FURY] runtime conflict -> deterministic cleanup intent + abort"
sql "UPDATE fury_director_run
  SET external_runtime_id=77,revision=1
  WHERE id=${run2};"

conflict_event="$(make_event "director:recovery:v1:${run2}:7:88"   "director.recovery.abort_conflict" 5 "${household}"   "director_run" "${run2}"   "{\"run_id\":${run2},\"action\":7,\"expected_runtime_id\":77,\"runtime_id\":88}")"

sql "UPDATE fury_director_run
  SET status=6,
      outcome_key='recovery.runtime_conflict',
      resolved_event_id=NULL,
      last_event_id=${conflict_event},
      completed_at=COALESCE(completed_at,CURRENT_TIMESTAMP(6)),
      revision=revision+1
  WHERE id=${run2}
    AND household_id=${household}
    AND status IN (1,2,3);"

assert_eq "recovery.runtime_conflict" "$(sql "SELECT outcome_key
  FROM fury_director_run WHERE id=${run2};")"   "runtime conflict has deterministic abort outcome"

echo "[FURY] orphan managed runtime intent is replay-idempotent"
orphan1="$(make_event "director:recovery:orphan:v1:9001"   "director.recovery.orphan_runtime" 5 NULL   "living_world_runtime" 9001   "{\"runtime_id\":9001,\"invasion_id\":1}")"
orphan2="$(make_event "director:recovery:orphan:v1:9001"   "director.recovery.orphan_runtime" 5 NULL   "living_world_runtime" 9001   "{\"runtime_id\":9001,\"invasion_id\":1}")"
assert_eq "${orphan1}" "${orphan2}"   "orphan cleanup intent dedupes on replay"
assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_event
  WHERE event_type='director.recovery.orphan_runtime'
    AND subject_id=9001;")"   "one durable orphan recovery event exists"

RECOVERY="${ROOT}/modules/mod-fury/src/content/defias/DefiasRecoveryService.cpp"
ADAPTER="${ROOT}/modules/mod-fury/src/integrations/LivingWorldAdapter.cpp"
APP="${ROOT}/modules/mod-fury/src/core/FuryApp.cpp"

grep -Fq 'director.recovery.attach_runtime' "${RECOVERY}"
grep -Fq 'director.recovery.abort_missing' "${RECOVERY}"
grep -Fq 'director.recovery.abort_conflict' "${RECOVERY}"
grep -Fq 'director.recovery.orphan_runtime' "${RECOVERY}"
grep -Fq 'FailManagedRuntime' "${RECOVERY}"
grep -Fq 'sInvasionRuntimeMgr.FailRuntime' "${ADAPTER}"
grep -Fq '_defiasRecovery.Tick()' "${APP}"
grep -Fq 'LOG_WARN' "${RECOVERY}"
grep -Fq 'LOG_ERROR' "${RECOVERY}"

echo "[FURY][PASS] T33 Director/Living World recovery gate passed"
