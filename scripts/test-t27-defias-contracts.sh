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
  "${ROOT}/tests/golden/defias_contract_policy_golden.cpp" \
  -o "${WORKDIR}/defias_contract_policy_golden"

"${WORKDIR}/defias_contract_policy_golden"

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

assert_eq "6" "$(sql "SELECT COUNT(*) FROM fury_contract WHERE board_key='classic.westfall.contracts';")"   "six Defias contracts are seeded"

assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_contract WHERE contract_key='classic.westfall.defias.scout_report' AND title='Recon Roads' AND director_phase='rumours' AND repeat_policy=3 AND enabled=1;")"   "Recon Roads is the rumours activation contract"

assert_eq "5" "$(sql "SELECT COUNT(*) FROM fury_contract WHERE board_key='classic.westfall.contracts' AND director_phase='invasion';")"   "five follow-up contracts are invasion-phase content"

assert_eq "0" "$(sql "SELECT COUNT(*) FROM fury_contract_objective WHERE contract_key LIKE 'classic.westfall.defias.%' AND event_type='creature.killed';")"   "ordinary AzerothCore creature kills are not Defias contract objectives"

assert_eq "15" "$(sql "SELECT COUNT(*) FROM fury_contract_objective WHERE contract_key LIKE 'classic.westfall.defias.%';")"   "all authored Defias objectives are present"

assert_eq "14" "$(sql "SELECT COUNT(*) FROM fury_contract_objective WHERE contract_key LIKE 'classic.westfall.defias.%' AND event_type='living_world.entity.killed' AND subject_type='living_world_spawn_group' AND subject_id BETWEEN 100 AND 105;")"   "runtime kill objectives use only verified hostile spawn groups"

assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_contract_objective WHERE contract_key='classic.westfall.defias.field_relief' AND event_type='defias.field_relief.completed' AND subject_type='defias_contract_signal' AND subject_id=1;")"   "Field Relief is explicitly delegated to T31"

# Re-applying the migration must not duplicate first-party content.
"${mysql_cmd[@]}" < "${ROOT}/modules/mod-fury/data/sql/fury/updates/2026_09_23_10_defias_contracts.sql"
"${mysql_cmd[@]}" < "${ROOT}/modules/mod-fury/data/sql/fury/updates/2026_09_23_10_defias_contracts.sql"
assert_eq "6" "$(sql "SELECT COUNT(*) FROM fury_contract WHERE board_key='classic.westfall.contracts';")"   "Defias content migration is idempotent"

household_id="$(sql "SELECT id FROM fury_household WHERE slug='alpha' LIMIT 1;")"

sql "INSERT INTO fury_event
  (event_type, actor_kind, household_id, subject_type, subject_id, source_system, dedupe_key, payload)
  VALUES
  ('living_world.entity.killed', 1, ${household_id},
   'living_world_spawn_group', 100, 'living_world',
   UNHEX(SHA2('t27-old-runtime-kill',256)),
   JSON_OBJECT('runtime_id', 77, 'spawn_group_id', 100));"
old_event="$(sql "SELECT id FROM fury_event WHERE dedupe_key=UNHEX(SHA2('t27-old-runtime-kill',256));")"

sql "INSERT INTO fury_event
  (event_type, actor_kind, household_id, source_system, dedupe_key, payload)
  VALUES
  ('contract.board.accept.requested', 1, ${household_id},
   'fury.contract_board', UNHEX(SHA2('t27-accept',256)), JSON_OBJECT());"
accept_event="$(sql "SELECT id FROM fury_event WHERE dedupe_key=UNHEX(SHA2('t27-accept',256));")"

sql "INSERT INTO fury_director_run
  (household_id, graph_key, scope_key, status, phase_key,
   external_runtime_id, started_event_id, last_event_id)
  VALUES
  (${household_id}, 'classic.westfall.defias_resurgence.v1',
   'classic.westfall', 2, 'invasion', 77, ${accept_event}, ${accept_event});"
director_run_id="$(sql "SELECT id FROM fury_director_run WHERE household_id=${household_id} AND graph_key='classic.westfall.defias_resurgence.v1';")"

sql "INSERT INTO fury_contract_instance
  (household_id, contract_key, director_run_id, status, accepted_event_id)
  VALUES
  (${household_id}, 'classic.westfall.defias.break_scouts',
   ${director_run_id}, 2, ${accept_event});"
instance_id="$(sql "SELECT id FROM fury_contract_instance WHERE household_id=${household_id} AND contract_key='classic.westfall.defias.break_scouts' AND status=2;")"

sql "INSERT INTO fury_contract_progress
  (instance_id, objective_ordinal)
  SELECT ${instance_id}, ordinal
  FROM fury_contract_objective
  WHERE contract_key='classic.westfall.defias.break_scouts';"

old_matches="$(sql "SELECT COUNT(*)
  FROM fury_contract_objective o FORCE INDEX (ix_fury_contract_objective_match)
  JOIN fury_contract_instance i
    ON i.contract_key=o.contract_key
   AND i.household_id=${household_id}
   AND i.status=2
  JOIN fury_contract_progress p
    ON p.instance_id=i.id
   AND p.objective_ordinal=o.ordinal
  LEFT JOIN fury_director_run d
    ON d.id=i.director_run_id
  WHERE o.event_type='living_world.entity.killed'
    AND (o.subject_type IS NULL OR o.subject_type='living_world_spawn_group')
    AND (o.subject_id IS NULL OR o.subject_id=100)
    AND i.accepted_event_id <= ${old_event}
    AND (i.director_run_id IS NULL
         OR o.event_type <> 'living_world.entity.killed'
         OR d.external_runtime_id = 77);")"
