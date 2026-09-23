#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

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
COMMANDER="classic.westfall.defias.commander"

assert_eq "1" "$(sql "SELECT COUNT(*) FROM information_schema.tables
  WHERE table_schema='${MYSQL_DATABASE}'
    AND table_name='fury_bestiary_event_map';")"   "Bestiary event map table exists"

assert_eq "3" "$(sql "SELECT COUNT(*) FROM fury_bestiary_entry
  WHERE entry_key LIKE 'classic.westfall.defias.%';")"   "three Defias Bestiary entries are seeded"

assert_eq "6" "$(sql "SELECT COUNT(*) FROM fury_bestiary_event_map
  WHERE event_type='defias.bestiary.entity.participated'
    AND subject_type='living_world_spawn_group'
    AND subject_id BETWEEN 100 AND 105
    AND entry_key LIKE 'classic.westfall.defias.%'
    AND discovery_level=2
    AND enabled=1;")"   "all six hostile runtime groups map explicitly to Studied"

assert_eq "0" "$(sql "SELECT COUNT(*) FROM fury_bestiary_creature_map
  WHERE entry_key LIKE 'classic.westfall.defias.%';")"   "ordinary creature-entry mappings cannot advance Defias Bestiary"

assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_bestiary_event_map
  WHERE subject_id=105
    AND entry_key='${COMMANDER}'
    AND discovery_level=2;")"   "leadership runtime group maps Captain Garrick Vane to Studied"

household_id="$(sql "SELECT id FROM fury_household WHERE slug='alpha' LIMIT 1;")"
sql "INSERT IGNORE INTO fury_household_member
  (household_id, account_id, role)
  VALUES (${household_id}, 1002, 1);"

sql "INSERT INTO fury_event
  (event_type, actor_kind, account_id, household_id,
   subject_type, subject_id, source_system, correlation_key,
   dedupe_key, payload)
  VALUES
  ('defias.bestiary.entity.participated', 1, 1001, ${household_id},
   'living_world_spawn_group', 105, 'fury.defias', '${GRAPH}',
   UNHEX(SHA2('t30-commander-runtime-kill',256)),
   JSON_OBJECT('runtime_id',77,'spawn_group_id',105));"
kill_event="$(sql "SELECT id FROM fury_event
  WHERE dedupe_key=UNHEX(SHA2('t30-commander-runtime-kill',256));")"

sql "INSERT INTO fury_bestiary_state
  (account_id, entry_key, discovery_level, kill_count,
   first_event_id, last_event_id, revision)
  SELECT 1001, m.entry_key, m.discovery_level, 1,
         ${kill_event}, ${kill_event}, 0
  FROM fury_bestiary_event_map m
  WHERE m.event_type='defias.bestiary.entity.participated'
    AND m.subject_type='living_world_spawn_group'
    AND m.subject_id=105
    AND m.entry_key='${COMMANDER}'
    AND m.enabled=1;"

assert_eq "2" "$(sql "SELECT discovery_level
  FROM fury_bestiary_state
  WHERE account_id=1001 AND entry_key='${COMMANDER}';")"   "runtime commander participation produces Studied"
assert_eq "1" "$(sql "SELECT kill_count
  FROM fury_bestiary_state
  WHERE account_id=1001 AND entry_key='${COMMANDER}';")"   "runtime commander kill is counted once"
assert_eq "0" "$(sql "SELECT COUNT(*) FROM fury_bestiary_state
  WHERE account_id=1002 AND entry_key='${COMMANDER}';")"   "household member without commander participation remains Unknown"

sql "INSERT INTO fury_event
  (event_type, actor_kind, household_id, source_system,
   correlation_key, dedupe_key, payload)
  VALUES
  ('golden.defias.start', 5, ${household_id}, 'golden.t30',
   '${GRAPH}', UNHEX(SHA2('t30-director-start',256)), JSON_OBJECT());"
start_event="$(sql "SELECT id FROM fury_event
  WHERE dedupe_key=UNHEX(SHA2('t30-director-start',256));")"

sql "INSERT INTO fury_director_run
  (household_id, graph_key, scope_key, status, phase_key,
   external_runtime_id, started_event_id, last_event_id)
  VALUES
  (${household_id}, '${GRAPH}', 'classic.westfall', 2, 'resolution',
   77, ${start_event}, ${start_event});"
