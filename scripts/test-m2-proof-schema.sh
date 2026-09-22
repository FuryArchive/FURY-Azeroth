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

bash "${ROOT}/scripts/test-m2-campaign-schema.sh"

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

echo "[FURY] fresh-install Proof schema"
assert_eq "1" "$(sql "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema='${MYSQL_DATABASE}' AND table_name='fury_proof';")" "fury_proof table exists"

echo "[FURY] T13 -> T14 migration path"
sql "DROP TABLE fury_proof;"
"${mysql_cmd[@]}" < "${ROOT}/modules/mod-fury/data/sql/fury/updates/2026_09_22_02_proofs.sql"
assert_eq "1" "$(sql "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema='${MYSQL_DATABASE}' AND table_name='fury_proof';")" "proof migration creates table"

echo "[FURY] proof lookup indexes"
assert_eq "2" "$(sql "SELECT COUNT(*) FROM information_schema.statistics WHERE table_schema='${MYSQL_DATABASE}' AND table_name='fury_proof' AND index_name='PRIMARY';")" "household/proof key composite primary index exists"
assert_eq "household_id,proof_key" "$(sql "SELECT GROUP_CONCAT(column_name ORDER BY seq_in_index) FROM information_schema.statistics WHERE table_schema='${MYSQL_DATABASE}' AND table_name='fury_proof' AND index_name='PRIMARY';")" "primary index supports household proof lookup"
assert_eq "1" "$(sql "SELECT COUNT(*) FROM information_schema.statistics WHERE table_schema='${MYSQL_DATABASE}' AND table_name='fury_proof' AND index_name='ix_fury_proof_event' AND column_name='source_event_id';")" "source event reverse index exists"

household_id="$(sql "SELECT id FROM fury_household WHERE slug='alpha' LIMIT 1;")"
source_one="$(sql "SELECT MIN(id) FROM fury_event;")"

sql "INSERT INTO fury_event
  (event_type, actor_kind, household_id, source_system, dedupe_key, payload)
  VALUES
  ('golden.proof.source', 1, ${household_id}, 'golden.proofs',
   UNHEX(SHA2('golden-proof-source-two', 256)), JSON_OBJECT('source', 2));"
source_two="$(sql "SELECT id FROM fury_event WHERE dedupe_key=UNHEX(SHA2('golden-proof-source-two', 256));")"

echo "[FURY] immutable idempotent proof grant"
sql "INSERT IGNORE INTO fury_proof
  (household_id, proof_key, source_event_id, metadata)
  VALUES
  (${household_id}, 'world.westfall.defended', ${source_one}, JSON_OBJECT('winner', 1));"

sql "INSERT IGNORE INTO fury_proof
  (household_id, proof_key, source_event_id, metadata)
  VALUES
  (${household_id}, 'world.westfall.defended', ${source_two}, JSON_OBJECT('winner', 2));"

assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_proof WHERE household_id=${household_id} AND proof_key='world.westfall.defended';")" "duplicate grant creates one durable proof"
assert_eq "${source_one}" "$(sql "SELECT source_event_id FROM fury_proof WHERE household_id=${household_id} AND proof_key='world.westfall.defended';")" "duplicate grant retains original source event"
assert_eq "1" "$(sql "SELECT JSON_UNQUOTE(JSON_EXTRACT(metadata, '$.winner')) FROM fury_proof WHERE household_id=${household_id} AND proof_key='world.westfall.defended';")" "duplicate grant retains original metadata"

echo "[FURY] same proof key is independent across households"
sql "INSERT INTO fury_household (slug, display_name) VALUES ('proof-secondary', 'Proof Secondary');"
secondary_household="$(sql "SELECT id FROM fury_household WHERE slug='proof-secondary';")"
sql "INSERT IGNORE INTO fury_proof
  (household_id, proof_key, source_event_id, metadata)
  VALUES
  (${secondary_household}, 'world.westfall.defended', ${source_two}, JSON_OBJECT('winner', 3));"
assert_eq "2" "$(sql "SELECT COUNT(*) FROM fury_proof WHERE proof_key='world.westfall.defended';")" "proof uniqueness is scoped per household"

echo "[FURY] schema re-apply preserves proofs"
"${mysql_cmd[@]}" < "${ROOT}/modules/mod-fury/data/sql/fury/base/51_proofs.sql"
"${mysql_cmd[@]}" < "${ROOT}/modules/mod-fury/data/sql/fury/updates/2026_09_22_02_proofs.sql"
assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_proof WHERE household_id=${household_id} AND proof_key='world.westfall.defended';")" "proof survives idempotent schema re-apply"
assert_eq "${source_one}" "$(sql "SELECT source_event_id FROM fury_proof WHERE household_id=${household_id} AND proof_key='world.westfall.defended';")" "source event survives schema re-apply"

echo "[FURY][PASS] T14 Proof schema golden gate passed"
