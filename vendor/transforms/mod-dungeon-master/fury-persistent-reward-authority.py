#!/usr/bin/env python3
from pathlib import Path
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: fury-persistent-reward-authority.py <mod-dungeon-master-dir>")

root = Path(sys.argv[1])

def replace_once(rel: str, old: str, new: str) -> None:
    path = root / rel
    text = path.read_text(encoding="utf-8")
    if new in text:
        return
    if old not in text:
        raise SystemExit(f"[FURY][AUTHORITY][FAIL] Dungeon Master anchor changed: {rel}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")

replace_once(
    "conf/mod_dungeon_master.conf.dist",
    """# REWARDS
###############################################################################

#    DungeonMaster.Rewards.BaseGold""",
    """# REWARDS
###############################################################################

#    DungeonMaster.Rewards.DirectPersistentEnabled
#        Allow Dungeon Master to directly grant custom kill XP, generated loot,
#        completion gold/items, and roguelike reward items.
#        FURY keeps this disabled so persistent rewards/progression can be
#        mediated by the FURY reward/proof layer instead of bypassing it.
#        Default for the FURY integration: 0
DungeonMaster.Rewards.DirectPersistentEnabled = 0

#    DungeonMaster.Rewards.BaseGold""",
)

replace_once(
    "src/DMConfig.h",
    """    // --- Rewards ---
    uint32 GetBaseGold()""",
    """    // --- Rewards ---
    bool   AreDirectPersistentRewardsEnabled() const { return _directPersistentRewardsEnabled; }
    uint32 GetBaseGold()""",
)

replace_once(
    "src/DMConfig.h",
    """    // Rewards
    uint32 _baseGold""",
    """    // Rewards
    bool   _directPersistentRewardsEnabled = false;
    uint32 _baseGold""",
)

replace_once(
    "src/DMConfig.cpp",
    """    // Rewards
    _baseGold""",
    """    // Rewards
    _directPersistentRewardsEnabled = sConfigMgr->GetOption<bool>("DungeonMaster.Rewards.DirectPersistentEnabled", false);
    _baseGold""",
)

replace_once(
    "src/DungeonMasterMgr.cpp",
    """void DungeonMasterMgr::DistributeRewards(Session* session)
{
    if (!session) return;""",
    """void DungeonMasterMgr::DistributeRewards(Session* session)
{
    if (!sDMConfig->AreDirectPersistentRewardsEnabled())
    {
        LOG_INFO("module", "DungeonMaster: direct persistent completion rewards suppressed by FURY authority");
        return;
    }

    if (!session) return;""",
)

replace_once(
    "src/DungeonMasterMgr.cpp",
    """void DungeonMasterMgr::GiveKillXP(Session* session, bool isBoss, bool isElite)
{
    if (!session) return;""",
    """void DungeonMasterMgr::GiveKillXP(Session* session, bool isBoss, bool isElite)
{
    if (!sDMConfig->AreDirectPersistentRewardsEnabled())
        return;

    if (!session) return;""",
)

replace_once(
    "src/DungeonMasterMgr.cpp",
    """void DungeonMasterMgr::GiveGoldReward(Player* player, uint32 amount)
{
    if (!player || !amount) return;""",
    """void DungeonMasterMgr::GiveGoldReward(Player* player, uint32 amount)
{
    if (!sDMConfig->AreDirectPersistentRewardsEnabled())
        return;

    if (!player || !amount) return;""",
)

replace_once(
    "src/DungeonMasterMgr.cpp",
    """void DungeonMasterMgr::GiveItemReward(Player* player, uint8 level, uint8 quality)
{
    uint32 playerClass""",
    """void DungeonMasterMgr::GiveItemReward(Player* player, uint8 level, uint8 quality)
{
    if (!sDMConfig->AreDirectPersistentRewardsEnabled())
        return;

    uint32 playerClass""",
)

replace_once(
    "src/DungeonMasterMgr.cpp",
    """void DungeonMasterMgr::MailItemReward(Player* player, uint8 level, uint8 quality,
                                       const std::string& subject, const std::string& body)
{
    if (!player || !player->IsInWorld()) return;""",
    """void DungeonMasterMgr::MailItemReward(Player* player, uint8 level, uint8 quality,
                                       const std::string& subject, const std::string& body)
{
    if (!sDMConfig->AreDirectPersistentRewardsEnabled())
        return;

    if (!player || !player->IsInWorld()) return;""",
)

replace_once(
    "src/DungeonMasterMgr.cpp",
    """void DungeonMasterMgr::DistributeRoguelikeRewards(uint32 tier, uint8 effectiveLevel,
                                                    const std::vector<ObjectGuid>& playerGuids)
{
    uint8 rewardLevel""",
    """void DungeonMasterMgr::DistributeRoguelikeRewards(uint32 tier, uint8 effectiveLevel,
                                                    const std::vector<ObjectGuid>& playerGuids)
{
    if (!sDMConfig->AreDirectPersistentRewardsEnabled())
    {
        LOG_INFO("module", "DungeonMaster: direct persistent roguelike rewards suppressed by FURY authority");
        return;
    }

    uint8 rewardLevel""",
)

replace_once(
    "src/DungeonMasterMgr.cpp",
    """void DungeonMasterMgr::FillCreatureLoot(Creature* creature, Session* session, bool isBoss)
{
    if (!creature || !session) return;""",
    """void DungeonMasterMgr::FillCreatureLoot(Creature* creature, Session* session, bool isBoss)
{
    if (!sDMConfig->AreDirectPersistentRewardsEnabled())
    {
        LOG_DEBUG("module", "DungeonMaster: generated creature loot suppressed by FURY authority");
        return;
    }

    if (!creature || !session) return;""",
)

print("[FURY][AUTHORITY][PASS] Dungeon Master direct persistent rewards gated")
