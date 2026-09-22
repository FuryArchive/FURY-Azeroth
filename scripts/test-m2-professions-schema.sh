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

for table in fury_profession_order fury_profession_order_option fury_profession_order_instance; do
  assert_eq "1" "$(sql "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema='${MYSQL_DATABASE}' AND table_name='${table}';")" "table ${table} exists"
done

echo "[FURY] T16 -> T17 migration path"
sql "DROP TABLE fury_profession_order_instance;
     DROP TABLE fury_profession_order_option;
     DROP TABLE fury_profession_order;"
"${mysql_cmd[@]}" < "${ROOT}/modules/mod-fury/data/sql/fury/updates/2026_09_22_05_profession_orders.sql"

for table in fury_profession_order fury_profession_order_option fury_profession_order_instance; do
  assert_eq "1" "$(sql "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema='${MYSQL_DATABASE}' AND table_name='${table}';")" "migration creates ${table}"
done

assert_eq "skill_id,item_id,order_key,ordinal" "$(sql "SELECT GROUP_CONCAT(column_name ORDER BY seq_in_index) FROM information_schema.statistics WHERE table_schema='${MYSQL_DATABASE}' AND table_name='fury_profession_order_option' AND index_name='ix_fury_profession_order_option_target';")" "target index is exact profession/item first"

household_id="$(sql "SELECT id FROM fury_household WHERE slug='alpha' LIMIT 1;")"
sql "INSERT INTO fury_event
  (event_type, actor_kind, household_id, source_system, dedupe_key, payload)
  VALUES
  ('golden.profession.accept',1,${household_id},'golden.professions',
   UNHEX(SHA2('golden-profession-accept',256)),JSON_OBJECT());"
accept_event_id="$(sql "SELECT id FROM fury_event WHERE dedupe_key=UNHEX(SHA2('golden-profession-accept',256));")"

sql "INSERT INTO fury_profession_order
  (order_key, title, repeat_policy, enabled)
  VALUES ('golden.profession.order','Golden Supplies',2,1);"
sql "INSERT INTO fury_profession_order_option
  (order_key, ordinal, skill_id, item_id, required_count)
  VALUES ('golden.profession.order',1,164,2840,2);"

echo "[FURY] duplicate start yields one active instance"
sql "INSERT IGNORE INTO fury_profession_order_instance
  (household_id, order_key, option_ordinal, status, accepted_event_id, last_event_id)
  VALUES (${household_id},'golden.profession.order',1,2,${accept_event_id},${accept_event_id});"
sql "INSERT IGNORE INTO fury_profession_order_instance
  (household_id, order_key, option_ordinal, status, accepted_event_id, last_event_id)
  VALUES (${household_id},'golden.profession.order',1,2,${accept_event_id},${accept_event_id});"
assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_profession_order_instance WHERE household_id=${household_id} AND order_key='golden.profession.order' AND status=2;")" "only one active order exists"

instance_id="$(sql "SELECT id FROM fury_profession_order_instance WHERE household_id=${household_id} AND order_key='golden.profession.order' AND status=2 LIMIT 1;")"

echo "[FURY] exact profession + item matching"
match_query() {
  local skill="$1"
  local item="$2"
  sql "SELECT COUNT(*)
    FROM fury_profession_order_option o FORCE INDEX (ix_fury_profession_order_option_target)
    JOIN fury_profession_order_instance i
      ON i.order_key=o.order_key AND i.option_ordinal=o.ordinal
      AND i.household_id=${household_id} AND i.status=2
    WHERE o.skill_id=${skill} AND o.item_id=${item};"
}

assert_eq "1" "$(match_query 164 2840)" "matching profession/item finds active order"
assert_eq "0" "$(match_query 165 2840)" "wrong profession does not match"
assert_eq "0" "$(match_query 164 2841)" "wrong item does not match"

make_craft_event() {
  local identity="$1"
  local skill="$2"
  local item="$3"
  local target=$(( (skill << 32) | item ))
  sql "INSERT INTO fury_event
    (event_type, actor_kind, household_id, subject_type, subject_id, source_system, dedupe_key, payload)
    VALUES
    ('profession.crafted',1,${household_id},'profession_craft',${target},'azerothcore',
     UNHEX(SHA2('${identity}',256)),
     JSON_OBJECT('skill_id',${skill},'item_id',${item}));"
  sql "SELECT id FROM fury_event WHERE dedupe_key=UNHEX(SHA2('${identity}',256));"
}

advance() {
  local event_id="$1"
  sql "UPDATE fury_profession_order_instance i
    JOIN fury_profession_order_option o
      ON o.order_key=i.order_key AND o.ordinal=i.option_ordinal
    SET i.progress_count=LEAST(o.required_count,i.progress_count+1),
        i.last_event_id=${event_id},
        i.revision=i.revision+1
    WHERE i.id=${instance_id}
      AND i.household_id=${household_id}
      AND i.status=2
      AND i.last_event_id<${event_id};"
}

