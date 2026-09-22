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

bash "${ROOT}/scripts/test-m2-schema.sh"

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

for table in fury_contract fury_contract_objective fury_contract_instance fury_contract_progress; do
  present="$(sql "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema = '${MYSQL_DATABASE}' AND table_name = '${table}';")"
  assert_eq "1" "${present}" "table ${table} exists"
done

echo "[FURY] seed repeatable contract"
sql "INSERT INTO fury_contract (contract_key, board_key, title, campaign_node_key, repeat_policy, reward_key, enabled) VALUES ('golden.contract', 'golden.board', 'Golden Contract', 'golden.campaign', 2, 'golden.reward', 1);"
sql "INSERT INTO fury_contract_objective (contract_key, ordinal, objective_type, event_type, required_count) VALUES ('golden.contract', 1, 11, 'golden.event', 2);"

echo "[FURY] active instance uniqueness"
sql "INSERT IGNORE INTO fury_contract_instance (household_id, contract_key, status, accepted_event_id) VALUES (1, 'golden.contract', 2, 1);"
sql "INSERT IGNORE INTO fury_contract_instance (household_id, contract_key, status, accepted_event_id) VALUES (1, 'golden.contract', 2, 1);"
assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_contract_instance WHERE household_id=1 AND contract_key='golden.contract' AND status=2;")" "only one active instance exists"

instance_id="$(sql "SELECT id FROM fury_contract_instance WHERE household_id=1 AND contract_key='golden.contract' AND status=2 LIMIT 1;")"
sql "INSERT IGNORE INTO fury_contract_progress (instance_id, objective_ordinal) SELECT ${instance_id}, ordinal FROM fury_contract_objective WHERE contract_key='golden.contract';"
sql "INSERT IGNORE INTO fury_contract_progress (instance_id, objective_ordinal) SELECT ${instance_id}, ordinal FROM fury_contract_objective WHERE contract_key='golden.contract';"
assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_contract_progress WHERE instance_id=${instance_id};")" "progress initialization is idempotent"

echo "[FURY] indexed objective matching"
assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_contract_instance i JOIN fury_contract_objective o ON o.contract_key=i.contract_key JOIN fury_contract_progress p ON p.instance_id=i.id AND p.objective_ordinal=o.ordinal WHERE i.household_id=1 AND i.status=2 AND o.event_type='golden.event' AND (o.subject_type IS NULL OR o.subject_type='none') AND (o.subject_id IS NULL OR o.subject_id=0);")" "matching query finds active objective"

echo "[FURY] event replay does not double-progress"
sql "UPDATE fury_contract_progress p JOIN fury_contract_instance i ON i.id=p.instance_id AND i.status=2 JOIN fury_contract_objective o ON o.contract_key='golden.contract' AND o.ordinal=p.objective_ordinal SET p.completed_at=CASE WHEN p.progress_count+1>=o.required_count THEN COALESCE(p.completed_at,CURRENT_TIMESTAMP(6)) ELSE p.completed_at END, p.progress_count=LEAST(o.required_count,p.progress_count+1), p.last_event_id=1, p.revision=p.revision+1 WHERE p.instance_id=${instance_id} AND p.objective_ordinal=1 AND p.last_event_id<1;"
assert_eq "1" "$(sql "SELECT progress_count FROM fury_contract_progress WHERE instance_id=${instance_id} AND objective_ordinal=1;")" "first event increments objective"

sql "UPDATE fury_contract_progress p JOIN fury_contract_instance i ON i.id=p.instance_id AND i.status=2 JOIN fury_contract_objective o ON o.contract_key='golden.contract' AND o.ordinal=p.objective_ordinal SET p.completed_at=CASE WHEN p.progress_count+1>=o.required_count THEN COALESCE(p.completed_at,CURRENT_TIMESTAMP(6)) ELSE p.completed_at END, p.progress_count=LEAST(o.required_count,p.progress_count+1), p.last_event_id=1, p.revision=p.revision+1 WHERE p.instance_id=${instance_id} AND p.objective_ordinal=1 AND p.last_event_id<1;"
assert_eq "1" "$(sql "SELECT progress_count FROM fury_contract_progress WHERE instance_id=${instance_id} AND objective_ordinal=1;")" "replayed event cannot increment twice"

