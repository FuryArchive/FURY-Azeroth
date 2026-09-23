#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKDIR="$(mktemp -d)"
trap 'rm -rf "${WORKDIR}"' EXIT

cat >"${WORKDIR}/Define.h" <<'EOF'
#pragma once
#include <cstdint>
using uint8 = std::uint8_t;
using uint16 = std::uint16_t;
using uint32 = std::uint32_t;
using uint64 = std::uint64_t;
EOF

CXX="${CXX:-g++}"
"${CXX}" \
  -std=c++17 \
  -Wall -Wextra -Werror \
  -I"${WORKDIR}" \
  -I"${ROOT}/modules/mod-fury/src" \
  "${ROOT}/tests/golden/defias_field_relief_policy_golden.cpp" \
  -o "${WORKDIR}/defias_field_relief_policy_golden"

"${WORKDIR}/defias_field_relief_policy_golden"

export MYSQL_HOST="${MYSQL_HOST:-127.0.0.1}"
export MYSQL_PORT="${MYSQL_PORT:-3306}"
export MYSQL_USER="${MYSQL_USER:-root}"
export MYSQL_PASSWORD="${MYSQL_PASSWORD:-fury}"
export MYSQL_DATABASE="${MYSQL_DATABASE:-acore_fury}"
export MYSQL_PWD="${MYSQL_PASSWORD}"

# T17 remains the replay/idempotency authority for the generic order engine.
bash "${ROOT}/scripts/test-m2-professions-schema.sh"

# Rebuild current schema/content after the T17 migration-path test deliberately
# drops and recreates the generic profession-order tables.
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

ORDER="classic.westfall.defias.field_relief.supplies"
CONTRACT="classic.westfall.defias.field_relief"

assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_profession_order
  WHERE order_key='${ORDER}' AND title='Field Relief Supplies'
    AND repeat_policy=1 AND enabled=1;")"   "Field Relief order is seeded once/enabled"

assert_eq "7" "$(sql "SELECT COUNT(*) FROM fury_profession_order_option
  WHERE order_key='${ORDER}';")"   "seven supported profession paths are seeded"

assert_eq "129:1251:8,171:118:5,185:2679:8,165:2304:4,164:2862:6,197:2996:6,202:4357:8"   "$(sql "SELECT GROUP_CONCAT(CONCAT(skill_id,':',item_id,':',required_count)
    ORDER BY ordinal SEPARATOR ',')
    FROM fury_profession_order_option WHERE order_key='${ORDER}';")"   "Field Relief option ids/counts match accepted content"

assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_contract_objective
  WHERE contract_key='${CONTRACT}'
    AND event_type='defias.field_relief.completed'
    AND subject_type='defias_contract_signal'
    AND subject_id=1
    AND required_count=1;")"   "Field Relief contract consumes one stable completion signal"

"${mysql_cmd[@]}" < "${ROOT}/modules/mod-fury/data/sql/fury/updates/2026_09_24_02_defias_field_relief.sql"
"${mysql_cmd[@]}" < "${ROOT}/modules/mod-fury/data/sql/fury/updates/2026_09_24_02_defias_field_relief.sql"
assert_eq "7" "$(sql "SELECT COUNT(*) FROM fury_profession_order_option
  WHERE order_key='${ORDER}';")"   "Field Relief migration is idempotent"

household_id="$(sql "SELECT id FROM fury_household WHERE slug='alpha' LIMIT 1;")"

sql "INSERT INTO fury_event
  (event_type, actor_kind, account_id, household_id, source_system,
   correlation_key, dedupe_key, payload)
  VALUES
  ('contract.board.accept.requested',1,1001,${household_id},
   'fury.contract_board','${CONTRACT}',
   UNHEX(SHA2('t31-contract-accept',256)),JSON_OBJECT());"
accept_event="$(sql "SELECT id FROM fury_event
  WHERE dedupe_key=UNHEX(SHA2('t31-contract-accept',256));")"

