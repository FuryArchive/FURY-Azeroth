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
household="$(sql "SELECT id FROM fury_household WHERE slug='alpha';")"
event="$(sql 'SELECT MIN(id) FROM fury_event;')"
insert="INSERT IGNORE INTO fury_director_run (household_id,graph_key,scope_key,status,phase_key,started_event_id,last_event_id) VALUES ($household,'classic.westfall.defias_resurgence.v1','classic.westfall',2,'rumours',$event,$event);"
sql "$insert $insert"
assert_eq 1 "$(sql "SELECT COUNT(*) FROM fury_director_run WHERE graph_key='classic.westfall.defias_resurgence.v1';")" "duplicate Westfall triggers create one run"
sql "UPDATE fury_director_run SET phase_key='invasion',external_runtime_id=42 WHERE graph_key='classic.westfall.defias_resurgence.v1';"
assert_eq 'invasion:42' "$(sql "SELECT CONCAT(phase_key,':',external_runtime_id) FROM fury_director_run WHERE graph_key='classic.westfall.defias_resurgence.v1';")" "fresh connection restores phase and runtime"
for outcome in success partial ignored; do
  sql "UPDATE fury_director_run SET status=4,outcome_key='$outcome',phase_key='resolution' WHERE graph_key='classic.westfall.defias_resurgence.v1'; $insert"
  assert_eq 1 "$(sql "SELECT COUNT(*) FROM fury_director_run WHERE graph_key='classic.westfall.defias_resurgence.v1';")" "terminal $outcome cannot restart"
done
# Base install also contains the constraint (the T16 gate replaced its table).
sql "DELETE FROM fury_director_run; DROP TABLE fury_director_run;"
"${mysql_cmd[@]}" < "${ROOT}/modules/mod-fury/data/sql/fury/base/70_director.sql"
assert_eq 2 "$(sql "SELECT COUNT(*) FROM information_schema.statistics WHERE table_schema=DATABASE() AND table_name='fury_director_run' AND index_name='uq_fury_defias_once';")" "clean base has one-shot index"
echo '[FURY][PASS] T25 migration/replay/restart schema gate'
