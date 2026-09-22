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

bash "${ROOT}/scripts/test-m2-bestiary-schema.sh"

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

echo "[FURY] campaign node exposes explicit IP requirement"
assert_eq "1" "$(sql "SELECT COUNT(*) FROM information_schema.columns WHERE table_schema='${MYSQL_DATABASE}' AND table_name='fury_campaign_node' AND column_name='ip_required_state';")" "ip_required_state column exists"

sql "INSERT INTO fury_campaign_node (node_key,era,ordinal,display_name,required_power_band,grants_power_band,ip_required_state,enabled) VALUES ('golden.ip.node',1,99,'Golden IP Gate',0,0,3,1);"
assert_eq "3" "$(sql "SELECT ip_required_state FROM fury_campaign_node WHERE node_key='golden.ip.node';")" "campaign node stores character IP requirement"

echo "[FURY] campaign schema re-apply preserves IP requirement"
SQL_BASE="${ROOT}/modules/mod-fury/data/sql/fury/base"
while IFS= read -r file; do
  "${mysql_cmd[@]}" < "${file}"
done < <(find "${SQL_BASE}" -maxdepth 1 -type f -name '*.sql' -print | sort)

assert_eq "3" "$(sql "SELECT ip_required_state FROM fury_campaign_node WHERE node_key='golden.ip.node';")" "schema re-apply preserves IP gate"

echo "[FURY] Individual Progression adapter remains read-only"
ADAPTER="${ROOT}/modules/mod-fury/src/integrations/IndividualProgressionAdapter.cpp"

for forbidden in "UpdateProgressionState" "ForceUpdateProgressionState" "UpdatePlayerSetting"; do
  if grep -Fq "${forbidden}" "${ADAPTER}"; then
    echo "[FURY][FAIL] adapter contains forbidden IP mutation API: ${forbidden}" >&2
    exit 1
  fi
done

grep -Fq "GetPlayerProgressionFromQuests" "${ADAPTER}"
grep -Fq "hasPassedProgression" "${ADAPTER}"
grep -Fq "CONFIG_PLAYER_SETTINGS_ENABLED" "${ADAPTER}"

echo "[FURY][PASS] adapter uses read-only IP APIs and validates Player Settings"
echo "[FURY] M2 Individual Progression adapter gate passed"
