#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOCK="${ROOT}/vendor/lock/fury.lock.yaml"
UPSTREAM="${ROOT}/upstream"
AC_DATA="${FURY_AC_DATA_DIR:-${ROOT}/build/dist/data}"
OUT="${1:-${ROOT}/build/playable-profile}"
SERVER="${OUT}/server"
CLIENT="${OUT}/client"
RAW="${CLIENT}/mpq-root"
FINAL_DBC="${OUT}/generated/dbc"

read_dir() {
  python3 - "${LOCK}" "$1" <<'PY'
import json, sys
with open(sys.argv[1], "r", encoding="utf-8") as fh:
    lock = json.load(fh)
print(lock["integrations"][sys.argv[2]]["directory"])
PY
}

SOLO="${UPSTREAM}/integrations/$(read_dir solo_collections_platform)"
WORG="${UPSTREAM}/integrations/$(read_dir worgoblin)"
DELVES="${UPSTREAM}/integrations/$(read_dir delves)"
MYTHIC="${UPSTREAM}/integrations/$(read_dir mythic_plus_extended)"
AIO="${UPSTREAM}/integrations/$(read_dir aio)"

fail() { echo "[FURY][PLAYABLE][FAIL] $*" >&2; exit 1; }
pass() { echo "[FURY][PLAYABLE][PASS] $*"; }

for dir in "${SOLO}" "${WORG}" "${DELVES}" "${MYTHIC}" "${AIO}" "${AC_DATA}/dbc"; do
  [[ -d "${dir}" ]] || fail "required workspace missing: ${dir}"
done

rm -rf "${OUT}"
mkdir -p   "${SERVER}/lua_scripts"   "${SERVER}/sql/world/delves"   "${SERVER}/sql/world/mythicplus"   "${SERVER}/sql/characters/mythicplus"   "${SERVER}/data-overlay/dbc"   "${SERVER}/data-overlay/maps"   "${SERVER}/data-overlay/vmaps"   "${SERVER}/data-overlay/mmaps"   "${CLIENT}/Interface/AddOns"   "${RAW}/DBFilesClient"   "${FINAL_DBC}"

# --- Client AddOns -----------------------------------------------------------
bash "${ROOT}/scripts/stage-client-integrations.sh" "${OUT}/solo-collections"
cp -a "${OUT}/solo-collections/Interface/AddOns/." "${CLIENT}/Interface/AddOns/"
cp -a "${AIO}/AIO_Client" "${CLIENT}/Interface/AddOns/AIO_Client"
pass "matched SoloCollections + AIO client AddOns staged"

# --- Server Lua --------------------------------------------------------------
cp -a "${AIO}/AIO_Server/." "${SERVER}/lua_scripts/"
cp -a "${MYTHIC}/MythicPlus" "${SERVER}/lua_scripts/MythicPlus"
mkdir -p "${SERVER}/lua_scripts/Delves"
cp -a "${DELVES}/lua_scripts/." "${SERVER}/lua_scripts/Delves/"
pass "AIO + Mythic+ + Delves Lua staged"

# --- SQL --------------------------------------------------------------------
cp -a "${DELVES}/data/sql/db-world/base/." "${SERVER}/sql/world/delves/"
cp -a "${MYTHIC}/Data/SQL/world/." "${SERVER}/sql/world/mythicplus/"
cp -a "${MYTHIC}/Data/SQL/characters/." "${SERVER}/sql/characters/mythicplus/"
pass "Delves + Mythic+ SQL staged"

# --- Delves server map data --------------------------------------------------
for kind in maps vmaps mmaps; do
  if [[ -d "${DELVES}/Server Map Files/${kind}" ]]; then
    cp -a "${DELVES}/Server Map Files/${kind}/." "${SERVER}/data-overlay/${kind}/"
  fi
done
pass "Delves server maps/vmaps/mmaps staged"

# --- Raw client assets, excluding DBC (merged below) -------------------------
rsync -a --delete-excluded --exclude='DBFilesClient/' "${WORG}/data/patch/." "${RAW}/"
if [[ -d "${WORG}/data/patch-hd/Interface" ]]; then
  mkdir -p "${RAW}/Interface"
  cp -a "${WORG}/data/patch-hd/Interface/." "${RAW}/Interface/"
