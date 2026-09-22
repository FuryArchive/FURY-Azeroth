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

bash "${ROOT}/scripts/test-m1-schema.sh"

mysql_cmd=(
  mysql
  --protocol=tcp
  --host="${MYSQL_HOST}"
  --port="${MYSQL_PORT}"
  --user="${MYSQL_USER}"
  --batch
  --skip-column-names
  "${MYSQL_DATABASE}"
)

sql() {
  "${mysql_cmd[@]}" -e "$1"
}

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

for table in fury_campaign_node fury_campaign_state fury_proof; do
  present="$(sql "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema = '${MYSQL_DATABASE}' AND table_name = '${table}';")"
  assert_eq "1" "${present}" "table ${table} exists"
done

echo "[FURY] campaign state and optimistic revision"
sql "INSERT INTO fury_campaign_node (node_key, era, ordinal, display_name, required_power_band, grants_power_band, enabled) VALUES ('golden.campaign', 1, 1, 'Golden Campaign', 0, 110, 1);"
sql "INSERT INTO fury_campaign_state (household_id, node_key, status, source_event_id, revision) VALUES (1, 'golden.campaign', 2, 1, 0);"

sql "UPDATE fury_campaign_state SET status=3, revision=revision+1 WHERE household_id=1 AND node_key='golden.campaign' AND revision=0;"
assert_eq "3" "$(sql "SELECT status FROM fury_campaign_state WHERE household_id=1 AND node_key='golden.campaign';")" "campaign transition applies"
assert_eq "1" "$(sql "SELECT revision FROM fury_campaign_state WHERE household_id=1 AND node_key='golden.campaign';")" "campaign revision advances"

sql "UPDATE fury_campaign_state SET status=4, revision=revision+1 WHERE household_id=1 AND node_key='golden.campaign' AND revision=0;"
assert_eq "3" "$(sql "SELECT status FROM fury_campaign_state WHERE household_id=1 AND node_key='golden.campaign';")" "stale campaign revision cannot overwrite state"

echo "[FURY] household power band monotonicity"
sql "UPDATE fury_household SET current_power_band=110 WHERE id=1;"
sql "UPDATE fury_household SET current_power_band=GREATEST(current_power_band, 100) WHERE id=1;"
assert_eq "110" "$(sql "SELECT current_power_band FROM fury_household WHERE id=1;")" "power band never moves backwards"
sql "UPDATE fury_household SET current_power_band=GREATEST(current_power_band, 120) WHERE id=1;"
assert_eq "120" "$(sql "SELECT current_power_band FROM fury_household WHERE id=1;")" "power band can advance"

echo "[FURY] durable household proofs"
sql "INSERT IGNORE INTO fury_proof (household_id, proof_key, source_event_id, metadata) VALUES (1, 'golden.proof', 1, JSON_OBJECT('pass', 1));"
sql "INSERT IGNORE INTO fury_proof (household_id, proof_key, source_event_id, metadata) VALUES (1, 'golden.proof', 1, JSON_OBJECT('pass', 2));"
assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_proof WHERE household_id=1 AND proof_key='golden.proof';")" "proof grant is idempotent"
assert_eq "1" "$(sql "SELECT source_event_id FROM fury_proof WHERE household_id=1 AND proof_key='golden.proof';")" "proof retains source event"

echo "[FURY] base schema remains re-applicable with M2 state"
SQL_BASE="${ROOT}/modules/mod-fury/data/sql/fury/base"
while IFS= read -r file; do
  "${mysql_cmd[@]}" < "${file}"
done < <(find "${SQL_BASE}" -maxdepth 1 -type f -name '*.sql' -print | sort)

assert_eq "3" "$(sql "SELECT status FROM fury_campaign_state WHERE household_id=1 AND node_key='golden.campaign';")" "schema re-apply preserves campaign state"
assert_eq "1" "$(sql "SELECT COUNT(*) FROM fury_proof WHERE household_id=1 AND proof_key='golden.proof';")" "schema re-apply preserves proof"

echo "[FURY] M2 campaign/proof schema golden gate passed"
