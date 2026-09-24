#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOCK="${ROOT}/vendor/lock/fury.lock.yaml"
UPSTREAM="${ROOT}/upstream"
OUT="${1:-${ROOT}/build/client-integrations}"
ADDONS="${OUT}/Interface/AddOns"

platform_dir="$(python3 - "${LOCK}" <<'PY'
import json
import sys
with open(sys.argv[1], "r", encoding="utf-8") as fh:
    lock = json.load(fh)
print(lock["integrations"]["solo_collections_platform"]["directory"])
PY
)"

PLATFORM="${UPSTREAM}/integrations/${platform_dir}"
SOLO="${PLATFORM}/SoloCollections"
SUITE="${PLATFORM}/SoloClientSuite/Interface/AddOns"

require_dir() {
  [[ -d "$1" ]] || { echo "[FURY][CLIENT][FAIL] missing directory: $1" >&2; exit 1; }
}

require_dir "${SOLO}/addon/SoloCollections"
require_dir "${SUITE}/!!!ClassicAPI"
require_dir "${SUITE}/DragonUI"
require_dir "${SUITE}/DragonUI_NewEra"

rm -rf "${OUT}"
mkdir -p "${ADDONS}"

cp -a "${SOLO}/addon/SoloCollections" "${ADDONS}/SoloCollections"
cp -a "${SUITE}/!!!ClassicAPI" "${ADDONS}/!!!ClassicAPI"
cp -a "${SUITE}/DragonUI" "${ADDONS}/DragonUI"
cp -a "${SUITE}/DragonUI_NewEra" "${ADDONS}/DragonUI_NewEra"

if [[ -d "${SUITE}/DragonUI_Options" ]]; then
  cp -a "${SUITE}/DragonUI_Options" "${ADDONS}/DragonUI_Options"
fi

cat > "${OUT}/FURY-SOLO-COLLECTIONS-MANIFEST.txt" <<EOF
source=SoloCollectionsPlatform
commit=$(git -C "${PLATFORM}" rev-parse HEAD)
backend=mod-solo-collections
addons=!!!ClassicAPI,DragonUI,DragonUI_NewEra,SoloCollections
optional_addons=DragonUI_Options
backend_mode=Cpp/SC2
EOF

echo "[FURY][CLIENT][PASS] staged matched SoloCollections client at ${OUT}"
find "${ADDONS}" -maxdepth 1 -mindepth 1 -type d -printf '  - %f\n' | sort
