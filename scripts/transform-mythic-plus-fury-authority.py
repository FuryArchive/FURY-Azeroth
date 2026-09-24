#!/usr/bin/env python3
from pathlib import Path
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: transform-mythic-plus-fury-authority.py <MythicPlus-dir>")

root = Path(sys.argv[1])

def replace_once(path: Path, old: str, new: str, label: str) -> None:
    text = path.read_text(encoding="utf-8")
    if new in text:
        return
    if old not in text:
        raise SystemExit(f"[FURY][MYTHIC][FAIL] upstream anchor changed: {label}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")

config = root / "Mythic_Config.lua"
replace_once(
    config,
    "    HeroicRatingMultiplier = 1.1, -- Score rating multiplier on Heroic Mythic+ completion\n    AllowPlayerConfig = 1",
    "    HeroicRatingMultiplier = 1.1, -- Score rating multiplier on Heroic Mythic+ completion\n"
    "    DirectPersistentRewards = 0, -- FURY owns durable payout; keep rating/keys/vault progress active\n"
    "    AllowPlayerConfig = 0",
    "Mythic_Config authority defaults",
)

server = root / "Mythic_Server.lua"
replace_once(
    server,
    "    HeroicRatingMultiplier = 1.1, -- +10% rating score on Heroic\n}",
    "    HeroicRatingMultiplier = 1.1, -- +10% rating score on Heroic\n"
    "    DirectPersistentRewards = 0,  -- FURY reward authority\n"
    "    AllowPlayerConfig       = 0,  -- GM-only server settings\n}",
    "server config defaults",
)

replace_once(
    server,
    '            f:write(string.format("    HeroicRatingMultiplier = %.2f,\\n", MythicConfig.HeroicRatingMultiplier or 1.1))\n'
    '            f:write(string.format("    AllowPlayerConfig = %d,\\n", MythicConfig.AllowPlayerConfig or 1))',
    '            f:write(string.format("    HeroicRatingMultiplier = %.2f,\\n", MythicConfig.HeroicRatingMultiplier or 1.1))\n'
    '            f:write(string.format("    DirectPersistentRewards = %d,\\n", MythicConfig.DirectPersistentRewards or 0))\n'
    '            f:write(string.format("    AllowPlayerConfig = %d,\\n", MythicConfig.AllowPlayerConfig or 0))',
    "saved authority config",
)

replace_once(
    server,
    "local function TryRewardMythicLoot(player, tier, upgradeLevel, diff)\n"
    "    tier = tier or 1",
    "local function TryRewardMythicLoot(player, tier, upgradeLevel, diff)\n"
    "    if (MythicConfig.DirectPersistentRewards or 0) ~= 1 then\n"
    "        return\n"
    "    end\n\n"
    "    tier = tier or 1",
    "end-of-run reward gate",
)

replace_once(
    server,
    "function MythicHandlers.SelectVaultItem(player, itemIndex)\n"
    "    local guid = player:GetGUIDLow()",
    "function MythicHandlers.SelectVaultItem(player, itemIndex)\n"
    "    if (MythicConfig.DirectPersistentRewards or 0) ~= 1 then\n"
    '        player:SendBroadcastMessage("[Mythic+] FURY reward adapter is not active yet; your vault progress is preserved.")\n'
    "        return\n"
    "    end\n\n"
    "    local guid = player:GetGUIDLow()",
    "Great Vault payout gate",
)

print("[FURY][MYTHIC][PASS] direct durable rewards disabled and player config locked")
