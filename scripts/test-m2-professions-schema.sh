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

bash "${ROOT}/scripts/test-m2-director-schema.sh"

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

for table in fury_profession_order fury_profession_order_option fury_profession_order_instance; do
  present="$(sql "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema = '${MYSQL_DATABASE}' AND table_name = '${table}';")"
  assert_eq "1" "${present}" "table ${table} exists"
done

echo "[FURY] seed profession order"
sql "INSERT INTO fury_profession_order (order_key, title, repeat_policy, enabled) VALUES ('golden.profession.order', 'Golden Supplies', 2, 1);"
sql "INSERT INTO fury_profession_order_option (order_key, ordinal, skill_id, item_id, required_count) VALUES ('golden.profession.order', 1, 164, 2840, 2);"

accept_hash="UNHEX(SHA2('golden:profession:accept', 256))"
sql "INSERT INTO fury_event (event_type, actor_kind, actor_guid, account_id, household_id, source_system, dedupe_key, payload) VALUES ('golden.profession.accept', 1, 1, 1001, 1, 'golden', ${accept_hash}, JSON_OBJECT());"
accept_event_id="$(sql "SELECT id FROM fury_event WHERE dedupe_key=${accept_hash};")"

echo "[FURY] one active profession order per household/key"
sql "INSERT IGNORE INTO fury_profession_order_instance (household_id, order_key, option_ordinal, status, accepted_event_id, last_event_id) VALUES (1, 'golden.profession.order', 1, 2, ${accept_event_id}, ${accept_event_id});"
sql "INSERT IGNORE INTO fury_profession_order_instance (household_id, order_key, option_ordinal, status, accepted_event_id, last_event_id) VALUES (1, 'golden.profession.order', 1, 2, ${accept_event_id}, ${accept_event_id});"
assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_profession_order_instance WHERE household_id=1 AND order_key='golden.profession.order' AND status=2;")" "only one active profession order exists"

instance_id="$(sql "SELECT id FROM fury_profession_order_instance WHERE household_id=1 AND order_key='golden.profession.order' AND status=2 LIMIT 1;")"

echo "[FURY] skill + item matching is exact"
assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_profession_order_instance i JOIN fury_profession_order_option o ON o.order_key=i.order_key AND o.ordinal=i.option_ordinal WHERE i.household_id=1 AND i.status=2 AND o.skill_id=164 AND o.item_id=2840;")" "matching profession/item finds active order"
assert_eq "0" "$(sql "SELECT COUNT(*) FROM fury_profession_order_instance i JOIN fury_profession_order_option o ON o.order_key=i.order_key AND o.ordinal=i.option_ordinal WHERE i.household_id=1 AND i.status=2 AND o.skill_id=165 AND o.item_id=2840;")" "wrong profession does not match"
assert_eq "0" "$(sql "SELECT COUNT(*) FROM fury_profession_order_instance i JOIN fury_profession_order_option o ON o.order_key=i.order_key AND o.ordinal=i.option_ordinal WHERE i.household_id=1 AND i.status=2 AND o.skill_id=164 AND o.item_id=2841;")" "wrong item does not match"

craft1_hash="UNHEX(SHA2('golden:profession:craft:1', 256))"
sql "INSERT INTO fury_event (event_type, actor_kind, actor_guid, account_id, household_id, subject_type, subject_id, source_system, dedupe_key, payload) VALUES ('profession.crafted', 1, 1, 1001, 1, 'profession_craft', ((CAST(164 AS UNSIGNED) << 32) | 2840), 'azerothcore', ${craft1_hash}, JSON_OBJECT('skill_id',164,'item_id',2840,'count',1));"
craft1_id="$(sql "SELECT id FROM fury_event WHERE dedupe_key=${craft1_hash};")"

echo "[FURY] first craft and replay"
sql "UPDATE fury_profession_order_instance i JOIN fury_profession_order_option o ON o.order_key=i.order_key AND o.ordinal=i.option_ordinal SET i.progress_count=LEAST(o.required_count,i.progress_count+1), i.last_event_id=${craft1_id}, i.revision=i.revision+1 WHERE i.id=${instance_id} AND i.household_id=1 AND i.status=2 AND i.last_event_id<${craft1_id};"
assert_eq "1" "$(sql "SELECT progress_count FROM fury_profession_order_instance WHERE id=${instance_id};")" "first craft increments order"

