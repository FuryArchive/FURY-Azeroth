#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

export MYSQL_HOST="${MYSQL_HOST:-127.0.0.1}"
export MYSQL_PORT="${MYSQL_PORT:-3306}"
export MYSQL_USER="${MYSQL_USER:-root}"
export MYSQL_PASSWORD="${MYSQL_PASSWORD:-fury}"
export MYSQL_DATABASE="${MYSQL_DATABASE:-acore_fury}"
export MYSQL_PWD="${MYSQL_PASSWORD}"

bash "${ROOT}/scripts/test-t25-defias-graph.sh"
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

  echo "[FURY][PASS] ${label}" >&2
}

GRAPH="classic.westfall.defias_resurgence.v1"
CAMPAIGN="campaign.classic.westfall"

sql "INSERT INTO fury_household (slug, display_name)
     VALUES ('gamma','Gamma')
     ON DUPLICATE KEY UPDATE display_name=VALUES(display_name);"

alpha="$(sql "SELECT id FROM fury_household WHERE slug='alpha';")"
beta="$(sql "SELECT id FROM fury_household WHERE slug='beta';")"
gamma="$(sql "SELECT id FROM fury_household WHERE slug='gamma';")"

make_event() {
  local identity="$1"
  local event_type="$2"
  local household="$3"
  local actor_kind="$4"
  local subject_type="${5:-}"
  local subject_id="${6:-NULL}"
  local correlation="${7:-}"
  local source_system="${8:-golden.t32}"

  local subject_type_sql="NULL"
  local subject_id_sql="NULL"
  local correlation_sql="NULL"

  if [[ -n "${subject_type}" ]]; then
    subject_type_sql="'${subject_type}'"
  fi
  if [[ "${subject_id}" != "NULL" ]]; then
    subject_id_sql="${subject_id}"
  fi
  if [[ -n "${correlation}" ]]; then
    correlation_sql="'${correlation}'"
  fi

  sql "INSERT IGNORE INTO fury_event
    (event_type, actor_kind, household_id, subject_type, subject_id,
     source_system, correlation_key, dedupe_key, payload)
    VALUES
    ('${event_type}', ${actor_kind}, ${household},
     ${subject_type_sql}, ${subject_id_sql},
     '${source_system}', ${correlation_sql},
     UNHEX(SHA2('${identity}',256)), JSON_OBJECT());"

  sql "SELECT id FROM fury_event
    WHERE dedupe_key=UNHEX(SHA2('${identity}',256));"
}

seed_run() {
  local household="$1"
  local suffix="$2"

  local start_event
  start_event="$(make_event "t32-start-${suffix}" "player.zone.changed" "${household}" 1 "" NULL "" "azerothcore")"

  sql "INSERT INTO fury_director_run
    (household_id, graph_key, scope_key, status, phase_key,
     started_event_id, last_event_id, revision)
    VALUES
    (${household}, '${GRAPH}', 'classic.westfall', 2, 'resolution',
     ${start_event}, ${start_event}, 4);"

  sql "SELECT id FROM fury_director_run
    WHERE household_id=${household}
      AND graph_key='${GRAPH}'
    ORDER BY id DESC LIMIT 1;"
}

bind_terminal() {
  local household="$1"
  local run="$2"
  local outcome="$3"
  local suffix="$4"

  local source_event
  source_event="$(make_event "t32-resolve-source-${suffix}" "living_world.runtime.observed" "${household}" 4 "living_world_runtime" 900 "invasion:1" "living_world")"

  sql "UPDATE fury_director_run
    SET status=4,
        outcome_key='${outcome}',
        resolved_event_id=NULL,
        last_event_id=${source_event},
        revision=revision+1
    WHERE id=${run}
      AND household_id=${household}
      AND status IN (1,2,3);"

  local terminal
  terminal="$(make_event "director:terminal:v1:${run}" "director.run.resolved" "${household}" 4 "director_run" "${run}" "${GRAPH}" "fury.director")"

  sql "UPDATE fury_director_run
    SET resolved_event_id=${terminal},
        last_event_id=${terminal}
    WHERE id=${run}
      AND household_id=${household}
      AND status=4
      AND resolved_event_id IS NULL;"

  sql "UPDATE fury_director_run
    SET resolved_event_id=${terminal},
        last_event_id=${terminal}
    WHERE id=${run}
      AND household_id=${household}
      AND status=4
      AND resolved_event_id IS NULL;"

  assert_eq "${terminal}" "$(sql "SELECT resolved_event_id
    FROM fury_director_run WHERE id=${run};")"     "canonical director.run.resolved id is stable for ${outcome}"

  echo "${terminal}"
}

