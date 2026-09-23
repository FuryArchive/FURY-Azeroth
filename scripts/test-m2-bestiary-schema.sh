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

for table in fury_bestiary_entry fury_bestiary_creature_map fury_bestiary_state; do
  assert_eq "1" "$(sql "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema='${MYSQL_DATABASE}' AND table_name='${table}';")" "table ${table} exists"
done

echo "[FURY] T17 -> T18 migration path"
# T30 adds an event-map FK to fury_bestiary_entry. Remove the newer extension
# before simulating the historical T17 -> T18 migration boundary.
sql "DROP TABLE IF EXISTS fury_bestiary_event_map;
     DROP TABLE fury_bestiary_state;
     DROP TABLE fury_bestiary_creature_map;
     DROP TABLE fury_bestiary_entry;"
"${mysql_cmd[@]}" < "${ROOT}/modules/mod-fury/data/sql/fury/updates/2026_09_22_06_bestiary.sql"

for table in fury_bestiary_entry fury_bestiary_creature_map fury_bestiary_state; do
  assert_eq "1" "$(sql "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema='${MYSQL_DATABASE}' AND table_name='${table}';")" "migration creates ${table}"
done

assert_eq "creature_entry,enabled,entry_key" "$(sql "SELECT GROUP_CONCAT(column_name ORDER BY seq_in_index) FROM information_schema.statistics WHERE table_schema='${MYSQL_DATABASE}' AND table_name='fury_bestiary_creature_map' AND index_name='ix_fury_bestiary_creature_map_lookup';")" "creature mapping index starts with explicit content key"

sql "INSERT INTO fury_bestiary_entry
  (entry_key, display_name, enabled)
  VALUES
  ('golden.wolf','Golden Wolf',1),
  ('golden.disabled','Disabled Beast',0);"

sql "INSERT INTO fury_bestiary_creature_map
  (creature_entry, entry_key, discovery_level, enabled)
  VALUES
  (90001,'golden.wolf',1,1),
  (90002,'golden.wolf',2,1),
  (90003,'golden.disabled',1,1),
  (90004,'golden.wolf',3,0);"

echo "[FURY] explicit mapping filters normal creature events"
assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_bestiary_creature_map m JOIN fury_bestiary_entry e ON e.entry_key=m.entry_key AND e.enabled=1 WHERE m.creature_entry=90001 AND m.enabled=1;")" "enabled mapped creature resolves"
assert_eq "0" "$(sql "SELECT COUNT(*) FROM fury_bestiary_creature_map m JOIN fury_bestiary_entry e ON e.entry_key=m.entry_key AND e.enabled=1 WHERE m.creature_entry=99999 AND m.enabled=1;")" "unmapped creature has no target"
assert_eq "0" "$(sql "SELECT COUNT(*) FROM fury_bestiary_creature_map m JOIN fury_bestiary_entry e ON e.entry_key=m.entry_key AND e.enabled=1 WHERE m.creature_entry=90003 AND m.enabled=1;")" "disabled Bestiary entry does not resolve"
assert_eq "0" "$(sql "SELECT COUNT(*) FROM fury_bestiary_creature_map m JOIN fury_bestiary_entry e ON e.entry_key=m.entry_key AND e.enabled=1 WHERE m.creature_entry=90004 AND m.enabled=1;")" "disabled mapping does not resolve"

make_event() {
  local identity="$1"
  local account="$2"
  local creature="$3"
  sql "INSERT INTO fury_event
    (event_type, actor_kind, account_id, subject_type, subject_id, source_system, dedupe_key, payload)
    VALUES
    ('creature.killed',1,${account},'creature',${creature},'azerothcore',
     UNHEX(SHA2('${identity}',256)),
     JSON_OBJECT('creature_entry',${creature}));"
  sql "SELECT id FROM fury_event WHERE dedupe_key=UNHEX(SHA2('${identity}',256));"
}

