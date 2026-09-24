#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOCK="${ROOT}/vendor/lock/fury.lock.yaml"
UPSTREAM="${ROOT}/upstream"
OUT="${1:-${ROOT}/build/worgoblin-integration}"

integration_dir="$(python3 - "${LOCK}" <<'PY'
import json
import sys
with open(sys.argv[1], "r", encoding="utf-8") as fh:
    lock = json.load(fh)
print(lock["integrations"]["worgoblin"]["directory"])
PY
)"

SRC="${UPSTREAM}/integrations/${integration_dir}"
PATCH="${SRC}/data/patch"
HD="${SRC}/data/patch-hd"

require_file() {
  [[ -f "$1" ]] || { echo "[FURY][WORGOBLIN][FAIL] missing file: $1" >&2; exit 1; }
}

require_dir() {
  [[ -d "$1" ]] || { echo "[FURY][WORGOBLIN][FAIL] missing directory: $1" >&2; exit 1; }
}

require_dir "${PATCH}/DBFilesClient"
require_file "${PATCH}/DBFilesClient/ChrRaces.dbc"
require_file "${PATCH}/DBFilesClient/CharBaseInfo.dbc"
require_file "${PATCH}/DBFilesClient/Spell.dbc"
require_dir "${PATCH}/Character"
require_dir "${PATCH}/Interface"
require_dir "${HD}/DBFilesClient"

rm -rf "${OUT}"
mkdir -p "${OUT}/server/dbc" "${OUT}/client"

cp -a "${PATCH}/DBFilesClient/." "${OUT}/server/dbc/"
cp -a "${PATCH}" "${OUT}/client/base-patch"
cp -a "${HD}" "${OUT}/client/hd-reference"

cat > "${OUT}/FURY-WORGOBLIN-MANIFEST.txt" <<EOF
source=araxiaonline/mod-worgoblin
commit=$(git -C "${SRC}" rev-parse HEAD)
server_dbc=server/dbc
client_base_assets=client/base-patch
client_hd_reference=client/hd-reference
packaging_mode=merge-input-only
direct_client_install=false
EOF

echo "[FURY][WORGOBLIN][PASS] staged Worgen/Goblin merge inputs at ${OUT}"
