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

bash "${ROOT}/scripts/test-m2-proof-schema.sh"

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

for table in fury_contract fury_contract_objective fury_contract_instance fury_contract_progress; do
  assert_eq "1" "$(sql "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema='${MYSQL_DATABASE}' AND table_name='${table}';")" "table ${table} exists"
done

echo "[FURY] T14 -> T15 migration path"
sql "DROP TABLE fury_contract_progress; DROP TABLE fury_contract_instance; DROP TABLE fury_contract_objective; DROP TABLE fury_contract;"
"${mysql_cmd[@]}" < "${ROOT}/modules/mod-fury/data/sql/fury/updates/2026_09_22_03_contracts.sql"
for table in fury_contract fury_contract_objective fury_contract_instance fury_contract_progress; do
  assert_eq "1" "$(sql "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema='${MYSQL_DATABASE}' AND table_name='${table}';")" "migration creates ${table}"
done

assert_eq "1" "$(sql "SELECT COUNT(*) FROM information_schema.statistics WHERE table_schema='${MYSQL_DATABASE}' AND table_name='fury_contract_objective' AND index_name='ix_fury_contract_objective_match' AND seq_in_index=1 AND column_name='event_type';")" "objective match index begins with event_type"

household_id="$(sql "SELECT id FROM fury_household WHERE slug='alpha' LIMIT 1;")"
accept_event="$(sql "SELECT MIN(id) FROM fury_event WHERE household_id=${household_id};")"

sql "INSERT INTO fury_contract
  (contract_key, board_key, title, repeat_policy, enabled)
  VALUES ('golden.contract', 'golden.board', 'Golden Contract', 2, 1);"
sql "INSERT INTO fury_contract_objective
  (contract_key, ordinal, objective_type, event_type, subject_type, subject_id, required_count)
  VALUES ('golden.contract', 1, 1, 'creature.kill', 'creature', 4242, 2);"

echo "[FURY] active instance + progress initialization are idempotent"
sql "INSERT IGNORE INTO fury_contract_instance
  (household_id, contract_key, status, accepted_event_id)
  VALUES (${household_id}, 'golden.contract', 2, ${accept_event});"
sql "INSERT IGNORE INTO fury_contract_instance
  (household_id, contract_key, status, accepted_event_id)
  VALUES (${household_id}, 'golden.contract', 2, ${accept_event});"
assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_contract_instance WHERE household_id=${household_id} AND contract_key='golden.contract' AND status=2;")" "only one active instance exists"

instance_id="$(sql "SELECT id FROM fury_contract_instance WHERE household_id=${household_id} AND contract_key='golden.contract' AND status=2 LIMIT 1;")"
sql "INSERT IGNORE INTO fury_contract_progress
  (instance_id, objective_ordinal)
  SELECT ${instance_id}, ordinal FROM fury_contract_objective WHERE contract_key='golden.contract';"
sql "INSERT IGNORE INTO fury_contract_progress
  (instance_id, objective_ordinal)
  SELECT ${instance_id}, ordinal FROM fury_contract_objective WHERE contract_key='golden.contract';"
assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_contract_progress WHERE instance_id=${instance_id};")" "progress initialization is idempotent"

echo "[FURY] unrelated events match no objectives"
assert_eq "0" "$(sql "SELECT COUNT(*)
  FROM fury_contract_objective o FORCE INDEX (ix_fury_contract_objective_match)
  JOIN fury_contract_instance i ON i.contract_key=o.contract_key AND i.household_id=${household_id} AND i.status=2
  JOIN fury_contract_progress p ON p.instance_id=i.id AND p.objective_ordinal=o.ordinal
  WHERE o.event_type='item.create'
    AND (o.subject_type IS NULL OR o.subject_type='item')
    AND (o.subject_id IS NULL OR o.subject_id=4242);")" "unrelated event does not touch active objectives"

make_event() {
  local identity="$1"
  sql "INSERT INTO fury_event
    (event_type, actor_kind, household_id, subject_type, subject_id, source_system, dedupe_key, payload)
    VALUES
    ('creature.kill', 1, ${household_id}, 'creature', 4242, 'golden.contracts',
     UNHEX(SHA2('${identity}',256)), JSON_OBJECT('identity','${identity}'));"
  sql "SELECT id FROM fury_event WHERE dedupe_key=UNHEX(SHA2('${identity}',256));"
}

event1="$(make_event golden-contract-event-1)"
event2="$(make_event golden-contract-event-2)"