ensure_campaign_active() {
  local household="$1"
  local source_event="$2"

  sql "INSERT IGNORE INTO fury_campaign_state
    (household_id, node_key, status, source_event_id, revision)
    VALUES
    (${household}, '${CAMPAIGN}', 3, ${source_event}, 0);"
}

record_chronicle() {
  local household="$1"
  local event_id="$2"
  local entry_key="$3"
  local title="$4"

  sql "INSERT IGNORE INTO fury_chronicle_entry
    (household_id, entry_key, category, title,
     source_event_id, occurred_at, metadata)
    SELECT ${household}, '${entry_key}', 'world', '${title}',
           id, occurred_at, payload
    FROM fury_event WHERE id=${event_id};"

  sql "INSERT IGNORE INTO fury_chronicle_entry
    (household_id, entry_key, category, title,
     source_event_id, occurred_at, metadata)
    SELECT ${household}, '${entry_key}', 'world', '${title}',
           id, occurred_at, payload
    FROM fury_event WHERE id=${event_id};"
}

echo "[FURY] Success outcome"
success_run="$(seed_run "${alpha}" success)"
success_terminal="$(bind_terminal "${alpha}" "${success_run}" success success)"
ensure_campaign_active "${alpha}" "${success_terminal}"

success_metadata="{\"run_id\":${success_run},\"outcome_key\":\"success\",\"score\":75}"
sql "INSERT IGNORE INTO fury_proof
  (household_id, proof_key, source_event_id, metadata)
  VALUES
  (${alpha}, 'world.westfall.defended', ${success_terminal}, '${success_metadata}');"
sql "INSERT IGNORE INTO fury_proof
  (household_id, proof_key, source_event_id, metadata)
  VALUES
  (${alpha}, 'world.westfall.defended', ${success_terminal}, '${success_metadata}');"

sql "UPDATE fury_campaign_state
  SET status=4,
      completed_at=COALESCE(completed_at,CURRENT_TIMESTAMP(6)),
      source_event_id=${success_terminal},
      revision=revision+1
  WHERE household_id=${alpha}
    AND node_key='${CAMPAIGN}'
    AND status=3;"

sql "INSERT IGNORE INTO fury_reward_claim
  (source_event_id, reward_key, beneficiary_kind, beneficiary_id, status)
  VALUES
  (${success_terminal}, 'classic.westfall.defias.success', 3, ${alpha}, 0);"
sql "INSERT IGNORE INTO fury_reward_claim
  (source_event_id, reward_key, beneficiary_kind, beneficiary_id, status)
  VALUES
  (${success_terminal}, 'classic.westfall.defias.success', 3, ${alpha}, 0);"

success_outcome="$(make_event "defias:resolution:v1:${success_run}" "defias.resolution.success" "${alpha}" 1 "director_run" "${success_run}" "${GRAPH}" "fury.defias")"
record_chronicle "${alpha}" "${success_outcome}"   "classic.westfall.defias.resolution.success" "Westfall defended"

assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_proof
  WHERE household_id=${alpha}
    AND proof_key='world.westfall.defended';")"   "Success proof is replay-idempotent"

assert_eq "4" "$(sql "SELECT status FROM fury_campaign_state
  WHERE household_id=${alpha}
    AND node_key='${CAMPAIGN}';")"   "Success completes the Westfall campaign"

assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_reward_claim
  WHERE source_event_id=${success_terminal}
    AND reward_key='classic.westfall.defias.success'
    AND beneficiary_kind=3
    AND beneficiary_id=${alpha};")"   "Success reward entitlement is unique"

