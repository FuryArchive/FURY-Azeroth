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
"${CXX}" -std=c++17 -Wall -Wextra -Werror   -I"${WORKDIR}"   -I"${ROOT}/modules/mod-fury/src"   "${ROOT}/tests/golden/defias_field_relief_golden.cpp"   -o "${WORKDIR}/defias_field_relief_golden"
"${WORKDIR}/defias_field_relief_golden"

export MYSQL_HOST="${MYSQL_HOST:-127.0.0.1}"
export MYSQL_PORT="${MYSQL_PORT:-3306}"
export MYSQL_USER="${MYSQL_USER:-root}"
export MYSQL_PASSWORD="${MYSQL_PASSWORD:-fury}"
export MYSQL_DATABASE="${MYSQL_DATABASE:-acore_fury}"
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
  local label="$3"
  if [[ "${actual}" != "${expected}" ]]; then
    echo "[FURY][FAIL] ${label}: expected=${expected} actual=${actual}" >&2
    exit 1
  fi
  echo "[FURY][PASS] ${label}"
}

ORDER="classic.westfall.defias.field_relief"
CONTRACT="classic.westfall.defias.field_relief"

assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_profession_order
  WHERE order_key='${ORDER}' AND repeat_policy=2 AND enabled=1;")"   "Field Relief repeatable order exists"

assert_eq "3" "$(sql "SELECT COUNT(*) FROM fury_profession_order_option
  WHERE order_key='${ORDER}';")"   "Field Relief has three adaptive options"

assert_eq "171:118:3,129:1251:6,185:2681:6" "$(sql "
  SELECT GROUP_CONCAT(CONCAT(skill_id,':',item_id,':',required_count)
    ORDER BY ordinal)
  FROM fury_profession_order_option
  WHERE order_key='${ORDER}';")"   "Field Relief profession/item targets are exact"

assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_contract_objective
  WHERE contract_key='${CONTRACT}'
    AND event_type='defias.field_relief.completed'
    AND subject_type='defias_contract_signal'
    AND subject_id=1
    AND required_count=1;")"   "T27 Field Relief objective consumes stable T31 signal"

household_id="$(sql "SELECT id FROM fury_household WHERE slug='alpha' LIMIT 1;")"

sql "INSERT INTO fury_event
  (event_type, actor_kind, household_id, source_system, dedupe_key, payload)
  VALUES
  ('golden.field-relief.accept',1,${household_id},'golden.t31',
   UNHEX(SHA2('t31-field-relief-accept',256)),JSON_OBJECT());"
accept_event="$(sql "SELECT id FROM fury_event
  WHERE dedupe_key=UNHEX(SHA2('t31-field-relief-accept',256));")"

sql "INSERT INTO fury_contract_instance
  (household_id, contract_key, status, accepted_event_id, last_event_id)
  VALUES
  (${household_id},'${CONTRACT}',2,${accept_event},${accept_event});"
contract_instance="$(sql "SELECT id FROM fury_contract_instance
  WHERE household_id=${household_id} AND contract_key='${CONTRACT}'
    AND status=2 ORDER BY id DESC LIMIT 1;")"

sql "INSERT INTO fury_contract_progress
  (instance_id, objective_ordinal, progress_count, last_event_id)
  VALUES (${contract_instance},1,0,${accept_event});"

sql "INSERT INTO fury_profession_order_instance
  (household_id, order_key, option_ordinal, status,
   progress_count, accepted_event_id, last_event_id)
  VALUES
  (${household_id},'${ORDER}',1,3,3,${accept_event},${accept_event});"
order_instance="$(sql "SELECT id FROM fury_profession_order_instance
  WHERE household_id=${household_id} AND order_key='${ORDER}'
  ORDER BY id DESC LIMIT 1;")"

sql "INSERT INTO fury_event
  (event_type, actor_kind, household_id, subject_type, subject_id,
   source_system, correlation_key, dedupe_key, payload)
  VALUES
  ('profession.order.completed',1,${household_id},
   'profession_order',${order_instance},'fury.professions','${ORDER}',
   UNHEX(SHA2('t31-profession-complete',256)),
   JSON_OBJECT('instance_id',${order_instance}));"
profession_event="$(sql "SELECT id FROM fury_event
  WHERE dedupe_key=UNHEX(SHA2('t31-profession-complete',256));")"

signal_identity="defias:field-relief:completed:v1:${order_instance}"
sql "INSERT IGNORE INTO fury_event
  (event_type, actor_kind, household_id, subject_type, subject_id,
   source_system, correlation_key, dedupe_key, payload)
  VALUES
  ('defias.field_relief.completed',1,${household_id},
   'defias_contract_signal',1,'fury.defias','${CONTRACT}',
   UNHEX(SHA2('${signal_identity}',256)),
   JSON_OBJECT('profession_order_instance_id',${order_instance},
               'profession_completion_event_id',${profession_event}));"
signal_event="$(sql "SELECT id FROM fury_event
  WHERE dedupe_key=UNHEX(SHA2('${signal_identity}',256));")"

assert_eq "1" "$(sql "SELECT COUNT(*)
  FROM fury_contract_objective o
  JOIN fury_contract_instance i
    ON i.contract_key=o.contract_key
   AND i.household_id=${household_id}
   AND i.status=2
  WHERE o.event_type='defias.field_relief.completed'
    AND o.subject_type='defias_contract_signal'
    AND o.subject_id=1
    AND i.accepted_event_id<=${signal_event};")"   "post-acceptance Field Relief signal matches active contract"

sql "UPDATE fury_contract_progress
  SET progress_count=LEAST(1,progress_count+1),
      last_event_id=${signal_event}
  WHERE instance_id=${contract_instance}
    AND objective_ordinal=1
    AND last_event_id<${signal_event};"
sql "UPDATE fury_contract_progress
  SET progress_count=LEAST(1,progress_count+1),
      last_event_id=${signal_event}
  WHERE instance_id=${contract_instance}
    AND objective_ordinal=1
    AND last_event_id<${signal_event};"

assert_eq "1" "$(sql "SELECT progress_count FROM fury_contract_progress
  WHERE instance_id=${contract_instance} AND objective_ordinal=1;")"   "replayed Field Relief signal does not double-count"

"${mysql_cmd[@]}" < "${ROOT}/modules/mod-fury/data/sql/fury/updates/2026_09_24_02_defias_field_relief.sql"
"${mysql_cmd[@]}" < "${ROOT}/modules/mod-fury/data/sql/fury/updates/2026_09_24_02_defias_field_relief.sql"
assert_eq "3" "$(sql "SELECT COUNT(*) FROM fury_profession_order_option
  WHERE order_key='${ORDER}';")"   "T31 content migration is idempotent"

BOARD="${ROOT}/modules/mod-fury/src/scripts/FuryDefiasBoardScript.cpp"
SERVICE="${ROOT}/modules/mod-fury/src/content/defias/DefiasFieldReliefService.cpp"

grep -Fq 'FieldReliefChoiceFor' "${BOARD}"
grep -Fq 'GetSkillValue' "${BOARD}"
grep -Fq 'ProfessionOrders().Start' "${BOARD}"
grep -Fq 'defias.field_relief.completed' "${SERVICE}"
grep -Fq 'profession.order.completed' "${SERVICE}"

echo "[FURY][PASS] T31 adaptive Field Relief gate passed"
