#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export MYSQL_HOST="${MYSQL_HOST:-127.0.0.1}" MYSQL_PORT="${MYSQL_PORT:-3306}"
export MYSQL_USER="${MYSQL_USER:-root}" MYSQL_PASSWORD="${MYSQL_PASSWORD:-fury}" MYSQL_DATABASE="${MYSQL_DATABASE:-acore_fury}"
export MYSQL_PWD="${MYSQL_PASSWORD}"
bash "${ROOT}/scripts/test-m2-director-schema.sh"
mysql_cmd=(mysql --protocol=tcp -h "$MYSQL_HOST" -P "$MYSQL_PORT" -u "$MYSQL_USER" --batch --skip-column-names "$MYSQL_DATABASE")
sql() { "${mysql_cmd[@]}" -e "$1"; }
assert_eq() { [[ "$1" == "$2" ]] || { echo "FAIL $3: expected $1 got $2"; exit 1; }; echo "[FURY][PASS] $3"; }
# T16 gate leaves the old Director schema: exercise a real forward upgrade.
for pass in 1 2; do
  "${mysql_cmd[@]}" < "${ROOT}/modules/mod-fury/data/sql/fury/updates/2026_09_23_00_defias_graph.sql"
done
"${mysql_cmd[@]}" < "${ROOT}/modules/mod-fury/data/sql/fury/updates/2026_09_23_08_defias_director.sql"
"${mysql_cmd[@]}" < "${ROOT}/modules/mod-fury/data/sql/fury/updates/2026_09_23_09_defias_retire_prototype.sql"
assert_eq 0 "$(sql "SELECT enabled FROM fury_director_graph WHERE graph_key='defias.resurgence';")" "prototype retained but retired"
assert_eq 1 "$(sql "SELECT COUNT(*) FROM fury_director_graph WHERE graph_key='classic.westfall.defias_resurgence.v1' AND enabled=1;")" "idempotent graph seed"

# Historical T25 upgrades were deliberately one-shot. T32 must remove this on
# upgraded databases so Partial/Ignored can be attempted again.
assert_eq 2 "$(sql "SELECT COUNT(*) FROM information_schema.statistics WHERE table_schema=DATABASE() AND table_name='fury_director_run' AND index_name='uq_fury_defias_once';")" "historical T25 one-shot index is present before T32 migration"

household="$(sql "SELECT id FROM fury_household WHERE slug='alpha';")"
event="$(sql 'SELECT MIN(id) FROM fury_event;')"
insert="INSERT IGNORE INTO fury_director_run (household_id,graph_key,scope_key,status,phase_key,started_event_id,last_event_id) VALUES ($household,'classic.westfall.defias_resurgence.v1','classic.westfall',2,'rumours',$event,$event);"
sql "$insert $insert"
assert_eq 1 "$(sql "SELECT COUNT(*) FROM fury_director_run WHERE graph_key='classic.westfall.defias_resurgence.v1';")" "duplicate simultaneous Westfall triggers create one active run"
sql "UPDATE fury_director_run SET phase_key='invasion',external_runtime_id=42 WHERE graph_key='classic.westfall.defias_resurgence.v1';"
assert_eq 'invasion:42' "$(sql "SELECT CONCAT(phase_key,':',external_runtime_id) FROM fury_director_run WHERE graph_key='classic.westfall.defias_resurgence.v1';")" "fresh connection restores phase and runtime"

for pass in 1 2; do
  "${mysql_cmd[@]}" < "${ROOT}/modules/mod-fury/data/sql/fury/updates/2026_09_24_04_defias_repeatable_runs.sql"
done
assert_eq 0 "$(sql "SELECT COUNT(*) FROM information_schema.statistics WHERE table_schema=DATABASE() AND table_name='fury_director_run' AND index_name='uq_fury_defias_once';")" "T32 migration removes historical one-shot index"
assert_eq 0 "$(sql "SELECT COUNT(*) FROM information_schema.columns WHERE table_schema=DATABASE() AND table_name='fury_director_run' AND column_name='defias_once';")" "T32 migration removes historical generated column"

sql "UPDATE fury_director_run SET status=4,outcome_key='partial',phase_key='resolution' WHERE graph_key='classic.westfall.defias_resurgence.v1' AND status IN (1,2,3);"
sql "$insert"
assert_eq 2 "$(sql "SELECT COUNT(*) FROM fury_director_run WHERE graph_key='classic.westfall.defias_resurgence.v1';")" "Partial allows a later Defias run"
assert_eq 1 "$(sql "SELECT COUNT(*) FROM fury_director_run WHERE graph_key='classic.westfall.defias_resurgence.v1' AND status IN (1,2,3);")" "Partial still preserves one active run per scope"

sql "UPDATE fury_director_run SET status=4,outcome_key='ignored',phase_key='resolution' WHERE graph_key='classic.westfall.defias_resurgence.v1' AND status IN (1,2,3);"
sql "$insert"
assert_eq 3 "$(sql "SELECT COUNT(*) FROM fury_director_run WHERE graph_key='classic.westfall.defias_resurgence.v1';")" "Ignored allows a later Defias run"
assert_eq 1 "$(sql "SELECT COUNT(*) FROM fury_director_run WHERE graph_key='classic.westfall.defias_resurgence.v1' AND status IN (1,2,3);")" "Ignored still preserves one active run per scope"

# A clean T32-era base never creates the retired one-shot column/index.
sql "DROP TABLE IF EXISTS fury_director_score_award; DROP TABLE IF EXISTS fury_director_score_component; DELETE FROM fury_director_run; DROP TABLE fury_director_run;"
"${mysql_cmd[@]}" < "${ROOT}/modules/mod-fury/data/sql/fury/base/70_director.sql"
assert_eq 0 "$(sql "SELECT COUNT(*) FROM information_schema.statistics WHERE table_schema=DATABASE() AND table_name='fury_director_run' AND index_name='uq_fury_defias_once';")" "clean base has no retired one-shot index"
assert_eq 0 "$(sql "SELECT COUNT(*) FROM information_schema.columns WHERE table_schema=DATABASE() AND table_name='fury_director_run' AND column_name='defias_once';")" "clean base has no retired one-shot column"
echo '[FURY][PASS] T25/T32 migration/replay/restart schema gate'