assert_eq "0" "$(sql "SELECT status FROM fury_reward_claim
  WHERE source_event_id=${success_terminal}
    AND reward_key='classic.westfall.defias.success'
    AND beneficiary_kind=3
    AND beneficiary_id=${alpha};")"   "Success reward remains pending"

assert_eq "0" "$(sql "SELECT COUNT(*) FROM fury_reward_entry
  WHERE reward_key='classic.westfall.defias.success';")"   "valuable physical reward is withheld until receipt-backed delivery exists"

assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_chronicle_entry
  WHERE household_id=${alpha}
    AND entry_key='classic.westfall.defias.resolution.success'
    AND source_event_id=${success_outcome};")"   "Success Chronicle projection is replay-idempotent"

echo "[FURY] Partial outcome"
partial_run="$(seed_run "${beta}" partial)"
partial_terminal="$(bind_terminal "${beta}" "${partial_run}" partial partial)"
ensure_campaign_active "${beta}" "${partial_terminal}"

partial_metadata="{\"run_id\":${partial_run},\"outcome_key\":\"partial\",\"score\":30}"
sql "INSERT IGNORE INTO fury_proof
  (household_id, proof_key, source_event_id, metadata)
  VALUES
  (${beta}, 'world.westfall.bloodied', ${partial_terminal}, '${partial_metadata}');"
sql "INSERT IGNORE INTO fury_proof
  (household_id, proof_key, source_event_id, metadata)
  VALUES
  (${beta}, 'world.westfall.bloodied', ${partial_terminal}, '${partial_metadata}');"

partial_outcome="$(make_event "defias:resolution:v1:${partial_run}" "defias.resolution.partial" "${beta}" 1 "director_run" "${partial_run}" "${GRAPH}" "fury.defias")"
record_chronicle "${beta}" "${partial_outcome}"   "classic.westfall.defias.resolution.partial" "Westfall bloodied"

assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_proof
  WHERE household_id=${beta}
    AND proof_key='world.westfall.bloodied';")"   "Partial proof is replay-idempotent"

assert_eq "3" "$(sql "SELECT status FROM fury_campaign_state
  WHERE household_id=${beta}
    AND node_key='${CAMPAIGN}';")"   "Partial keeps the campaign active"

assert_eq "0" "$(sql "SELECT COUNT(*) FROM fury_reward_claim
  WHERE beneficiary_kind=3
    AND beneficiary_id=${beta}
    AND reward_key='classic.westfall.defias.success';")"   "Partial does not create Success reward"

assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_chronicle_entry
  WHERE household_id=${beta}
    AND entry_key='classic.westfall.defias.resolution.partial'
    AND source_event_id=${partial_outcome};")"   "Partial Chronicle projection is replay-idempotent"

retry_event="$(make_event "t32-partial-retry" "player.zone.changed" "${beta}" 1 "" NULL "" "azerothcore")"
sql "INSERT INTO fury_director_run
  (household_id, graph_key, scope_key, status, phase_key,
   started_event_id, last_event_id, revision)
  VALUES
  (${beta}, '${GRAPH}', 'classic.westfall', 2, 'rumours',
   ${retry_event}, ${retry_event}, 0);"
assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_director_run
  WHERE household_id=${beta}
    AND graph_key='${GRAPH}'
    AND status IN (1,2,3);")"   "Partial leaves room for a later Defias attempt"

echo "[FURY] Ignored outcome"
ignored_run="$(seed_run "${gamma}" ignored)"
ignored_terminal="$(bind_terminal "${gamma}" "${ignored_run}" ignored ignored)"
ensure_campaign_active "${gamma}" "${ignored_terminal}"

ignored_metadata="{\"run_id\":${ignored_run},\"outcome_key\":\"ignored\",\"score\":10}"
sql "INSERT IGNORE INTO fury_proof
  (household_id, proof_key, source_event_id, metadata)
  VALUES
  (${gamma}, 'world.westfall.emergency', ${ignored_terminal}, '${ignored_metadata}');"
