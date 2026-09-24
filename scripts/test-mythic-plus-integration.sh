#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${1:-${ROOT}/build/mythic-plus-integration}"
fail(){ echo "[FURY][MYTHIC][FAIL] $*" >&2; exit 1; }
pass(){ echo "[FURY][MYTHIC][PASS] $*"; }

[[ -f "${OUT}/server/lua_scripts/AIO/AIO.lua" ]] || fail "AIO server not staged"
[[ -f "${OUT}/client/Interface/AddOns/AIO_Client/AIO_Client.toc" ]] || fail "AIO client addon not staged"
pass "AIO server/client bridge staged"

server="${OUT}/server/lua_scripts/MythicPlus/Mythic_Server.lua"
config="${OUT}/server/lua_scripts/MythicPlus/Mythic_Config.lua"
grep -q 'DirectPersistentRewards = 0' "${config}" || fail "reward authority default missing"
grep -q 'AllowPlayerConfig = 0' "${config}" || fail "player config is not locked"
grep -q 'FURY reward adapter is not active yet' "${server}" || fail "Great Vault payout gate missing"
grep -q 'MythicConfig.DirectPersistentRewards' "${server}" || fail "end-of-run payout gate missing"
pass "FURY reward/config authority enforced"

for file in character_mythic_history.sql character_mythic_keys.sql character_mythic_rating.sql character_mythic_vault.sql character_mythic_weekly_affixes.sql; do
  [[ -f "${OUT}/server/sql/characters/${file}" ]] || fail "missing character SQL ${file}"
done
for file in creature_and_keystones.sql world_mythic_loot.sql world_vault_loot.sql; do
  [[ -f "${OUT}/server/sql/world/${file}" ]] || fail "missing world SQL ${file}"
done
pass "Mythic state/keystone/loot schemas staged"

[[ -f "${OUT}/dbc/custom-csv/Item.csv" ]] || fail "Item delta missing"
[[ -f "${OUT}/dbc/custom-csv/ItemDisplayInfo.csv" ]] || fail "ItemDisplayInfo delta missing"
grep -q '"900100"' "${OUT}/dbc/custom-csv/Item.csv" || fail "keystone Item delta missing"
grep -q '"62471"' "${OUT}/dbc/custom-csv/ItemDisplayInfo.csv" || fail "keystone display delta missing"
[[ ! -f "${OUT}/client/mpq-merge-input/DBFilesClient/Item.dbc" ]] || fail "foreign full Item.dbc must not be staged"
pass "client uses DBC deltas, not foreign full DBCs"

[[ -f "${OUT}/client/mpq-merge-input/Interface/MythicPlus/textures/MythicFrame.blp" ]] || fail "Mythic UI texture missing"
[[ -f "${OUT}/client/mpq-merge-input/Interface/MythicPlus/sounds/UI_BattlegroundCountdown_End.ogg" ]] || fail "Mythic UI sound missing"
pass "Mythic UI assets staged"

echo "[FURY][MYTHIC][PASS] MythicPlus integration contract passed"
