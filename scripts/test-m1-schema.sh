#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SQL_BASE="${ROOT}/modules/mod-fury/data/sql/fury/base"

MYSQL_HOST="${MYSQL_HOST:-127.0.0.1}"
MYSQL_PORT="${MYSQL_PORT:-3306}"
MYSQL_USER="${MYSQL_USER:-root}"
MYSQL_PASSWORD="${MYSQL_PASSWORD:-fury}"
MYSQL_DATABASE="${MYSQL_DATABASE:-acore_fury}"

export MYSQL_PWD="${MYSQL_PASSWORD}"

mysql_cmd=(
  mysql
  --protocol=tcp
  --host="${MYSQL_HOST}"
  --port="${MYSQL_PORT}"
  --user="${MYSQL_USER}"
  --batch
  --skip-column-names
)

sql() {
  "${mysql_cmd[@]}" "${MYSQL_DATABASE}" -e "$1"
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

echo "[FURY] recreate clean schema"
"${mysql_cmd[@]}" -e "DROP DATABASE IF EXISTS \`${MYSQL_DATABASE}\`; CREATE DATABASE \`${MYSQL_DATABASE}\` CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;"

mapfile -t base_files < <(find "${SQL_BASE}" -maxdepth 1 -type f -name '*.sql' -print | sort)

if [[ "${#base_files[@]}" -eq 0 ]]; then
  echo "[FURY][FAIL] no base SQL files found" >&2
  exit 1
fi

apply_base() {
  local pass="$1"
  echo "[FURY] apply base SQL pass ${pass}"

  for file in "${base_files[@]}"; do
    echo "[FURY]   $(basename "${file}")"
    "${mysql_cmd[@]}" "${MYSQL_DATABASE}" < "${file}"
  done
}

apply_base 1
apply_base 2

expected_tables=(
  fury_household
  fury_household_member
  fury_event
  fury_event_consumer
  fury_reward_bundle
  fury_reward_entry
  fury_reward_claim
  fury_chronicle_entry
  updates
  updates_include
  version_db_fury
)

for table in "${expected_tables[@]}"; do
  present="$(sql "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema = '${MYSQL_DATABASE}' AND table_name = '${table}';")"
  assert_eq "1" "${present}" "table ${table} exists"
done

revision_count="$(sql "SELECT COUNT(*) FROM version_db_fury WHERE sql_rev='2026_09_22_00_fury_base';")"
assert_eq "1" "${revision_count}" "base revision is idempotent"

echo "[FURY] household constraints"
sql "INSERT INTO fury_household (slug, display_name) VALUES ('alpha', 'Alpha'), ('beta', 'Beta');"
sql "INSERT INTO fury_household_member (household_id, account_id, role) VALUES (1, 1001, 1);"

if sql "INSERT INTO fury_household_member (household_id, account_id, role) VALUES (2, 1001, 1);" >/dev/null 2>&1; then
  echo "[FURY][FAIL] account was allowed into two households" >&2
  exit 1
fi

assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_household_member WHERE account_id=1001;")" "account belongs to one household"

echo "[FURY] event dedupe"
event_hash="UNHEX(SHA2('golden:event:1', 256))"
sql "INSERT IGNORE INTO fury_event (event_type, actor_kind, actor_guid, account_id, household_id, source_system, dedupe_key, payload) VALUES ('golden.event', 1, 1, 1001, 1, 'golden', ${event_hash}, JSON_OBJECT('pass', 1));"
sql "INSERT IGNORE INTO fury_event (event_type, actor_kind, actor_guid, account_id, household_id, source_system, dedupe_key, payload) VALUES ('golden.event', 1, 1, 1001, 1, 'golden', ${event_hash}, JSON_OBJECT('pass', 2));"
assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_event WHERE dedupe_key=${event_hash};")" "event dedupe key is unique"

echo "[FURY] replay checkpoint monotonicity"
sql "INSERT INTO fury_event_consumer (consumer_key, last_event_id) VALUES ('golden', 10) ON DUPLICATE KEY UPDATE last_event_id=GREATEST(last_event_id, VALUES(last_event_id));"
sql "INSERT INTO fury_event_consumer (consumer_key, last_event_id) VALUES ('golden', 5) ON DUPLICATE KEY UPDATE last_event_id=GREATEST(last_event_id, VALUES(last_event_id));"
assert_eq "10" "$(sql "SELECT last_event_id FROM fury_event_consumer WHERE consumer_key='golden';")" "consumer checkpoint never moves backwards"

echo "[FURY] GS replay after simulated consumer interruption"
replay_checkpoint="$(sql "SELECT COALESCE(MAX(id), 0) FROM fury_event;")"
sql "INSERT INTO fury_event_consumer (consumer_key, last_event_id) VALUES ('golden-replay', ${replay_checkpoint}) ON DUPLICATE KEY UPDATE last_event_id=VALUES(last_event_id);"

for occurrence in 1 2 3; do
  replay_hash="UNHEX(SHA2('golden:replay:${occurrence}', 256))"
  sql "INSERT INTO fury_event (event_type, actor_kind, actor_guid, account_id, household_id, source_system, dedupe_key, payload) VALUES ('golden.replay', 1, 1, 1001, 1, 'golden', ${replay_hash}, JSON_OBJECT('occurrence', ${occurrence}));"
done

pending_before_interrupt="$(sql "SELECT COUNT(*) FROM fury_event WHERE id > (SELECT last_event_id FROM fury_event_consumer WHERE consumer_key='golden-replay');")"
assert_eq "3" "${pending_before_interrupt}" "replay consumer sees all events after checkpoint"

# Simulate a consumer that handled work but crashed before persisting its
# checkpoint: no checkpoint update occurs, therefore the same durable events
# must be visible on restart.
pending_after_interrupt="$(sql "SELECT COUNT(*) FROM fury_event WHERE id > (SELECT last_event_id FROM fury_event_consumer WHERE consumer_key='golden-replay');")"
assert_eq "3" "${pending_after_interrupt}" "interrupted consumer replays the same durable events"

replay_last="$(sql "SELECT MAX(id) FROM fury_event WHERE event_type='golden.replay';")"
sql "INSERT INTO fury_event_consumer (consumer_key, last_event_id) VALUES ('golden-replay', ${replay_last}) ON DUPLICATE KEY UPDATE last_event_id=GREATEST(last_event_id, VALUES(last_event_id));"
assert_eq "0" "$(sql "SELECT COUNT(*) FROM fury_event WHERE id > (SELECT last_event_id FROM fury_event_consumer WHERE consumer_key='golden-replay');")" "checkpoint commit clears replay backlog"

echo "[FURY] reward claim idempotency"
sql "INSERT INTO fury_reward_bundle (reward_key, minimum_power_band, maximum_power_band) VALUES ('golden.reward', 0, NULL);"
sql "INSERT IGNORE INTO fury_reward_claim (source_event_id, reward_key, beneficiary_kind, beneficiary_id, status) VALUES (1, 'golden.reward', 3, 1, 0);"
sql "INSERT IGNORE INTO fury_reward_claim (source_event_id, reward_key, beneficiary_kind, beneficiary_id, status) VALUES (1, 'golden.reward', 3, 1, 0);"
assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_reward_claim WHERE source_event_id=1 AND reward_key='golden.reward' AND beneficiary_kind=3 AND beneficiary_id=1;")" "reward claim is idempotent"

pending_claim_id="$(sql "SELECT id FROM fury_reward_claim WHERE source_event_id=1 AND reward_key='golden.reward' AND beneficiary_kind=3 AND beneficiary_id=1 LIMIT 1;")"
assert_eq "0" "$(sql "SELECT status FROM fury_reward_claim WHERE id=${pending_claim_id};")" "new reward claim starts pending"

sql "UPDATE fury_reward_claim SET status=1, delivered_at=CURRENT_TIMESTAMP(6) WHERE id=${pending_claim_id} AND status=0;"
assert_eq "1" "$(sql "SELECT status FROM fury_reward_claim WHERE id=${pending_claim_id};")" "pending reward claim transitions to delivered"
assert_eq "1" "$(sql "SELECT delivered_at IS NOT NULL FROM fury_reward_claim WHERE id=${pending_claim_id};")" "delivered reward claim records delivery timestamp"

sql "UPDATE fury_reward_claim SET status=2 WHERE id=${pending_claim_id} AND status=0;"
assert_eq "1" "$(sql "SELECT status FROM fury_reward_claim WHERE id=${pending_claim_id};")" "terminal reward claim cannot be overwritten by a second reconciliation"

echo "[FURY] Chronicle projection idempotency"
sql "INSERT IGNORE INTO fury_chronicle_entry (household_id, entry_key, category, title, source_event_id, occurred_at, metadata) SELECT 1, 'golden.entry', 'golden', 'Golden entry', id, occurred_at, JSON_OBJECT('pass', 1) FROM fury_event WHERE id=1;"
sql "INSERT IGNORE INTO fury_chronicle_entry (household_id, entry_key, category, title, source_event_id, occurred_at, metadata) SELECT 1, 'golden.entry', 'golden', 'Golden entry', id, occurred_at, JSON_OBJECT('pass', 2) FROM fury_event WHERE id=1;"
assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_chronicle_entry WHERE household_id=1 AND entry_key='golden.entry' AND source_event_id=1;")" "Chronicle replay is idempotent"

echo "[FURY] Chronicle timeline ordering"
sql "UPDATE fury_event SET occurred_at='2026-09-22 00:00:01.000000' WHERE id=1;"
order_hash="UNHEX(SHA2('golden:chronicle:order:2', 256))"
sql "INSERT INTO fury_event (occurred_at, event_type, actor_kind, actor_guid, account_id, household_id, source_system, dedupe_key, payload) VALUES ('2026-09-22 00:00:02.000000', 'golden.chronicle.order', 1, 1, 1001, 1, 'golden', ${order_hash}, JSON_OBJECT('order', 2));"
order_event_id="$(sql "SELECT id FROM fury_event WHERE dedupe_key=${order_hash};")"
sql "INSERT INTO fury_chronicle_entry (household_id, entry_key, category, title, source_event_id, occurred_at) VALUES (1, 'golden.order.first', 'golden', 'First', 1, '2026-09-22 00:00:01.000000');"
sql "INSERT INTO fury_chronicle_entry (household_id, entry_key, category, title, source_event_id, occurred_at) VALUES (1, 'golden.order.second', 'golden', 'Second', ${order_event_id}, '2026-09-22 00:00:02.000000');"
timeline_order="$(sql "SELECT GROUP_CONCAT(entry_key ORDER BY occurred_at DESC, id DESC SEPARATOR ',') FROM fury_chronicle_entry WHERE household_id=1 AND entry_key LIKE 'golden.order.%';")"
assert_eq "golden.order.second,golden.order.first" "${timeline_order}" "Chronicle timeline is deterministic newest-first"

echo "[FURY] clean re-apply after populated data"
apply_base 3

assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_household WHERE slug='alpha';")" "base re-apply preserves household data"
assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_event WHERE id=1;")" "base re-apply preserves event data"

echo "[FURY] M1 schema golden gate passed"