sql "UPDATE fury_profession_order_instance i JOIN fury_profession_order_option o ON o.order_key=i.order_key AND o.ordinal=i.option_ordinal SET i.progress_count=LEAST(o.required_count,i.progress_count+1), i.last_event_id=${craft1_id}, i.revision=i.revision+1 WHERE i.id=${instance_id} AND i.household_id=1 AND i.status=2 AND i.last_event_id<${craft1_id};"
assert_eq "1" "$(sql "SELECT progress_count FROM fury_profession_order_instance WHERE id=${instance_id};")" "replayed craft cannot increment twice"

craft2_hash="UNHEX(SHA2('golden:profession:craft:2', 256))"
sql "INSERT INTO fury_event (event_type, actor_kind, actor_guid, account_id, household_id, subject_type, subject_id, source_system, dedupe_key, payload) VALUES ('profession.crafted', 1, 1, 1001, 1, 'profession_craft', ((CAST(164 AS UNSIGNED) << 32) | 2840), 'azerothcore', ${craft2_hash}, JSON_OBJECT('skill_id',164,'item_id',2840,'count',1));"
craft2_id="$(sql "SELECT id FROM fury_event WHERE dedupe_key=${craft2_hash};")"

sql "UPDATE fury_profession_order_instance i JOIN fury_profession_order_option o ON o.order_key=i.order_key AND o.ordinal=i.option_ordinal SET i.progress_count=LEAST(o.required_count,i.progress_count+1), i.last_event_id=${craft2_id}, i.revision=i.revision+1 WHERE i.id=${instance_id} AND i.household_id=1 AND i.status=2 AND i.last_event_id<${craft2_id};"
assert_eq "2" "$(sql "SELECT progress_count FROM fury_profession_order_instance WHERE id=${instance_id};")" "second distinct craft reaches required count"

echo "[FURY] durable completion"
complete_hash="UNHEX(SHA2('golden:profession:complete', 256))"
sql "INSERT INTO fury_event (event_type, actor_kind, actor_guid, account_id, household_id, subject_type, subject_id, source_system, correlation_key, dedupe_key, payload) VALUES ('profession.order.completed', 1, 1, 1001, 1, 'profession_order', ${instance_id}, 'fury.professions', 'golden.profession.order', ${complete_hash}, JSON_OBJECT('instance_id',${instance_id}));"
complete_event_id="$(sql "SELECT id FROM fury_event WHERE dedupe_key=${complete_hash};")"
sql "UPDATE fury_profession_order_instance SET status=3, completed_event_id=${complete_event_id}, last_event_id=${complete_event_id}, completed_at=CURRENT_TIMESTAMP(6), revision=revision+1 WHERE id=${instance_id} AND household_id=1 AND status=2;"
assert_eq "3" "$(sql "SELECT status FROM fury_profession_order_instance WHERE id=${instance_id};")" "profession order completes"
assert_eq "${complete_event_id}" "$(sql "SELECT completed_event_id FROM fury_profession_order_instance WHERE id=${instance_id};")" "profession order retains completion event"

echo "[FURY] repeatable order may start again"
sql "INSERT IGNORE INTO fury_profession_order_instance (household_id, order_key, option_ordinal, status, accepted_event_id, last_event_id) VALUES (1, 'golden.profession.order', 1, 2, ${craft2_id}, ${craft2_id});"
assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_profession_order_instance WHERE household_id=1 AND order_key='golden.profession.order' AND status=2;")" "repeatable order can create new active instance"

echo "[FURY] re-apply preserves profession order history"
SQL_BASE="${ROOT}/modules/mod-fury/data/sql/fury/base"
while IFS= read -r file; do
  "${mysql_cmd[@]}" < "${file}"
done < <(find "${SQL_BASE}" -maxdepth 1 -type f -name '*.sql' -print | sort)

assert_eq "2" "$(sql "SELECT COUNT(*) FROM fury_profession_order_instance WHERE household_id=1 AND order_key='golden.profession.order';")" "schema re-apply preserves profession order instances"

echo "[FURY] M2 profession-order schema golden gate passed"