assert_eq "0" "${old_matches}"   "pre-acceptance runtime kill cannot progress a contract during replay"

sql "INSERT INTO fury_event
  (event_type, actor_kind, household_id, subject_type, subject_id, source_system, dedupe_key, payload)
  VALUES
  ('creature.killed', 1, ${household_id},
   'creature', 449, 'azerothcore',
   UNHEX(SHA2('t27-ordinary-defias-kill',256)),
   JSON_OBJECT('entry', 449));"
ordinary_event="$(sql "SELECT id FROM fury_event WHERE dedupe_key=UNHEX(SHA2('t27-ordinary-defias-kill',256));")"

ordinary_matches="$(sql "SELECT COUNT(*)
  FROM fury_contract_objective o FORCE INDEX (ix_fury_contract_objective_match)
  JOIN fury_contract_instance i
    ON i.contract_key=o.contract_key
   AND i.household_id=${household_id}
   AND i.status=2
  JOIN fury_contract_progress p
    ON p.instance_id=i.id
   AND p.objective_ordinal=o.ordinal
  LEFT JOIN fury_director_run d
    ON d.id=i.director_run_id
  WHERE o.event_type='creature.killed'
    AND (o.subject_type IS NULL OR o.subject_type='creature')
    AND (o.subject_id IS NULL OR o.subject_id=449)
    AND i.accepted_event_id <= ${ordinary_event}
    AND (i.director_run_id IS NULL
         OR o.event_type <> 'living_world.entity.killed'
         OR d.external_runtime_id = 77);")"
assert_eq "0" "${ordinary_matches}"   "ordinary Westfall Defias kill cannot match runtime objectives"

sql "INSERT INTO fury_event
  (event_type, actor_kind, household_id, subject_type, subject_id, source_system, dedupe_key, payload)
  VALUES
  ('living_world.entity.killed', 1, ${household_id},
   'living_world_spawn_group', 100, 'living_world',
   UNHEX(SHA2('t27-new-runtime-kill',256)),
   JSON_OBJECT('runtime_id', 77, 'spawn_group_id', 100));"
runtime_event="$(sql "SELECT id FROM fury_event WHERE dedupe_key=UNHEX(SHA2('t27-new-runtime-kill',256));")"

runtime_matches="$(sql "SELECT COUNT(*)
  FROM fury_contract_objective o FORCE INDEX (ix_fury_contract_objective_match)
  JOIN fury_contract_instance i
    ON i.contract_key=o.contract_key
   AND i.household_id=${household_id}
   AND i.status=2
  JOIN fury_contract_progress p
    ON p.instance_id=i.id
   AND p.objective_ordinal=o.ordinal
  LEFT JOIN fury_director_run d
    ON d.id=i.director_run_id
  WHERE o.event_type='living_world.entity.killed'
    AND (o.subject_type IS NULL OR o.subject_type='living_world_spawn_group')
    AND (o.subject_id IS NULL OR o.subject_id=100)
    AND i.accepted_event_id <= ${runtime_event}
    AND (i.director_run_id IS NULL
         OR o.event_type <> 'living_world.entity.killed'
         OR d.external_runtime_id = 77);")"
assert_eq "1" "${runtime_matches}"   "post-acceptance runtime group kill matches the intended objective"

sql "INSERT INTO fury_event
  (event_type, actor_kind, household_id, subject_type, subject_id, source_system, dedupe_key, payload)
  VALUES
  ('living_world.entity.killed', 1, ${household_id},
   'living_world_spawn_group', 100, 'living_world',
   UNHEX(SHA2('t27-wrong-runtime-kill',256)),
   JSON_OBJECT('runtime_id', 88, 'spawn_group_id', 100));"
wrong_runtime_event="$(sql "SELECT id FROM fury_event WHERE dedupe_key=UNHEX(SHA2('t27-wrong-runtime-kill',256));")"

wrong_runtime_matches="$(sql "SELECT COUNT(*)
  FROM fury_contract_objective o FORCE INDEX (ix_fury_contract_objective_match)
  JOIN fury_contract_instance i
    ON i.contract_key=o.contract_key
   AND i.household_id=${household_id}
   AND i.status=2
  JOIN fury_contract_progress p
    ON p.instance_id=i.id
   AND p.objective_ordinal=o.ordinal
  LEFT JOIN fury_director_run d
    ON d.id=i.director_run_id
  WHERE o.event_type='living_world.entity.killed'
    AND (o.subject_type IS NULL OR o.subject_type='living_world_spawn_group')
    AND (o.subject_id IS NULL OR o.subject_id=100)
    AND i.accepted_event_id <= ${wrong_runtime_event}
    AND (i.director_run_id IS NULL
         OR o.event_type <> 'living_world.entity.killed'
         OR d.external_runtime_id = 88);")"
assert_eq "0" "${wrong_runtime_matches}"   "runtime kill from a different external runtime cannot progress the contract"

grep -Fq '"AND i.accepted_event_id <= ?"'   "${ROOT}/modules/mod-fury/src/database/FuryDatabase.cpp"
grep -Fq "JSON_EXTRACT(?, '$.runtime_id')"   "${ROOT}/modules/mod-fury/src/database/FuryDatabase.cpp"

echo "[FURY][PASS] T27 Defias contract schema/replay gate passed"
