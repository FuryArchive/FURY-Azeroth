#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOCK="${ROOT}/vendor/lock/fury.lock.yaml"
CORE="${ROOT}/upstream/azerothcore-wotlk"

integration_dir="$(python3 - "${LOCK}" <<'PY'
import json
import sys
with open(sys.argv[1], "r", encoding="utf-8") as fh:
    lock = json.load(fh)
print(lock["integrations"]["worgoblin"]["directory"])
PY
)"
SRC="${ROOT}/upstream/integrations/${integration_dir}"
MODULE="${CORE}/modules/mod-worgoblin"

fail() { echo "[FURY][WORGOBLIN][FAIL] $*" >&2; exit 1; }
pass() { echo "[FURY][WORGOBLIN][PASS] $*"; }

[[ -L "${MODULE}" ]] || fail "mod-worgoblin is not linked into AzerothCore"
[[ -f "${MODULE}/include.sh" ]] || fail "mod-worgoblin include.sh missing"
pass "server module linked"

grep -Eq 'RACE_GOBLIN[[:space:]]*=[[:space:]]*9' "${CORE}/src/server/shared/SharedDefines.h" || fail "Goblin race enum missing"
grep -Eq 'RACE_WORGEN[[:space:]]*=[[:space:]]*12' "${CORE}/src/server/shared/SharedDefines.h" || fail "Worgen race enum missing"
grep -q 'case RACE_GOBLIN' "${CORE}/src/server/shared/enuminfo_SharedDefines.cpp" || fail "Goblin EnumUtils mapping missing"
grep -q 'case RACE_WORGEN' "${CORE}/src/server/shared/enuminfo_SharedDefines.cpp" || fail "Worgen EnumUtils mapping missing"
pass "Playerbots race enum integration present"

grep -q 'HasSpell(69044)' "${CORE}/src/server/game/Entities/Player/Player.cpp" || fail "Goblin Best Deals Anywhere handling missing"
grep -q 'OnPlayerGetReputationPriceDiscount(this, factionTemplate, discount)' "${CORE}/src/server/game/Entities/Player/Player.cpp" || fail "reputation ScriptMgr hook missing"
pass "Goblin racial discount preserves ScriptMgr hook"

[[ -f "${SRC}/data/patch/DBFilesClient/ChrRaces.dbc" ]] || fail "ChrRaces.dbc missing"
[[ -f "${SRC}/data/patch/DBFilesClient/Spell.dbc" ]] || fail "Spell.dbc missing"
[[ -d "${SRC}/data/patch/Character" ]] || fail "client Character assets missing"
[[ -d "${SRC}/data/patch-hd/DBFilesClient" ]] || fail "HD reference DBC overlay missing"
pass "client/server custom-race assets present"

bash "${ROOT}/scripts/stage-worgoblin-integration.sh" "${ROOT}/build/worgoblin-integration-test"
[[ -f "${ROOT}/build/worgoblin-integration-test/FURY-WORGOBLIN-MANIFEST.txt" ]] || fail "staging manifest missing"
[[ -f "${ROOT}/build/worgoblin-integration-test/server/dbc/ChrRaces.dbc" ]] || fail "staged server DBC missing"
pass "merge-input staging passed"

echo "[FURY][WORGOBLIN][PASS] Worgen/Goblin integration contract passed"