fi
rsync -a --exclude='DBFilesClient/' "${DELVES}/MPQ/." "${RAW}/"
rsync -a --exclude='DBFilesClient/' "${MYTHIC}/Data/Client/Raw/all/." "${RAW}/"
pass "non-DBC client assets staged"

# Find a DBC by basename case-insensitively.
find_dbc_ci() {
  local dir="$1"
  local wanted="$2"
  python3 - "${dir}" "${wanted}" <<'PY'
from pathlib import Path
import sys
root = Path(sys.argv[1])
wanted = sys.argv[2].lower()
for p in root.iterdir():
    if p.is_file() and p.name.lower() == wanted:
        print(p)
        raise SystemExit(0)
raise SystemExit(1)
PY
}

# Prefer Worgen/Goblin HD DBCs as the baseline for all tables it supplies.
# Copy them using the canonical casing from AC data where one exists.
while IFS= read -r -d '' source; do
  base="$(basename "${source}")"
  canonical="${base}"
  if ac_match="$(find_dbc_ci "${AC_DATA}/dbc" "${base}" 2>/dev/null)"; then
    canonical="$(basename "${ac_match}")"
  fi
  cp -f "${source}" "${FINAL_DBC}/${canonical}"
done < <(find "${WORG}/data/patch-hd/DBFilesClient" -maxdepth 1 -type f -name '*.dbc' -print0)

merge_delta() {
  local csv_file="$1"
  local table
  table="$(basename "${csv_file}" .csv)"
  local wanted="${table}.dbc"
  local base=""

  if base="$(find_dbc_ci "${FINAL_DBC}" "${wanted}" 2>/dev/null)"; then
    :
  elif base="$(find_dbc_ci "${AC_DATA}/dbc" "${wanted}" 2>/dev/null)"; then
    :
  else
    fail "no baseline DBC found for ${table}"
  fi

  local output="${FINAL_DBC}/$(basename "${base}")"
  python3 "${ROOT}/scripts/merge-dbc-csv.py"     --base "${base}"     --delta "${csv_file}"     --output "${output}"     --table "${table}"
}

# Delves contributes rows to nine DBC tables.
while IFS= read -r -d '' delta; do
  merge_delta "${delta}"
done < <(find "${DELVES}/DBC_CSV/DBFilesClient" -maxdepth 1 -type f -name '*.csv' -print0 | sort -z)

# Mythic+ contributes keyed Item / ItemDisplayInfo rows.
merge_delta "${MYTHIC}/Data/Client/Raw/changes/Item.csv"
merge_delta "${MYTHIC}/Data/Client/Raw/changes/ItemDisplayInfo.csv"

cp -a "${FINAL_DBC}/." "${SERVER}/data-overlay/dbc/"
cp -a "${FINAL_DBC}/." "${RAW}/DBFilesClient/"
pass "merged Worgen HD + Delves + Mythic+ DBC overlay staged"

cat > "${OUT}/PLAYABLE-MANIFEST.txt" <<EOF
profile=FURY-Azeroth-playable
core=$(git -C "${UPSTREAM}/azerothcore-wotlk" rev-parse HEAD)
solo_collections=$(git -C "${SOLO}" rev-parse HEAD)
worgoblin=$(git -C "${WORG}" rev-parse HEAD)
delves=$(git -C "${DELVES}" rev-parse HEAD)
mythic_plus_extended=$(git -C "${MYTHIC}" rev-parse HEAD)
aio=$(git -C "${AIO}" rev-parse HEAD)
server_lua=server/lua_scripts
server_sql=server/sql
server_data_overlay=server/data-overlay
client_addons=client/Interface/AddOns
client_mpq_root=client/mpq-root
EOF

(
  cd "${OUT}"
  find server client -type f -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS
)

pass "playable profile staged at ${OUT}"
echo "[FURY][PLAYABLE] $(find "${SERVER}" -type f | wc -l) server payload files"
echo "[FURY][PLAYABLE] $(find "${CLIENT}" -type f | wc -l) client payload files"
