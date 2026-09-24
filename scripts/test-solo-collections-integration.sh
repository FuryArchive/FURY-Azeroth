#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CORE="${ROOT}/upstream/azerothcore-wotlk"
PLATFORM="${ROOT}/upstream/integrations/SoloCollectionsPlatform"
BACKEND="${CORE}/modules/mod-solo-collections"
ADDON="${PLATFORM}/SoloCollections/addon/SoloCollections"
SUITE="${PLATFORM}/SoloClientSuite/Interface/AddOns"

fail() { echo "[FURY][SOLO][FAIL] $*" >&2; exit 1; }
pass() { echo "[FURY][SOLO][PASS] $*"; }

[[ -L "${BACKEND}" ]] || fail "backend is not linked into AzerothCore modules"
[[ -f "${BACKEND}/include.sh" ]] || fail "backend include.sh missing"
pass "matched C++ backend linked"

conf="${BACKEND}/conf/transmog.conf.dist"
grep -Eq '^SoloCollections\.Backend[[:space:]]*=[[:space:]]*Cpp[[:space:]]*$' "${conf}" || fail "Cpp/SC2 backend is not the default"
grep -Eq '^SoloCollections\.Preview\.Enabled[[:space:]]*=[[:space:]]*1[[:space:]]*$' "${conf}" || fail "server-authoritative preview is not enabled"
pass "production Cpp/SC2 backend defaults are active"

toc="${ADDON}/SoloCollections.toc"
[[ -f "${toc}" ]] || fail "SoloCollections.toc missing"
grep -Eq '^## Dependencies:[[:space:]]*DragonUI_NewEra[[:space:]]*$' "${toc}" || fail "SoloCollections dependency contract changed"
pass "SoloCollections hard dependency is DragonUI_NewEra"

[[ -f "${SUITE}/DragonUI_NewEra/DragonUI_NewEra.toc" ]] || fail "DragonUI_NewEra missing"
grep -Eq '^## Dependencies:[[:space:]]*DragonUI,[[:space:]]*!!!ClassicAPI[[:space:]]*$' "${SUITE}/DragonUI_NewEra/DragonUI_NewEra.toc" || fail "DragonUI_NewEra dependency chain changed"
[[ -f "${SUITE}/DragonUI/DragonUI.toc" ]] || fail "DragonUI missing"
[[ -f "${SUITE}/!!!ClassicAPI/!!!ClassicAPI.toc" ]] || fail "!!!ClassicAPI missing"
pass "matched client dependency chain is complete"

bash "${ROOT}/scripts/stage-client-integrations.sh" "${ROOT}/build/client-integrations-test"
[[ -f "${ROOT}/build/client-integrations-test/Interface/AddOns/SoloCollections/SoloCollections.toc" ]] || fail "staged SoloCollections addon missing"
[[ -f "${ROOT}/build/client-integrations-test/FURY-SOLO-COLLECTIONS-MANIFEST.txt" ]] || fail "staging manifest missing"
pass "client staging contract passed"

echo "[FURY][SOLO][PASS] SoloCollections integration contract passed"