apply_kill() {
  local account="$1"
  local level="$2"
  local event_id="$3"
  sql "INSERT INTO fury_bestiary_state
    (account_id,entry_key,discovery_level,kill_count,first_event_id,last_event_id,revision)
    VALUES (${account},'golden.wolf',${level},1,${event_id},${event_id},0)
    ON DUPLICATE KEY UPDATE
      discovery_level=IF(last_event_id<VALUES(last_event_id),GREATEST(discovery_level,VALUES(discovery_level)),discovery_level),
      kill_count=IF(last_event_id<VALUES(last_event_id),kill_count+1,kill_count),
      revision=IF(last_event_id<VALUES(last_event_id),revision+1,revision),
      last_event_id=GREATEST(last_event_id,VALUES(last_event_id));"
}

apply_level() {
  local account="$1"
  local level="$2"
  local event_id="$3"
  sql "INSERT INTO fury_bestiary_state
    (account_id,entry_key,discovery_level,kill_count,first_event_id,last_event_id,revision)
    VALUES (${account},'golden.wolf',${level},0,${event_id},${event_id},0)
    ON DUPLICATE KEY UPDATE
      discovery_level=IF(last_event_id<VALUES(last_event_id),GREATEST(discovery_level,VALUES(discovery_level)),discovery_level),
      revision=IF(last_event_id<VALUES(last_event_id),revision+1,revision),
      last_event_id=GREATEST(last_event_id,VALUES(last_event_id));"
}

echo "[FURY] first mapped kill creates account-level Encountered state"
kill1="$(make_event golden-bestiary-kill-1 1001 90001)"
apply_kill 1001 1 "${kill1}"
assert_eq "1" "$(sql "SELECT discovery_level FROM fury_bestiary_state WHERE account_id=1001 AND entry_key='golden.wolf';")" "first mapped kill reaches Encountered"
assert_eq "1" "$(sql "SELECT kill_count FROM fury_bestiary_state WHERE account_id=1001 AND entry_key='golden.wolf';")" "first mapped kill increments count"
assert_eq "${kill1}" "$(sql "SELECT first_event_id FROM fury_bestiary_state WHERE account_id=1001 AND entry_key='golden.wolf';")" "first event is retained"

echo "[FURY] replay is idempotent"
apply_kill 1001 1 "${kill1}"
assert_eq "1" "$(sql "SELECT kill_count FROM fury_bestiary_state WHERE account_id=1001 AND entry_key='golden.wolf';")" "replayed kill does not double-count"

echo "[FURY] repeated Encountered kills do not invent higher discovery levels"
for n in 2 3 4 5; do
  eid="$(make_event golden-bestiary-kill-${n} 1001 90001)"
  apply_kill 1001 1 "${eid}"
done
assert_eq "5" "$(sql "SELECT kill_count FROM fury_bestiary_state WHERE account_id=1001 AND entry_key='golden.wolf';")" "distinct mapped kills accumulate"
assert_eq "1" "$(sql "SELECT discovery_level FROM fury_bestiary_state WHERE account_id=1001 AND entry_key='golden.wolf';")" "kill count alone does not auto-promote"

echo "[FURY] explicit content mapping can promote to Studied"
studied_event="$(make_event golden-bestiary-studied 1001 90002)"
apply_kill 1001 2 "${studied_event}"
assert_eq "2" "$(sql "SELECT discovery_level FROM fury_bestiary_state WHERE account_id=1001 AND entry_key='golden.wolf';")" "explicit mapping reaches Studied"
assert_eq "6" "$(sql "SELECT kill_count FROM fury_bestiary_state WHERE account_id=1001 AND entry_key='golden.wolf';")" "Studied mapping kill still counts"

echo "[FURY] explicit non-kill promotion reaches Mastered without inflating kills"
sql "INSERT INTO fury_event
  (event_type, actor_kind, account_id, subject_type, source_system, correlation_key, dedupe_key, payload)
  VALUES
  ('golden.bestiary.mastered',1,1001,'bestiary_entry','golden.bestiary','golden.wolf',
   UNHEX(SHA2('golden-bestiary-mastered',256)),JSON_OBJECT());"
