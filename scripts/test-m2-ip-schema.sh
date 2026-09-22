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

assert_eq "1" "$(sql "SELECT COUNT(*) FROM information_schema.columns WHERE table_schema='${MYSQL_DATABASE}' AND table_name='fury_campaign_node' AND column_name='ip_required_state';")" "fresh schema contains IP character gate"

echo "[FURY] T18 -> T19 migration path"
sql "ALTER TABLE fury_campaign_node DROP COLUMN ip_required_state;"
assert_eq "0" "$(sql "SELECT COUNT(*) FROM information_schema.columns WHERE table_schema='${MYSQL_DATABASE}' AND table_name='fury_campaign_node' AND column_name='ip_required_state';")" "pre-T19 schema has no IP gate"

"${mysql_cmd[@]}" < "${ROOT}/modules/mod-fury/data/sql/fury/updates/2026_09_22_07_ip_campaign_gate.sql"
assert_eq "1" "$(sql "SELECT COUNT(*) FROM information_schema.columns WHERE table_schema='${MYSQL_DATABASE}' AND table_name='fury_campaign_node' AND column_name='ip_required_state';")" "T19 migration adds IP gate"

echo "[FURY] default nodes do not require Individual Progression"
sql "INSERT INTO fury_campaign_node
  (node_key,era,ordinal,display_name,required_power_band,grants_power_band,enabled)
  VALUES ('golden.ip.none',1,990,'No IP Requirement',0,0,1);"
assert_eq "0" "$(sql "SELECT ip_required_state FROM fury_campaign_node WHERE node_key='golden.ip.none';")" "default IP requirement is zero"

echo "[FURY] explicit character requirement persists independently of household state"
sql "INSERT INTO fury_campaign_node
  (node_key,era,ordinal,display_name,required_power_band,grants_power_band,ip_required_state,enabled)
  VALUES ('golden.ip.required',1,991,'IP Required',0,0,3,1);"
assert_eq "3" "$(sql "SELECT ip_required_state FROM fury_campaign_node WHERE node_key='golden.ip.required';")" "explicit IP requirement persists"

household_id="$(sql "SELECT id FROM fury_household WHERE slug='alpha' LIMIT 1;")"
source_event_id="$(sql "SELECT MIN(id) FROM fury_event WHERE household_id=${household_id};")"
sql "INSERT INTO fury_campaign_state
  (household_id,node_key,status,source_event_id,revision)
  VALUES (${household_id},'golden.ip.required',2,${source_event_id},0);"
assert_eq "2" "$(sql "SELECT status FROM fury_campaign_state WHERE household_id=${household_id} AND node_key='golden.ip.required';")" "household unlock remains separate canonical state"
assert_eq "3" "$(sql "SELECT ip_required_state FROM fury_campaign_node WHERE node_key='golden.ip.required';")" "character requirement remains separate from household unlock"

echo "[FURY] migration re-apply is idempotent"
"${mysql_cmd[@]}" < "${ROOT}/modules/mod-fury/data/sql/fury/updates/2026_09_22_07_ip_campaign_gate.sql"
assert_eq "3" "$(sql "SELECT ip_required_state FROM fury_campaign_node WHERE node_key='golden.ip.required';")" "migration re-apply preserves IP requirement"

echo "[FURY][PASS] T19 IP campaign schema gate passed"
