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
  "${ROOT}/tests/golden/defias_score_policy_golden.cpp" \
  -o "${WORKDIR}/defias_score_policy_golden"

"${WORKDIR}/defias_score_policy_golden"

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

GRAPH="classic.westfall.defias_resurgence.v1"

for table in fury_director_score_component fury_director_score_award; do
  assert_eq "1" "$(sql "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema='${MYSQL_DATABASE}' AND table_name='${table}';")"     "table ${table} exists"
done

assert_eq "7" "$(sql "SELECT COUNT(*) FROM fury_director_score_component WHERE graph_key='${GRAPH}' AND enabled=1;")"   "Defias score has seven enabled components"

assert_eq "100" "$(sql "SELECT SUM(score_value) FROM fury_director_score_component WHERE graph_key='${GRAPH}' AND enabled=1;")"   "Defias score components total exactly 100"

assert_eq "10,10,15,15,10,25,15" "$(sql "SELECT GROUP_CONCAT(score_value ORDER BY FIELD(component_key,'recon','scouts','control','hold_sentinel','field_relief','captain','final_stage_presence') SEPARATOR ',') FROM fury_director_score_component WHERE graph_key='${GRAPH}';")"   "accepted T29 component weights are seeded"

assert_eq "85" "$(sql "SELECT SUM(score_value) FROM fury_director_score_component WHERE graph_key='${GRAPH}' AND source_event_type='contract.completed';")"   "six contract components contribute 85 points"

assert_eq "15" "$(sql "SELECT score_value FROM fury_director_score_component WHERE graph_key='${GRAPH}' AND component_key='final_stage_presence' AND source_event_type='defias.final_stage.participated' AND source_correlation_key='${GRAPH}';")"   "final-stage human participation contributes 15 points"

assert_eq "90" "$(sql "SELECT SUM(score_value) FROM fury_director_score_component WHERE graph_key='${GRAPH}' AND component_key<>'field_relief';")"   "optional Field Relief is not required to make Success reachable"

household_id="$(sql "SELECT id FROM fury_household WHERE slug='alpha' LIMIT 1;")"

make_event() {
  local identity="$1"
  local event_type="$2"
  local correlation="$3"
  sql "INSERT INTO fury_event
    (event_type, actor_kind, household_id, source_system, correlation_key, dedupe_key, payload)
    VALUES
    ('${event_type}', 1, ${household_id}, 'golden.score', '${correlation}',
     UNHEX(SHA2('${identity}',256)), JSON_OBJECT('identity','${identity}'));"
  sql "SELECT id FROM fury_event WHERE dedupe_key=UNHEX(SHA2('${identity}',256));"
}

start_event="$(make_event t29-run-start golden.start "${GRAPH}")"

sql "INSERT INTO fury_director_run
  (household_id, graph_key, scope_key, status, phase_key, external_runtime_id,
   started_event_id, last_event_id)
  VALUES
  (${household_id}, '${GRAPH}', 'classic.westfall', 2, 'invasion', 77,
   ${start_event}, ${start_event});"
run_id="$(sql "SELECT id FROM fury_director_run WHERE household_id=${household_id} AND graph_key='${GRAPH}' ORDER BY id DESC LIMIT 1;")"

accept_event="$(make_event t29-contract-accept contract.board.accept.requested classic.westfall.defias.scout_report)"
sql "INSERT INTO fury_contract_instance
  (household_id, contract_key, director_run_id, status, accepted_event_id)
  VALUES
  (${household_id}, 'classic.westfall.defias.scout_report',
   ${run_id}, 3, ${accept_event});"
instance_id="$(sql "SELECT id FROM fury_contract_instance WHERE household_id=${household_id} AND contract_key='classic.westfall.defias.scout_report' ORDER BY id DESC LIMIT 1;")"

recon_event="$(make_event t29-recon-complete contract.completed classic.westfall.defias.scout_report)"
recon_event_2="$(make_event t29-recon-complete-duplicate contract.completed classic.westfall.defias.scout_report)"
sql "UPDATE fury_event
  SET subject_type='contract_instance',
      subject_id=${instance_id},
      source_system='fury.contracts'
  WHERE id IN (${recon_event},${recon_event_2});"
sql "UPDATE fury_contract_instance
  SET completed_event_id=${recon_event}
  WHERE id=${instance_id};"

resolved_contract_run="$(sql "SELECT i.director_run_id
  FROM fury_contract_instance i
  JOIN fury_director_run r ON r.id=i.director_run_id
  WHERE i.id=${instance_id}
    AND i.household_id=${household_id}
    AND i.contract_key='classic.westfall.defias.scout_report'
    AND r.graph_key='${GRAPH}'
    AND i.completed_event_id=${recon_event}
  LIMIT 1;")"
assert_eq "${run_id}" "${resolved_contract_run}"   "contract completion resolves its attached Director run"

duplicate_completion_run_count="$(sql "SELECT COUNT(*)
  FROM fury_contract_instance i
  JOIN fury_director_run r ON r.id=i.director_run_id
  WHERE i.id=${instance_id}
    AND i.household_id=${household_id}
    AND i.contract_key='classic.westfall.defias.scout_report'
    AND r.graph_key='${GRAPH}'
    AND i.completed_event_id=${recon_event_2};")"
