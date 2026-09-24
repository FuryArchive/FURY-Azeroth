#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROFILE="${1:-${ROOT}/build/playable-profile}"
SERVER="${PROFILE}/server"
CLIENT="${PROFILE}/client"

fail() { echo "[FURY][PLAYABLE][FAIL] $*" >&2; exit 1; }
pass() { echo "[FURY][PLAYABLE][PASS] $*"; }

require_file() { [[ -f "$1" ]] || fail "missing file: $1"; }
require_dir() { [[ -d "$1" ]] || fail "missing directory: $1"; }

require_file "${PROFILE}/PLAYABLE-MANIFEST.txt"
require_file "${PROFILE}/SHA256SUMS"
require_dir "${CLIENT}/Interface/AddOns/SoloCollections"
require_dir "${CLIENT}/Interface/AddOns/DragonUI_NewEra"
require_dir "${CLIENT}/Interface/AddOns/AIO_Client"
require_dir "${CLIENT}/Interface/AddOns/DelvesTeleporter"
require_file "${CLIENT}/Interface/AddOns/QuestRadar/QuestRadar.toc"
require_file "${CLIENT}/Interface/AddOns/NemesisTracker/NemesisTracker.toc"
require_file "${CLIENT}/Interface/AddOns/QOLAddon/QOLAddon.toc"
require_file "${SERVER}/lua_scripts/AIO.lua"
require_file "${SERVER}/lua_scripts/MythicPlus/Mythic_Server.lua"
require_file "${SERVER}/lua_scripts/MythicPlus/Mythic_Client.lua"
require_file "${SERVER}/lua_scripts/Delves/delves-teleporter.lua"
pass "required client/server scripting surfaces present"

if grep -Eq 'wip[[:space:]]*=[[:space:]]*true|id[[:space:]]*=[[:space:]]*(4001|4011)' "${CLIENT}/Interface/AddOns/DelvesTeleporter/DelvesTeleporter.lua"; then
  fail "WIP Delves are exposed by the client browser"
fi
if grep -Eq '\[(4001|4011)\][[:space:]]*=' "${SERVER}/lua_scripts/Delves/delves-teleporter.lua"; then
  fail "WIP Delves are accepted by the server teleporter"
fi
pass "upstream WIP Delves are hidden from production profile"

grep -Fq 'RegisterCreatureEvent(BOSS_ID, 7, OnAIUpdate)' "${SERVER}/lua_scripts/Delves/ForemanGlitzbolt.lua" \
  || fail "Delves Foreman Glitzbolt is not bound to standard Eluna AIUPDATE event 7"
if grep -Fq 'RegisterCreatureEvent(BOSS_ID, 27, OnAIUpdate)' "${SERVER}/lua_scripts/Delves/ForemanGlitzbolt.lua"; then
  fail "Delves Foreman Glitzbolt still uses ALE/incorrect event 27 for AIUPDATE"
fi
pass "Delves AI update hooks target standard Eluna"

grep -Eq 'NoKeystoneRequired[[:space:]]*=[[:space:]]*0' "${SERVER}/lua_scripts/MythicPlus/Mythic_Config.lua" \
  || fail "Mythic+ strict keystone progression is not enabled"
pass "Mythic+ strict keystone progression enabled"

for sql in   "${SERVER}/sql/world/mythicplus/creature_and_keystones.sql"   "${SERVER}/sql/characters/mythicplus/character_mythic_keys.sql"; do
  require_file "${sql}"
done
if [[ "$(find "${SERVER}/sql/world/delves" -maxdepth 1 -name '*.sql' | wc -l)" -lt 20 ]]; then
  fail "Delves SQL payload incomplete"
fi
pass "SQL payload complete"

for file in   "${SERVER}/data-overlay/maps/9002627.map"   "${SERVER}/data-overlay/maps/9115537.map"   "${SERVER}/data-overlay/mmaps/900.mmap"   "${SERVER}/data-overlay/vmaps/911.vmtree"; do
  require_file "${file}"
done
pass "Delves navigation/map data present"

python3 - "${SERVER}/data-overlay/dbc" <<'PY'
from pathlib import Path
import struct, sys

root = Path(sys.argv[1])

def find(name):
    target = name.lower()
    for p in root.iterdir():
        if p.is_file() and p.name.lower() == target:
            return p
    raise SystemExit(f"[FURY][PLAYABLE][FAIL] DBC missing: {name}")

def ids(name):
    p = find(name)
    blob = p.read_bytes()
    magic, count, fields, record_size, strings = struct.unpack_from("<4s4I", blob)
    if magic != b"WDBC" or record_size < 4:
        raise SystemExit(f"[FURY][PLAYABLE][FAIL] invalid WDBC: {p}")
    off = 20
    return {struct.unpack_from("<I", blob, off + i * record_size)[0] for i in range(count)}

requirements = {
    "ChrRaces.dbc": {9, 12},
    "Map.dbc": {805, *range(900, 912)},
    "AreaTable.dbc": set(range(6002, 6015)),
    "Item.dbc": {900100},
    "ItemDisplayInfo.dbc": {62471},
    "CreatureDisplayInfo.dbc": set(range(34076, 34085)),
    "SoundEntries.dbc": set(range(90500, 90512)),
    "WorldSafeLocs.dbc": set(range(5000, 5013)),
}
for name, wanted in requirements.items():
    actual = ids(name)
    missing = sorted(wanted - actual)
    if missing:
        raise SystemExit(f"[FURY][PLAYABLE][FAIL] {name} missing IDs: {missing}")
    print(f"[FURY][PLAYABLE][PASS] {name}: required IDs present")
PY

while IFS= read -r -d '' dbc; do
  base="$(basename "${dbc}")"
  client_match="$(find "${CLIENT}/mpq-root/DBFilesClient" -maxdepth 1 -type f -iname "${base}" -print -quit)"
  [[ -n "${client_match}" ]] || fail "client DBC counterpart missing: ${base}"
  cmp -s "${dbc}" "${client_match}" || fail "server/client DBC mismatch: ${base}"
done < <(find "${SERVER}/data-overlay/dbc" -maxdepth 1 -type f -name '*.dbc' -print0)
pass "server/client DBC overlays are byte-identical"

if command -v luac5.1 >/dev/null 2>&1; then
  LUA_COMPILER=luac5.1
elif command -v luac >/dev/null 2>&1; then
  LUA_COMPILER=luac
else
  LUA_COMPILER=""
fi
if [[ -n "${LUA_COMPILER}" ]]; then
  while IFS= read -r -d '' lua; do
    "${LUA_COMPILER}" -p "${lua}" || fail "Lua syntax error: ${lua}"
  done < <(find "${SERVER}/lua_scripts" -type f -name '*.lua' -print0)
  pass "server Lua parses successfully"
fi

(
  cd "${PROFILE}"
  sha256sum -c SHA256SUMS
)
pass "payload checksums valid"

echo "[FURY][PLAYABLE][PASS] unified playable payload contract passed"