event2_hash="UNHEX(SHA2('golden:event:2', 256))"
sql "INSERT INTO fury_event (event_type, actor_kind, actor_guid, account_id, household_id, source_system, dedupe_key, payload) VALUES ('golden.event', 1, 1, 1001, 1, 'golden', ${event2_hash}, JSON_OBJECT('pass', 2));"
event2_id="$(sql "SELECT id FROM fury_event WHERE dedupe_key=${event2_hash};")"

sql "UPDATE fury_contract_progress p JOIN fury_contract_instance i ON i.id=p.instance_id AND i.status=2 JOIN fury_contract_objective o ON o.contract_key='golden.contract' AND o.ordinal=p.objective_ordinal SET p.completed_at=CASE WHEN p.progress_count+1>=o.required_count THEN COALESCE(p.completed_at,CURRENT_TIMESTAMP(6)) ELSE p.completed_at END, p.progress_count=LEAST(o.required_count,p.progress_count+1), p.last_event_id=${event2_id}, p.revision=p.revision+1 WHERE p.instance_id=${instance_id} AND p.objective_ordinal=1 AND p.last_event_id<${event2_id};"
assert_eq "2" "$(sql "SELECT progress_count FROM fury_contract_progress WHERE instance_id=${instance_id} AND objective_ordinal=1;")" "second durable event completes objective"
assert_eq "0" "$(sql "SELECT COUNT(*) FROM fury_contract_progress p JOIN fury_contract_objective o ON o.contract_key='golden.contract' AND o.ordinal=p.objective_ordinal WHERE p.instance_id=${instance_id} AND p.progress_count<o.required_count;")" "all objectives are complete"

echo "[FURY] completion event closes instance"
completion_hash="UNHEX(SHA2('contract:completed:v1:golden', 256))"
sql "INSERT INTO fury_event (event_type, actor_kind, actor_guid, account_id, household_id, subject_type, subject_id, source_system, correlation_key, dedupe_key, payload) VALUES ('contract.completed', 1, 1, 1001, 1, 'contract_instance', ${instance_id}, 'fury.contracts', 'golden.contract', ${completion_hash}, JSON_OBJECT('instance_id', ${instance_id}));"
completion_event_id="$(sql "SELECT id FROM fury_event WHERE dedupe_key=${completion_hash};")"
sql "UPDATE fury_contract_instance SET status=3, completed_event_id=${completion_event_id}, completed_at=CURRENT_TIMESTAMP(6), revision=revision+1 WHERE id=${instance_id} AND status=2;"
assert_eq "3" "$(sql "SELECT status FROM fury_contract_instance WHERE id=${instance_id};")" "instance becomes complete"
assert_eq "${completion_event_id}" "$(sql "SELECT completed_event_id FROM fury_contract_instance WHERE id=${instance_id};")" "instance retains completion event"

echo "[FURY] reward claim can use contract completion event"
sql "INSERT IGNORE INTO fury_reward_claim (source_event_id, reward_key, beneficiary_kind, beneficiary_id, status) VALUES (${completion_event_id}, 'golden.reward', 3, 1, 0);"
sql "INSERT IGNORE INTO fury_reward_claim (source_event_id, reward_key, beneficiary_kind, beneficiary_id, status) VALUES (${completion_event_id}, 'golden.reward', 3, 1, 0);"
assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_reward_claim WHERE source_event_id=${completion_event_id} AND reward_key='golden.reward' AND beneficiary_kind=3 AND beneficiary_id=1;")" "completion reward claim is idempotent"

echo "[FURY] repeatable contract may open a new instance after completion"
sql "INSERT IGNORE INTO fury_contract_instance (household_id, contract_key, status, accepted_event_id) VALUES (1, 'golden.contract', 2, ${event2_id});"
assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_contract_instance WHERE household_id=1 AND contract_key='golden.contract' AND status=2;")" "new active repeat instance is allowed"

echo "[FURY] schema re-apply preserves contract state"
SQL_BASE="${ROOT}/modules/mod-fury/data/sql/fury/base"
while IFS= read -r file; do
  "${mysql_cmd[@]}" < "${file}"
done < <(find "${SQL_BASE}" -maxdepth 1 -type f -name '*.sql' -print | sort)

assert_eq "2" "$(sql "SELECT COUNT(*) FROM fury_contract_instance WHERE household_id=1 AND contract_key='golden.contract';")" "schema re-apply preserves contract instances"

echo "[FURY] M2 contracts schema golden gate passed"