sql "INSERT IGNORE INTO fury_proof
  (household_id, proof_key, source_event_id, metadata)
  VALUES
  (${gamma}, 'world.westfall.emergency', ${ignored_terminal}, '${ignored_metadata}');"

pressure_event="$(make_event "defias:pressure:increased:v1:${ignored_run}" "defias.pressure.increased" "${gamma}" 1 "director_run" "${ignored_run}" "classic.westfall" "fury.defias")"
pressure_event_replay="$(make_event "defias:pressure:increased:v1:${ignored_run}" "defias.pressure.increased" "${gamma}" 1 "director_run" "${ignored_run}" "classic.westfall" "fury.defias")"

ignored_outcome="$(make_event "defias:resolution:v1:${ignored_run}" "defias.resolution.ignored" "${gamma}" 1 "director_run" "${ignored_run}" "${GRAPH}" "fury.defias")"
record_chronicle "${gamma}" "${ignored_outcome}"   "classic.westfall.defias.resolution.ignored" "Westfall crisis ignored"

assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_proof
  WHERE household_id=${gamma}
    AND proof_key='world.westfall.emergency';")"   "Ignored emergency flag is replay-idempotent"

assert_eq "${pressure_event}" "${pressure_event_replay}"   "Ignored pressure event dedupes on replay"

assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_event
  WHERE id=${pressure_event}
    AND event_type='defias.pressure.increased';")"   "Ignored records one pressure increase"

assert_eq "3" "$(sql "SELECT status FROM fury_campaign_state
  WHERE household_id=${gamma}
    AND node_key='${CAMPAIGN}';")"   "Ignored keeps the campaign active"

assert_eq "0" "$(sql "SELECT COUNT(*) FROM fury_reward_claim
  WHERE beneficiary_kind=3
    AND beneficiary_id=${gamma}
    AND reward_key='classic.westfall.defias.success';")"   "Ignored does not create Success reward"

assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_chronicle_entry
  WHERE household_id=${gamma}
    AND entry_key='classic.westfall.defias.resolution.ignored'
    AND source_event_id=${ignored_outcome};")"   "Ignored Chronicle projection is replay-idempotent"

SOURCE="${ROOT}/modules/mod-fury/src/content/defias/DefiasResolutionService.cpp"
DIRECTOR="${ROOT}/modules/mod-fury/src/director/DirectorService.cpp"
CHRONICLE="${ROOT}/modules/mod-fury/src/chronicle/ChronicleService.cpp"

grep -Fq 'world.westfall.defended' "${ROOT}/modules/mod-fury/src/content/defias/DefiasResolutionService.h"
grep -Fq 'world.westfall.bloodied' "${ROOT}/modules/mod-fury/src/content/defias/DefiasResolutionService.h"
grep -Fq 'world.westfall.emergency' "${ROOT}/modules/mod-fury/src/content/defias/DefiasResolutionService.h"
grep -Fq 'classic.westfall.defias.success' "${ROOT}/modules/mod-fury/src/content/defias/DefiasResolutionService.h"
grep -Fq 'EnsureCampaignActive' "${SOURCE}"
grep -Fq 'EmitPressureIncrease' "${SOURCE}"
grep -Fq 'BindTerminalEvent' "${DIRECTOR}"
grep -Fq 'defias.resolution.success' "${CHRONICLE}"
grep -Fq 'defias.resolution.partial' "${CHRONICLE}"
grep -Fq 'defias.resolution.ignored' "${CHRONICLE}"

"${mysql_cmd[@]}" < "${ROOT}/modules/mod-fury/data/sql/fury/updates/2026_09_24_03_defias_resolution.sql"
"${mysql_cmd[@]}" < "${ROOT}/modules/mod-fury/data/sql/fury/updates/2026_09_24_03_defias_resolution.sql"

assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_reward_bundle
  WHERE reward_key='classic.westfall.defias.success';")"   "T32 reward migration is idempotent"

echo "[FURY][PASS] T32 persistent Defias resolution gate passed"