run_id="$(sql "SELECT id FROM fury_director_run
  WHERE household_id=${household_id} AND graph_key='${GRAPH}' LIMIT 1;")"

sql "INSERT INTO fury_event
  (event_type, actor_kind, household_id, subject_type, subject_id,
   source_system, correlation_key, dedupe_key, payload)
  VALUES
  ('director.run.resolved', 5, ${household_id},
   'director_run', ${run_id}, 'fury.director', '${GRAPH}',
   UNHEX(SHA2('t30-director-success',256)),
   JSON_OBJECT('run_id',${run_id},'outcome_key','success'));"
success_event="$(sql "SELECT id FROM fury_event
  WHERE dedupe_key=UNHEX(SHA2('t30-director-success',256));")"

sql "UPDATE fury_director_run
  SET status=4, phase_key='resolution',
      outcome_key='success',
      resolved_event_id=${success_event},
      last_event_id=${success_event}
  WHERE id=${run_id};"

eligible_accounts="$(sql "SELECT GROUP_CONCAT(s.account_id ORDER BY s.account_id)
  FROM fury_bestiary_state s
  JOIN fury_household_member h ON h.account_id=s.account_id
  WHERE h.household_id=${household_id}
    AND s.entry_key='${COMMANDER}'
    AND s.discovery_level>=2;")"
assert_eq "1001" "${eligible_accounts}"   "success mastery selects only household accounts that studied commander"

master_commander() {
  sql "INSERT INTO fury_bestiary_state
    (account_id, entry_key, discovery_level, kill_count,
     first_event_id, last_event_id, revision)
    VALUES
    (1001, '${COMMANDER}', 3, 0,
     ${success_event}, ${success_event}, 0)
    ON DUPLICATE KEY UPDATE
      discovery_level=IF(last_event_id<VALUES(last_event_id),
        GREATEST(discovery_level,VALUES(discovery_level)),discovery_level),
      revision=IF(last_event_id<VALUES(last_event_id),revision+1,revision),
      last_event_id=GREATEST(last_event_id,VALUES(last_event_id));"
}

master_commander
assert_eq "3" "$(sql "SELECT discovery_level FROM fury_bestiary_state
  WHERE account_id=1001 AND entry_key='${COMMANDER}';")"   "successful outcome promotes studied commander to Mastered"
revision_after_first="$(sql "SELECT revision FROM fury_bestiary_state
  WHERE account_id=1001 AND entry_key='${COMMANDER}';")"
master_commander
assert_eq "${revision_after_first}" "$(sql "SELECT revision FROM fury_bestiary_state
  WHERE account_id=1001 AND entry_key='${COMMANDER}';")"   "replayed success event does not mutate mastery twice"
assert_eq "0" "$(sql "SELECT COUNT(*) FROM fury_bestiary_state
  WHERE account_id=1002 AND entry_key='${COMMANDER}';")"   "success does not grant commander mastery without prior study"

"${mysql_cmd[@]}" < "${ROOT}/modules/mod-fury/data/sql/fury/updates/2026_09_24_00_bestiary_event_map.sql"
"${mysql_cmd[@]}" < "${ROOT}/modules/mod-fury/data/sql/fury/updates/2026_09_24_01_defias_bestiary.sql"
"${mysql_cmd[@]}" < "${ROOT}/modules/mod-fury/data/sql/fury/updates/2026_09_24_01_defias_bestiary.sql"

assert_eq "6" "$(sql "SELECT COUNT(*) FROM fury_bestiary_event_map
  WHERE entry_key LIKE 'classic.westfall.defias.%';")"   "Defias Bestiary migration is idempotent"

SERVICE="${ROOT}/modules/mod-fury/src/bestiary/BestiaryService.cpp"
grep -Fq 'defias.bestiary.entity.participated' "${SERVICE}"
grep -Fq 'event.sourceSystem == "fury.defias"' "${SERVICE}"
grep -Fq 'defias.bestiary.entity.participated' "${ROOT}/modules/mod-fury/src/content/defias/DefiasParticipation.cpp"
grep -Fq 'director.run.resolved' "${SERVICE}"
grep -Fq '*run->outcomeKey != "success"' "${SERVICE}"
grep -Fq 'FindHouseholdAccountsAtLeastLevel' "${SERVICE}"
grep -Fq 'CommanderBestiaryEntry' "${SERVICE}"

echo "[FURY][PASS] T30 Defias Bestiary gate passed"
