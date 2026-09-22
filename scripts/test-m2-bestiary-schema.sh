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

bash "${ROOT}/scripts/test-m2-professions-schema.sh"

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

for table in fury_bestiary_entry fury_bestiary_creature_map fury_bestiary_state; do
  present="$(sql "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema='${MYSQL_DATABASE}' AND table_name='${table}';")"
  assert_eq "1" "${present}" "table ${table} exists"
done

echo "[FURY] seed explicit Bestiary mappings"
sql "INSERT INTO fury_bestiary_entry (entry_key, display_name, enabled) VALUES ('golden.wolf', 'Golden Wolf', 1);"
sql "INSERT INTO fury_bestiary_creature_map (creature_entry, entry_key, discovery_level, enabled) VALUES (90001, 'golden.wolf', 1, 1), (90002, 'golden.wolf', 2, 1);"

assert_eq "0" "$(sql "SELECT COUNT(*) FROM fury_bestiary_creature_map WHERE creature_entry=99999 AND enabled=1;")" "unmapped creature has no Bestiary target"

kill1_hash="UNHEX(SHA2('golden:bestiary:kill:1',256))"
sql "INSERT INTO fury_event (event_type, actor_kind, actor_guid, account_id, household_id, subject_type, subject_id, source_system, dedupe_key, payload) VALUES ('creature.killed',1,1,1001,1,'creature',90001,'azerothcore',${kill1_hash},JSON_OBJECT());"
kill1_id="$(sql "SELECT id FROM fury_event WHERE dedupe_key=${kill1_hash};")"

apply_kill_1="INSERT INTO fury_bestiary_state (account_id,entry_key,discovery_level,kill_count,first_event_id,last_event_id,revision) VALUES (1001,'golden.wolf',1,1,${kill1_id},${kill1_id},0) ON DUPLICATE KEY UPDATE discovery_level=IF(last_event_id<VALUES(last_event_id),GREATEST(discovery_level,VALUES(discovery_level)),discovery_level), kill_count=IF(last_event_id<VALUES(last_event_id),kill_count+1,kill_count), revision=IF(last_event_id<VALUES(last_event_id),revision+1,revision), last_event_id=GREATEST(last_event_id,VALUES(last_event_id));"

sql "${apply_kill_1}"
assert_eq "1" "$(sql "SELECT discovery_level FROM fury_bestiary_state WHERE account_id=1001 AND entry_key='golden.wolf';")" "first mapped kill reaches Encountered"
assert_eq "1" "$(sql "SELECT kill_count FROM fury_bestiary_state WHERE account_id=1001 AND entry_key='golden.wolf';")" "first mapped kill increments count"

sql "${apply_kill_1}"
assert_eq "1" "$(sql "SELECT kill_count FROM fury_bestiary_state WHERE account_id=1001 AND entry_key='golden.wolf';")" "replayed kill does not double-count"

kill2_hash="UNHEX(SHA2('golden:bestiary:kill:2',256))"
sql "INSERT INTO fury_event (event_type, actor_kind, actor_guid, account_id, household_id, subject_type, subject_id, source_system, dedupe_key, payload) VALUES ('creature.killed',1,1,1001,1,'creature',90001,'azerothcore',${kill2_hash},JSON_OBJECT());"
kill2_id="$(sql "SELECT id FROM fury_event WHERE dedupe_key=${kill2_hash};")"
sql "INSERT INTO fury_bestiary_state (account_id,entry_key,discovery_level,kill_count,first_event_id,last_event_id,revision) VALUES (1001,'golden.wolf',1,1,${kill2_id},${kill2_id},0) ON DUPLICATE KEY UPDATE discovery_level=IF(last_event_id<VALUES(last_event_id),GREATEST(discovery_level,VALUES(discovery_level)),discovery_level), kill_count=IF(last_event_id<VALUES(last_event_id),kill_count+1,kill_count), revision=IF(last_event_id<VALUES(last_event_id),revision+1,revision), last_event_id=GREATEST(last_event_id,VALUES(last_event_id));"
assert_eq "2" "$(sql "SELECT kill_count FROM fury_bestiary_state WHERE account_id=1001 AND entry_key='golden.wolf';")" "distinct mapped kill increments count"

