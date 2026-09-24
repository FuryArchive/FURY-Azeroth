#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOCK="${ROOT}/vendor/lock/fury.lock.yaml"
OUT="${1:-${ROOT}/build/delves-integration}"

integration_dir="$(python3 - "${LOCK}" <<'PY'
import json
import sys
with open(sys.argv[1], "r", encoding="utf-8") as fh:
    lock = json.load(fh)
print(lock["integrations"]["delves"]["directory"])
PY
)"
SRC="${ROOT}/upstream/integrations/${integration_dir}"

[[ -d "${SRC}/data/sql/db-world/base" ]] || { echo "[FURY][DELVES][FAIL] SQL source missing" >&2; exit 1; }
[[ -d "${SRC}/lua_scripts" ]] || { echo "[FURY][DELVES][FAIL] Lua source missing" >&2; exit 1; }
[[ -d "${SRC}/Server Map Files/maps" ]] || { echo "[FURY][DELVES][FAIL] server maps missing" >&2; exit 1; }
[[ -d "${SRC}/DBC_CSV/DBFilesClient" ]] || { echo "[FURY][DELVES][FAIL] DBC CSV source missing" >&2; exit 1; }
[[ -d "${SRC}/MPQ" ]] || { echo "[FURY][DELVES][FAIL] client MPQ tree missing" >&2; exit 1; }

rm -rf "${OUT}"
mkdir -p   "${OUT}/server/sql"   "${OUT}/server/lua_scripts"   "${OUT}/server/data"   "${OUT}/dbc/custom-csv"   "${OUT}/client/Interface/AddOns"

cp -a "${SRC}/lua_scripts/." "${OUT}/server/lua_scripts/"
cp -a "${SRC}/Server Map Files/maps" "${OUT}/server/data/maps"
cp -a "${SRC}/Server Map Files/mmaps" "${OUT}/server/data/mmaps"
cp -a "${SRC}/Server Map Files/vmaps" "${OUT}/server/data/vmaps"
cp -a "${SRC}/DBC_CSV/DBFilesClient/." "${OUT}/dbc/custom-csv/"
cp -a "${SRC}/MPQ" "${OUT}/client/mpq-merge-input"
cp -a "${SRC}/addon/DelvesTeleporter" "${OUT}/client/Interface/AddOns/DelvesTeleporter"

raw_sql="${OUT}/server/sql/upstream-world.sql"
safe_sql="${OUT}/server/sql/fury-world.sql"
: > "${raw_sql}"

while IFS= read -r -d '' file; do
  printf '\n-- ============================================================================\n' >> "${raw_sql}"
  printf -- '-- SOURCE: %s\n' "${file#"${SRC}/"}" >> "${raw_sql}"
  cat "${file}" >> "${raw_sql}"
  printf '\n' >> "${raw_sql}"
done < <(find "${SRC}/data/sql/db-world/base" -maxdepth 1 -type f -name '*.sql' -print0 | sort -z)

cp "${raw_sql}" "${safe_sql}"
cat >> "${safe_sql}" <<'SQL'

-- ============================================================================
-- FURY AUTHORITY / SAFETY FOOTER
-- Araxia Delves references custom reward item IDs that are not defined in the
-- Delves repository. Remove those reward surfaces before worldserver loads.
-- The maps, creatures, boss mechanics and normal per-creature loot remain.
DELETE FROM `reference_loot_template` WHERE `Entry` = 100500;
DELETE FROM `gameobject_loot_template` WHERE `Entry` = 110000;
DELETE FROM `creature_loot_template`
 WHERE `Reference` = 100500
    OR `Item` IN (43949, 910001, 911000, 911001);
DELETE FROM `gameobject` WHERE `id` = 130000;
-- Keep gameobject_template 130000 and the upstream SQL in provenance output;
-- FURY can re-enable chests once its Delves reward adapter owns the payout.
SQL

cat > "${OUT}/FURY-DELVES-MANIFEST.txt" <<EOF
source=araxiaonline/Delves
commit=$(git -C "${SRC}" rev-parse HEAD)
lua_scripts=$(find "${OUT}/server/lua_scripts" -type f -name '*.lua' | wc -l)
maps=$(find "${OUT}/server/data/maps" -type f | wc -l)
mmaps=$(find "${OUT}/server/data/mmaps" -type f | wc -l)
vmaps=$(find "${OUT}/server/data/vmaps" -type f | wc -l)
dbc_csv=$(find "${OUT}/dbc/custom-csv" -type f -name '*.csv' | wc -l)
reward_mode=fury-deferred
client_packaging=merge-input-only
requires=standard-eluna
EOF

echo "[FURY][DELVES][PASS] staged Delves integration at ${OUT}"
cat "${OUT}/FURY-DELVES-MANIFEST.txt"