sql "INSERT INTO fury_contract_instance
  (household_id, contract_key, status, accepted_event_id)
  VALUES (${household_id},'${CONTRACT}',2,${accept_event});"
contract_instance="$(sql "SELECT id FROM fury_contract_instance
  WHERE household_id=${household_id} AND contract_key='${CONTRACT}' AND status=2;")"

sql "INSERT INTO fury_contract_progress
  (instance_id, objective_ordinal, last_event_id)
  VALUES (${contract_instance},1,${accept_event});"

sql "INSERT INTO fury_event
  (event_type, actor_kind, account_id, household_id, subject_type, subject_id,
   source_system, correlation_key, dedupe_key, payload)
  VALUES
  ('profession.order.completed',1,1001,${household_id},'profession_order',77,
   'fury.professions','${ORDER}',
   UNHEX(SHA2('t31-order-complete',256)),JSON_OBJECT());"
order_completion="$(sql "SELECT id FROM fury_event
  WHERE dedupe_key=UNHEX(SHA2('t31-order-complete',256));")"

signal_identity="defias:field-relief:v1:77"
sql "INSERT IGNORE INTO fury_event
  (event_type, actor_kind, account_id, household_id, subject_type, subject_id,
   source_system, correlation_key, dedupe_key, payload)
  VALUES
  ('defias.field_relief.completed',1,1001,${household_id},
   'defias_contract_signal',1,'fury.defias','${CONTRACT}',
   UNHEX(SHA2('${signal_identity}',256)),
   JSON_OBJECT('profession_order_instance_id',77,'source_event_id',${order_completion}));"
sql "INSERT IGNORE INTO fury_event
  (event_type, actor_kind, account_id, household_id, subject_type, subject_id,
   source_system, correlation_key, dedupe_key, payload)
  VALUES
  ('defias.field_relief.completed',1,1001,${household_id},
   'defias_contract_signal',1,'fury.defias','${CONTRACT}',
   UNHEX(SHA2('${signal_identity}',256)),
   JSON_OBJECT('profession_order_instance_id',77,'source_event_id',${order_completion}));"

assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_event
  WHERE dedupe_key=UNHEX(SHA2('${signal_identity}',256));")"   "Field Relief completion signal is replay-idempotent"

signal_event="$(sql "SELECT id FROM fury_event
  WHERE dedupe_key=UNHEX(SHA2('${signal_identity}',256));")"

matches="$(sql "SELECT COUNT(*)
  FROM fury_contract_objective o FORCE INDEX (ix_fury_contract_objective_match)
  JOIN fury_contract_instance i
    ON i.contract_key=o.contract_key
   AND i.household_id=${household_id}
   AND i.status=2
  JOIN fury_contract_progress p
    ON p.instance_id=i.id AND p.objective_ordinal=o.ordinal
  WHERE o.event_type='defias.field_relief.completed'
    AND (o.subject_type IS NULL OR o.subject_type='defias_contract_signal')
    AND (o.subject_id IS NULL OR o.subject_id=1)
    AND i.accepted_event_id <= ${signal_event};")"
assert_eq "1" "${matches}"   "post-acceptance Field Relief signal matches the active contract"

BOARD="${ROOT}/modules/mod-fury/src/scripts/FuryDefiasBoardScript.cpp"
DEFIAS="${ROOT}/modules/mod-fury/src/content/defias/DefiasService.cpp"

grep -Fq 'SelectFieldReliefOption' "${BOARD}"
grep -Fq 'ProfessionOrders().Start' "${BOARD}"
grep -Fq 'FieldReliefOrderKey' "${BOARD}"
grep -Fq 'profession.order.completed' "${DEFIAS}"
grep -Fq 'defias.field_relief.completed' "${DEFIAS}"
grep -Fq 'defias:field-relief:v1:' "${DEFIAS}"

echo "[FURY][PASS] T31 adaptive Field Relief gate passed"