echo "[FURY] wrong craft events do not progress"
wrong_profession_event="$(make_craft_event golden-profession-wrong-skill 165 2840)"
wrong_item_event="$(make_craft_event golden-profession-wrong-item 164 2841)"
assert_eq "0" "$(match_query 165 2840)" "wrong profession remains unmatched after durable event"
assert_eq "0" "$(match_query 164 2841)" "wrong item remains unmatched after durable event"
assert_eq "0" "$(sql "SELECT progress_count FROM fury_profession_order_instance WHERE id=${instance_id};")" "wrong craft events leave progress unchanged"

echo "[FURY] matching craft replay increments exactly once"
craft1_id="$(make_craft_event golden-profession-craft-1 164 2840)"
advance "${craft1_id}"
assert_eq "1" "$(sql "SELECT progress_count FROM fury_profession_order_instance WHERE id=${instance_id};")" "first matching craft increments"
advance "${craft1_id}"
assert_eq "1" "$(sql "SELECT progress_count FROM fury_profession_order_instance WHERE id=${instance_id};")" "replayed craft does not double-count"

craft2_id="$(make_craft_event golden-profession-craft-2 164 2840)"
advance "${craft2_id}"
assert_eq "2" "$(sql "SELECT progress_count FROM fury_profession_order_instance WHERE id=${instance_id};")" "second matching craft reaches required count"
assert_eq "${craft2_id}" "$(sql "SELECT last_event_id FROM fury_profession_order_instance WHERE id=${instance_id};")" "last progress event is retained"

echo "[FURY] crash recovery completion is exactly once"
# Simulate replay after crash: progress was already persisted, completion was not.
advance "${craft2_id}"
assert_eq "2" "$(sql "SELECT progress_count FROM fury_profession_order_instance WHERE id=${instance_id};")" "recovery replay still does not double-count"

completion_identity="profession-order:completed:v1:${instance_id}"
sql "INSERT IGNORE INTO fury_event
  (event_type, actor_kind, household_id, subject_type, subject_id, source_system, correlation_key, dedupe_key, payload)
  VALUES
  ('profession.order.completed',1,${household_id},'profession_order',${instance_id},
   'fury.professions','golden.profession.order',
   UNHEX(SHA2('${completion_identity}',256)),
   JSON_OBJECT('instance_id',${instance_id},'source_event_id',${craft2_id}));"
sql "INSERT IGNORE INTO fury_event
  (event_type, actor_kind, household_id, subject_type, subject_id, source_system, correlation_key, dedupe_key, payload)
  VALUES
  ('profession.order.completed',1,${household_id},'profession_order',${instance_id},
   'fury.professions','golden.profession.order',
   UNHEX(SHA2('${completion_identity}',256)),
   JSON_OBJECT('instance_id',${instance_id},'source_event_id',${craft2_id}));"

assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_event WHERE dedupe_key=UNHEX(SHA2('${completion_identity}',256));")" "completion event is unique"
completion_event_id="$(sql "SELECT id FROM fury_event WHERE dedupe_key=UNHEX(SHA2('${completion_identity}',256));")"

sql "UPDATE fury_profession_order_instance
  SET status=3, completed_event_id=${completion_event_id},
      completed_at=COALESCE(completed_at,CURRENT_TIMESTAMP(6)),
      revision=revision+1
  WHERE id=${instance_id} AND household_id=${household_id} AND status=2;"

assert_eq "3" "$(sql "SELECT status FROM fury_profession_order_instance WHERE id=${instance_id};")" "order completes after recovery"
assert_eq "${completion_event_id}" "$(sql "SELECT completed_event_id FROM fury_profession_order_instance WHERE id=${instance_id};")" "completion event is retained"
assert_eq "${craft2_id}" "$(sql "SELECT last_event_id FROM fury_profession_order_instance WHERE id=${instance_id};")" "completion does not overwrite last craft event"

echo "[FURY] repeatable order can start again"
sql "INSERT IGNORE INTO fury_profession_order_instance
  (household_id, order_key, option_ordinal, status, accepted_event_id, last_event_id)
  VALUES (${household_id},'golden.profession.order',1,2,${craft2_id},${craft2_id});"
assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_profession_order_instance WHERE household_id=${household_id} AND order_key='golden.profession.order' AND status=2;")" "repeatable order reopens"

echo "[FURY] schema re-apply preserves history"
"${mysql_cmd[@]}" < "${ROOT}/modules/mod-fury/data/sql/fury/base/80_profession_orders.sql"
"${mysql_cmd[@]}" < "${ROOT}/modules/mod-fury/data/sql/fury/updates/2026_09_22_05_profession_orders.sql"
assert_eq "2" "$(sql "SELECT COUNT(*) FROM fury_profession_order_instance WHERE household_id=${household_id} AND order_key='golden.profession.order';")" "order history survives schema re-apply"

echo "[FURY][PASS] T17 Profession Orders schema/replay golden gate passed"
