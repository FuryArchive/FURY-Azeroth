#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOCK="${ROOT}/vendor/lock/fury.lock.yaml"
OUT="${1:-${ROOT}/build/mythic-plus-integration}"

read_dir() {
  python3 - "${LOCK}" "$1" <<'PY'
import json,sys
with open(sys.argv[1],encoding="utf-8") as f: lock=json.load(f)
print(lock["integrations"][sys.argv[2]]["directory"])
PY
}

MPLUS="${ROOT}/upstream/integrations/$(read_dir mythic_plus_extended)"
AIO="${ROOT}/upstream/integrations/$(read_dir aio)"

[[ -f "${MPLUS}/MythicPlus/Mythic_Server.lua" ]] || { echo "[FURY][MYTHIC][FAIL] Mythic server source missing" >&2; exit 1; }
[[ -f "${AIO}/AIO_Server/AIO.lua" ]] || { echo "[FURY][MYTHIC][FAIL] AIO server missing" >&2; exit 1; }

rm -rf "${OUT}"
mkdir -p "${OUT}/server/lua_scripts" "${OUT}/server/sql/world" "${OUT}/server/sql/characters"          "${OUT}/client/Interface/AddOns" "${OUT}/client/mpq-merge-input/Interface" "${OUT}/dbc/custom-csv"

cp -a "${AIO}/AIO_Server" "${OUT}/server/lua_scripts/AIO"
cp -a "${MPLUS}/MythicPlus" "${OUT}/server/lua_scripts/MythicPlus"
python3 "${ROOT}/scripts/transform-mythic-plus-fury-authority.py" "${OUT}/server/lua_scripts/MythicPlus"

cp -a "${MPLUS}/Data/SQL/world/." "${OUT}/server/sql/world/"
cp -a "${MPLUS}/Data/SQL/characters/." "${OUT}/server/sql/characters/"

cp -a "${AIO}/AIO_Client" "${OUT}/client/Interface/AddOns/AIO_Client"
cp -a "${MPLUS}/Data/Client/Raw/all/Interface/." "${OUT}/client/mpq-merge-input/Interface/"
cp -a "${MPLUS}/Data/Client/Raw/changes/." "${OUT}/dbc/custom-csv/"

cat > "${OUT}/FURY-MYTHIC-PLUS-MANIFEST.txt" <<EOF
mythic_source=Ildourol/MythicPlus-extended
mythic_commit=$(git -C "${MPLUS}" rev-parse HEAD)
aio_source=Rochet2/AIO
aio_commit=$(git -C "${AIO}" rev-parse HEAD)
runtime=standard-eluna+AIO
direct_persistent_rewards=false
player_config=gm-only
rating_authority=mythic-plus
keystone_authority=mythic-plus
vault_progress=mythic-plus
durable_payout=fury-deferred
client_packaging=merge-input-only
dbc_delta=Item:900100,ItemDisplayInfo:62471
EOF

echo "[FURY][MYTHIC][PASS] staged MythicPlus Extended + AIO"
cat "${OUT}/FURY-MYTHIC-PLUS-MANIFEST.txt"