master_event="$(sql "SELECT id FROM fury_event WHERE dedupe_key=UNHEX(SHA2('golden-bestiary-mastered',256));")"
apply_level 1001 3 "${master_event}"
assert_eq "3" "$(sql "SELECT discovery_level FROM fury_bestiary_state WHERE account_id=1001 AND entry_key='golden.wolf';")" "explicit promotion reaches Mastered"
assert_eq "6" "$(sql "SELECT kill_count FROM fury_bestiary_state WHERE account_id=1001 AND entry_key='golden.wolf';")" "non-kill promotion leaves kill count unchanged"

echo "[FURY] later lower mapping cannot downgrade Mastered"
later_kill="$(make_event golden-bestiary-after-mastered 1001 90001)"
apply_kill 1001 1 "${later_kill}"
assert_eq "3" "$(sql "SELECT discovery_level FROM fury_bestiary_state WHERE account_id=1001 AND entry_key='golden.wolf';")" "lower mapping cannot downgrade"
assert_eq "7" "$(sql "SELECT kill_count FROM fury_bestiary_state WHERE account_id=1001 AND entry_key='golden.wolf';")" "later kill still contributes to kill count"

echo "[FURY] projection is account-level, not household-global"
other_event="$(make_event golden-bestiary-other-account 2002 90001)"
apply_kill 2002 1 "${other_event}"
assert_eq "1" "$(sql "SELECT discovery_level FROM fury_bestiary_state WHERE account_id=2002 AND entry_key='golden.wolf';")" "second account gets independent state"
assert_eq "1" "$(sql "SELECT kill_count FROM fury_bestiary_state WHERE account_id=2002 AND entry_key='golden.wolf';")" "second account gets independent kill count"
assert_eq "7" "$(sql "SELECT kill_count FROM fury_bestiary_state WHERE account_id=1001 AND entry_key='golden.wolf';")" "first account state is unchanged"

echo "[FURY] stable advance-event identity is unique"
advance_identity="bestiary:advance:v1:1001:golden.wolf:3"
sql "INSERT IGNORE INTO fury_event
  (event_type, actor_kind, account_id, subject_type, source_system, correlation_key, dedupe_key, payload)
  VALUES
  ('bestiary.entry.advanced',1,1001,'bestiary_entry','fury.bestiary','golden.wolf',
   UNHEX(SHA2('${advance_identity}',256)),JSON_OBJECT('level',3));"
sql "INSERT IGNORE INTO fury_event
  (event_type, actor_kind, account_id, subject_type, source_system, correlation_key, dedupe_key, payload)
  VALUES
  ('bestiary.entry.advanced',1,1001,'bestiary_entry','fury.bestiary','golden.wolf',
   UNHEX(SHA2('${advance_identity}',256)),JSON_OBJECT('level',3));"
assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_event WHERE dedupe_key=UNHEX(SHA2('${advance_identity}',256));")" "advance event dedupe is stable"

echo "[FURY] invalid mapping level is rejected"
if sql "INSERT INTO fury_bestiary_creature_map
  (creature_entry,entry_key,discovery_level,enabled)
  VALUES (90005,'golden.wolf',9,1);" >/dev/null 2>&1; then
  echo "[FURY][FAIL] invalid discovery level was accepted" >&2
  exit 1
fi
echo "[FURY][PASS] invalid discovery level rejected"

echo "[FURY] schema re-apply preserves projection"
"${mysql_cmd[@]}" < "${ROOT}/modules/mod-fury/data/sql/fury/base/90_bestiary.sql"
"${mysql_cmd[@]}" < "${ROOT}/modules/mod-fury/data/sql/fury/updates/2026_09_22_06_bestiary.sql"
assert_eq "3" "$(sql "SELECT discovery_level FROM fury_bestiary_state WHERE account_id=1001 AND entry_key='golden.wolf';")" "Mastered survives schema re-apply"
assert_eq "7" "$(sql "SELECT kill_count FROM fury_bestiary_state WHERE account_id=1001 AND entry_key='golden.wolf';")" "kill count survives schema re-apply"

echo "[FURY][PASS] T18 Bestiary schema/replay golden gate passed"
