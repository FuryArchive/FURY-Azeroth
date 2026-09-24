#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ACCOUNT="${1:-}"
CHARACTER="${2:-}"
DB_PASSWORD="${FURY_MYSQL_PASSWORD:-fury}"
COMPOSE="${ROOT}/runtime/docker-compose.playable.yml"
CONF="${ROOT}/etc/modules/mod_ahbot.conf"
ENABLE_SELLER="${FURY_AHBOT_SELLER:-1}"
ENABLE_BUYER="${FURY_AHBOT_BUYER:-1}"

fail() { echo "[FURY][AHBOT][FAIL] $*" >&2; exit 1; }

[[ -n "${ACCOUNT}" && -n "${CHARACTER}" ]] ||   fail "usage: ./runtime/configure-ahbot.sh <dedicated-account> <dedicated-character>"
[[ "${ENABLE_SELLER}" =~ ^[01]$ ]] || fail "FURY_AHBOT_SELLER must be 0 or 1"
[[ "${ENABLE_BUYER}" =~ ^[01]$ ]] || fail "FURY_AHBOT_BUYER must be 0 or 1"
[[ -f "${CONF}" ]] || fail "AHBot config missing; bootstrap the server first"

if pgrep -x worldserver >/dev/null 2>&1 || pgrep -x authserver >/dev/null 2>&1; then
  fail "stop the realm before configuring AHBot"
fi

cd "${ROOT}"
docker compose -f "${COMPOSE}" up -d mysql

for _ in {1..90}; do
  if docker compose -f "${COMPOSE}" exec -T mysql \
      mysqladmin ping -h localhost -uroot -p"${DB_PASSWORD}" --silent >/dev/null 2>&1; then
    break
  fi
  sleep 1
done

mysql_exec() {
  docker compose -f "${COMPOSE}" exec -T mysql     mysql -uroot -p"${DB_PASSWORD}" "$@"
}

read -r account_sql character_sql < <(
  python3 - "${ACCOUNT}" "${CHARACTER}" <<'PY'
import sys
account = sys.argv[1].encode("utf-8").hex()
character = sys.argv[2].encode("utf-8").hex()
print(
    "SELECT id FROM account "
    f"WHERE username=UPPER(CONVERT(0x{account} USING utf8mb4)) LIMIT 1;"
)
print(
    "SELECT guid FROM characters "
    "WHERE account={ACCOUNT_ID} AND "
    f"name=CONVERT(0x{character} USING utf8mb4) LIMIT 1;"
)
PY
)

account_id="$(mysql_exec acore_auth -Nse "${account_sql}")"
[[ "${account_id}" =~ ^[1-9][0-9]*$ ]] || fail "account not found: ${ACCOUNT}"

character_sql="${character_sql//\{ACCOUNT_ID\}/${account_id}}"
character_guid="$(mysql_exec acore_characters -Nse "${character_sql}")"
[[ "${character_guid}" =~ ^[1-9][0-9]*$ ]] ||   fail "character '${CHARACTER}' was not found on account '${ACCOUNT}'"

python3 - "${CONF}" "${account_id}" "${character_guid}" "${ENABLE_SELLER}" "${ENABLE_BUYER}" <<'PY'
from pathlib import Path
import re
import sys

path = Path(sys.argv[1])
account_id, guid, seller, buyer = sys.argv[2:6]
text = path.read_text()

def set_option(key: str, value: str) -> None:
    global text
    line = f"{key} = {value}"
    pattern = re.compile(rf"(?m)^\s*{re.escape(key)}\s*=.*$")
    if pattern.search(text):
        text = pattern.sub(lambda _: line, text, count=1)
    else:
        text = text.rstrip() + "\n" + line + "\n"

set_option("AuctionHouseBot.Account", account_id)
set_option("AuctionHouseBot.GUID", guid)
set_option("AuctionHouseBot.EnableSeller", seller)
set_option("AuctionHouseBot.EnableBuyer", buyer)
path.write_text(text)
PY

echo "[FURY][AHBOT][PASS] account=${ACCOUNT} character=${CHARACTER} guid=${character_guid}"
echo "[FURY] seller=${ENABLE_SELLER} buyer=${ENABLE_BUYER}"
echo "[FURY] next: ./runtime/run-playable.sh"