advance() {
  local event_id="$1"
  sql "UPDATE fury_contract_progress p
    JOIN fury_contract_instance i ON i.id=p.instance_id AND i.status=2
    JOIN fury_contract_objective o ON o.contract_key='golden.contract' AND o.ordinal=p.objective_ordinal
    SET p.completed_at=CASE
          WHEN p.progress_count+1>=o.required_count
          THEN COALESCE(p.completed_at,CURRENT_TIMESTAMP(6))
          ELSE p.completed_at END,
        p.progress_count=LEAST(o.required_count,p.progress_count+1),
        p.last_event_id=${event_id},
        p.revision=p.revision+1
    WHERE p.instance_id=${instance_id}
      AND p.objective_ordinal=1
      AND p.last_event_id<${event_id};"
}

echo "[FURY] replayed kill event increments exactly once"
advance "${event1}"
assert_eq "1" "$(sql "SELECT progress_count FROM fury_contract_progress WHERE instance_id=${instance_id} AND objective_ordinal=1;")" "first matching event increments"
advance "${event1}"
assert_eq "1" "$(sql "SELECT progress_count FROM fury_contract_progress WHERE instance_id=${instance_id} AND objective_ordinal=1;")" "replayed event does not double-count"
advance "${event2}"
assert_eq "2" "$(sql "SELECT progress_count FROM fury_contract_progress WHERE instance_id=${instance_id} AND objective_ordinal=1;")" "second matching event completes objective"
assert_eq "0" "$(sql "SELECT COUNT(*) FROM fury_contract_progress p JOIN fury_contract_objective o ON o.contract_key='golden.contract' AND o.ordinal=p.objective_ordinal WHERE p.instance_id=${instance_id} AND p.progress_count<o.required_count;")" "all objectives complete"

echo "[FURY] crash-recovery completion emits one durable event"
completion_identity="contract:completed:v1:${instance_id}"
sql "INSERT IGNORE INTO fury_event
  (event_type, actor_kind, household_id, subject_type, subject_id, source_system, correlation_key, dedupe_key, payload)
  VALUES
  ('contract.completed',1,${household_id},'contract_instance',${instance_id},'fury.contracts','golden.contract',
   UNHEX(SHA2('${completion_identity}',256)), JSON_OBJECT('instance_id',${instance_id},'source_event_id',${event2}));"
completion_event_id="$(sql "SELECT id FROM fury_event WHERE dedupe_key=UNHEX(SHA2('${completion_identity}',256));")"

# Simulate a crash here: event durable, instance still Active. Replay must be able
# to re-observe completed progress, re-append the same dedupe identity, and close.
sql "INSERT IGNORE INTO fury_event
  (event_type, actor_kind, household_id, subject_type, subject_id, source_system, correlation_key, dedupe_key, payload)
  VALUES
  ('contract.completed',1,${household_id},'contract_instance',${instance_id},'fury.contracts','golden.contract',
   UNHEX(SHA2('${completion_identity}',256)), JSON_OBJECT('instance_id',${instance_id},'source_event_id',${event2}));"
assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_event WHERE dedupe_key=UNHEX(SHA2('${completion_identity}',256));")" "completion event is durable and unique"

sql "UPDATE fury_contract_instance
  SET status=3,
      completed_event_id=${completion_event_id},
      completed_at=COALESCE(completed_at,CURRENT_TIMESTAMP(6)),
      revision=revision+1
  WHERE id=${instance_id} AND status=2;"
assert_eq "3" "$(sql "SELECT status FROM fury_contract_instance WHERE id=${instance_id};")" "recovery closes completed instance"
assert_eq "${completion_event_id}" "$(sql "SELECT completed_event_id FROM fury_contract_instance WHERE id=${instance_id};")" "instance retains one completion event"

echo "[FURY] repeatable contract may open another active instance after completion"
sql "INSERT IGNORE INTO fury_contract_instance
  (household_id, contract_key, status, accepted_event_id)
  VALUES (${household_id}, 'golden.contract', 2, ${event2});"
assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_contract_instance WHERE household_id=${household_id} AND contract_key='golden.contract' AND status=2;")" "repeatable contract can reopen"

echo "[FURY] schema re-apply preserves contract state"
"${mysql_cmd[@]}" < "${ROOT}/modules/mod-fury/data/sql/fury/base/60_contracts.sql"
"${mysql_cmd[@]}" < "${ROOT}/modules/mod-fury/data/sql/fury/updates/2026_09_22_03_contracts.sql"
assert_eq "2" "$(sql "SELECT COUNT(*) FROM fury_contract_instance WHERE household_id=${household_id} AND contract_key='golden.contract';")" "contract instances survive schema re-apply"

echo "[FURY][PASS] T15 Contracts schema golden gate passed"