assert_eq "0" "${duplicate_completion_run_count}"   "non-canonical duplicate completion event cannot resolve a score run"

wrong_contract_run_count="$(sql "SELECT COUNT(*)
  FROM fury_contract_instance i
  JOIN fury_director_run r ON r.id=i.director_run_id
  WHERE i.id=999999999
    AND i.household_id=${household_id}
    AND i.contract_key='classic.westfall.defias.scout_report'
    AND r.graph_key='${GRAPH}';")"
assert_eq "0" "${wrong_contract_run_count}"   "unknown contract instance cannot resolve a score run"

final_stage_event="$(make_event t29-final-stage defias.final_stage.participated "${GRAPH}")"
sql "UPDATE fury_event
  SET subject_type='living_world_runtime',
      subject_id=77,
      source_system='fury.defias'
  WHERE id=${final_stage_event};"
resolved_runtime_run="$(sql "SELECT id FROM fury_director_run
  WHERE household_id=${household_id}
    AND graph_key='${GRAPH}'
    AND external_runtime_id=77
  LIMIT 1;")"
assert_eq "${run_id}" "${resolved_runtime_run}"   "final-stage event resolves only the attached external runtime"

award_component() {
  local component="$1"
  local event_id="$2"
  sql "INSERT IGNORE INTO fury_director_score_award
    (director_run_id, graph_key, component_key, score_value, source_event_id)
    SELECT ${run_id}, '${GRAPH}', component_key, score_value, ${event_id}
    FROM fury_director_score_component
    WHERE graph_key='${GRAPH}' AND component_key='${component}' AND enabled=1;"
}

award_component recon "${recon_event}"
award_component recon "${recon_event_2}"

assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_director_score_award WHERE director_run_id=${run_id} AND component_key='recon';")"   "repeated component completion awards once"
assert_eq "10" "$(sql "SELECT score_value FROM fury_director_score_award WHERE director_run_id=${run_id} AND component_key='recon';")"   "award snapshots the component value"
assert_eq "${recon_event}" "$(sql "SELECT source_event_id FROM fury_director_score_award WHERE director_run_id=${run_id} AND component_key='recon';")"   "first source event remains authoritative"

sql "UPDATE fury_director_score_component
  SET score_value=99
  WHERE graph_key='${GRAPH}' AND component_key='recon';"

assert_eq "10" "$(sql "SELECT SUM(score_value) FROM fury_director_score_award WHERE director_run_id=${run_id};")"   "later definition tuning cannot rewrite an awarded score snapshot"

# Restore the definition and verify repeatable migration does not overwrite
# operator-owned tuning/value rows because the seed uses INSERT IGNORE.
sql "UPDATE fury_director_score_component
  SET score_value=10
  WHERE graph_key='${GRAPH}' AND component_key='recon';"
"${mysql_cmd[@]}" < "${ROOT}/modules/mod-fury/data/sql/fury/updates/2026_09_23_11_defias_score.sql"
"${mysql_cmd[@]}" < "${ROOT}/modules/mod-fury/data/sql/fury/updates/2026_09_23_11_defias_score.sql"

assert_eq "7" "$(sql "SELECT COUNT(*) FROM fury_director_score_component WHERE graph_key='${GRAPH}';")"   "score migration is idempotent"
assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_director_score_award WHERE director_run_id=${run_id};")"   "score migration preserves runtime awards"

PARTICIPATION="${ROOT}/modules/mod-fury/src/content/defias/DefiasParticipation.cpp"
SCORE_SERVICE="${ROOT}/modules/mod-fury/src/content/defias/DefiasScoreService.cpp"
DATABASE="${ROOT}/modules/mod-fury/src/database/FuryDatabase.cpp"
CONFIG="${ROOT}/modules/mod-fury/conf/mod_fury.conf.dist"

grep -Fq 'defias.final_stage.participated' "${PARTICIPATION}"
grep -Fq 'runtime.stageId != 1006' "${PARTICIPATION}"
grep -Fq 'defias:final-stage-participation:v1:' "${PARTICIPATION}"

grep -Fq 'ResolveRunForEvent' "${SCORE_SERVICE}"
if grep -Fq 'FindLatestGraph' "${SCORE_SERVICE}"; then
  echo "[FURY][FAIL] score consumer still uses latest-run attribution" >&2
  exit 1
fi
grep -Fq 'FURY_SEL_DIRECTOR_SCORE_RUN_FOR_CONTRACT' "${DATABASE}"
grep -Fq 'FURY_SEL_DIRECTOR_SCORE_RUN_FOR_RUNTIME' "${DATABASE}"
grep -Fq 'i.completed_event_id = ?' "${DATABASE}"
grep -Fq 'event.sourceSystem == "fury.contracts"' "${ROOT}/modules/mod-fury/src/director/DirectorScoreRepository.cpp"
grep -Fq 'event.sourceSystem == "fury.defias"' "${ROOT}/modules/mod-fury/src/director/DirectorScoreRepository.cpp"
grep -Fq 'Fury.Defias.Score.PartialThreshold = 30' "${CONFIG}"
grep -Fq 'Fury.Defias.Score.SuccessThreshold = 70' "${CONFIG}"

echo "[FURY][PASS] T29 Defias score schema/replay gate passed"
