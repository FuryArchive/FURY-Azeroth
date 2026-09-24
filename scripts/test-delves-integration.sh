#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${1:-${ROOT}/build/delves-integration}"

fail() { echo "[FURY][DELVES][FAIL] $*" >&2; exit 1; }
pass() { echo "[FURY][DELVES][PASS] $*"; }

[[ -f "${OUT}/FURY-DELVES-MANIFEST.txt" ]] || fail "manifest missing"
grep -q '^requires=standard-eluna$' "${OUT}/FURY-DELVES-MANIFEST.txt" || fail "Eluna dependency not recorded"
pass "integration manifest"

lua_count="$(find "${OUT}/server/lua_scripts" -type f -name '*.lua' | wc -l)"
[[ "${lua_count}" -ge 28 ]] || fail "expected at least 28 Lua scripts, got ${lua_count}"
pass "Lua boss/teleporter scripts staged"

for map_id in 805 900 901 902 903 904 905 906 907 908 909 910 911; do
  [[ -f "${OUT}/server/data/mmaps/${map_id}.mmap" ]] || fail "missing mmap index for ${map_id}"
  [[ -f "${OUT}/server/data/vmaps/${map_id}.vmtree" ]] || fail "missing vmap tree for ${map_id}"
done
pass "custom map navigation/collision assets staged"

for csv in Map MapDifficulty AreaTable LoadingScreens WorldSafelocs CreatureDisplayInfo CreatureModelData SoundEntries ZoneMusic; do
  [[ -f "${OUT}/dbc/custom-csv/${csv}.csv" ]] || fail "missing DBC CSV ${csv}.csv"
done
pass "nine additive DBC CSV tables staged"

grep -q 'DELETE FROM `reference_loot_template` WHERE `Entry` = 100500' "${OUT}/server/sql/fury-world.sql" || fail "FURY reward cleanup missing"
grep -q '910001, 911000, 911001' "${OUT}/server/sql/fury-world.sql" || fail "undefined reward IDs not covered by cleanup"
grep -q 'DELETE FROM `gameobject` WHERE `id` = 130000' "${OUT}/server/sql/fury-world.sql" || fail "unowned reward chests are still enabled"
pass "undefined Araxia rewards are suppressed"

[[ -f "${OUT}/client/Interface/AddOns/DelvesTeleporter/DelvesTeleporter.toc" ]] || fail "client teleporter addon missing"
[[ -d "${OUT}/client/mpq-merge-input/world/maps" ]] || fail "client custom map tree missing"
pass "client merge inputs staged"

tmp="${OUT}/dbc/schema-test"
mkdir -p "${tmp}/empty-base" "${tmp}/out"
python3 "${ROOT}/scripts/merge-delves-dbc.py"   --csv-dir "${OUT}/dbc/custom-csv"   --base-dir "${tmp}/empty-base"   --out-dir "${tmp}/out"   --allow-empty-base

python3 - "${tmp}/out" <<'PY'
from pathlib import Path
import struct
import sys
root = Path(sys.argv[1])
for path in sorted(root.glob("*.dbc")):
    raw = path.read_bytes()
    magic, count, fields, record_size, string_size = struct.unpack_from("<4s4I", raw, 0)
    assert magic == b"WDBC", path
    assert count > 0, path
    assert record_size == fields * 4, path
    assert len(raw) == 20 + count * record_size + string_size, path
print(f"[FURY][DELVES][PASS] validated {len(list(root.glob('*.dbc')))} generated WDBC tables")
PY
pass "DBC merge schemas are structurally valid"

echo "[FURY][DELVES][PASS] Delves integration contract passed"