variant_hash="UNHEX(SHA2('golden:bestiary:variant',256))"
sql "INSERT INTO fury_event (event_type, actor_kind, actor_guid, account_id, household_id, subject_type, subject_id, source_system, dedupe_key, payload) VALUES ('creature.killed',1,1,1001,1,'creature',90002,'azerothcore',${variant_hash},JSON_OBJECT());"
variant_id="$(sql "SELECT id FROM fury_event WHERE dedupe_key=${variant_hash};")"
sql "INSERT INTO fury_bestiary_state (account_id,entry_key,discovery_level,kill_count,first_event_id,last_event_id,revision) VALUES (1001,'golden.wolf',2,1,${variant_id},${variant_id},0) ON DUPLICATE KEY UPDATE discovery_level=IF(last_event_id<VALUES(last_event_id),GREATEST(discovery_level,VALUES(discovery_level)),discovery_level), kill_count=IF(last_event_id<VALUES(last_event_id),kill_count+1,kill_count), revision=IF(last_event_id<VALUES(last_event_id),revision+1,revision), last_event_id=GREATEST(last_event_id,VALUES(last_event_id));"
assert_eq "2" "$(sql "SELECT discovery_level FROM fury_bestiary_state WHERE account_id=1001 AND entry_key='golden.wolf';")" "explicit variant mapping reaches Studied"
assert_eq "3" "$(sql "SELECT kill_count FROM fury_bestiary_state WHERE account_id=1001 AND entry_key='golden.wolf';")" "variant kill still counts as encounter"

master_hash="UNHEX(SHA2('golden:bestiary:mastered',256))"
sql "INSERT INTO fury_event (event_type, actor_kind, actor_guid, account_id, household_id, subject_type, source_system, correlation_key, dedupe_key, payload) VALUES ('golden.bestiary.mastered',1,1,1001,1,'bestiary_entry','golden','golden.wolf',${master_hash},JSON_OBJECT());"
master_id="$(sql "SELECT id FROM fury_event WHERE dedupe_key=${master_hash};")"
sql "INSERT INTO fury_bestiary_state (account_id,entry_key,discovery_level,kill_count,first_event_id,last_event_id,revision) VALUES (1001,'golden.wolf',3,0,${master_id},${master_id},0) ON DUPLICATE KEY UPDATE discovery_level=IF(last_event_id<VALUES(last_event_id),GREATEST(discovery_level,VALUES(discovery_level)),discovery_level), revision=IF(last_event_id<VALUES(last_event_id),revision+1,revision), last_event_id=GREATEST(last_event_id,VALUES(last_event_id));"
assert_eq "3" "$(sql "SELECT discovery_level FROM fury_bestiary_state WHERE account_id=1001 AND entry_key='golden.wolf';")" "explicit content event can promote to Mastered"
assert_eq "3" "$(sql "SELECT kill_count FROM fury_bestiary_state WHERE account_id=1001 AND entry_key='golden.wolf';")" "non-kill promotion does not inflate kill count"

echo "[FURY] invalid mapping level is rejected"
if sql "INSERT INTO fury_bestiary_creature_map (creature_entry,entry_key,discovery_level,enabled) VALUES (90003,'golden.wolf',9,1);" >/dev/null 2>&1; then
  echo "[FURY][FAIL] invalid discovery level was accepted" >&2
  exit 1
fi

echo "[FURY] schema re-apply preserves Bestiary state"
SQL_BASE="${ROOT}/modules/mod-fury/data/sql/fury/base"
while IFS= read -r file; do
  "${mysql_cmd[@]}" < "${file}"
done < <(find "${SQL_BASE}" -maxdepth 1 -type f -name '*.sql' -print | sort)

assert_eq "3" "$(sql "SELECT discovery_level FROM fury_bestiary_state WHERE account_id=1001 AND entry_key='golden.wolf';")" "schema re-apply preserves discovery level"
assert_eq "3" "$(sql "SELECT kill_count FROM fury_bestiary_state WHERE account_id=1001 AND entry_key='golden.wolf';")" "schema re-apply preserves kill count"

echo "[FURY] M2 Bestiary schema golden gate passed"
