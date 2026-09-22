#include "core/FuryApp.h"
#include "actors/ActorPolicy.h"
#include "events/FuryEventFactory.h"

#include "PlayerScript.h"
#include "SpellInfo.h"
#include "SpellMgr.h"

#include <unordered_map>

namespace
{
struct PendingCreatedItem
{
    uint32 itemId = 0;
    uint32 count = 0;
};

uint32 ResolveCraftOutputItem(uint32 spellId)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return 0;

    for (SpellEffectInfo const& effect : spellInfo->GetEffects())
    {
        if ((effect.Effect == SPELL_EFFECT_CREATE_ITEM ||
             effect.Effect == SPELL_EFFECT_CREATE_ITEM_2) &&
            effect.ItemType)
        {
            return effect.ItemType;
        }
    }

    return 0;
}

void Publish(Fury::FuryEvent event)
{
    // Random world-population bots are intentionally not written to the
    // general durable spine. Recording every bot login/zone/kill with hundreds
    // of bots would create useless write amplification.
    if (!Fury::ShouldPersistGeneralEvent(event.actor.kind))
        return;

    Fury::App::Instance().Events().Append(event);
}

class FuryPlayerScript final : public PlayerScript
{
public:
    FuryPlayerScript()
        : PlayerScript(
            "FuryPlayerScript",
            {
                PLAYERHOOK_ON_LOGIN,
                PLAYERHOOK_ON_LEVEL_CHANGED,
                PLAYERHOOK_ON_UPDATE_ZONE,
                PLAYERHOOK_ON_PLAYER_COMPLETE_QUEST,
                PLAYERHOOK_ON_CREATURE_KILL,
                PLAYERHOOK_ON_CREATURE_KILLED_BY_PET,
                PLAYERHOOK_ON_LOOT_ITEM,
                PLAYERHOOK_ON_CREATE_ITEM,
                PLAYERHOOK_ON_UPDATE_CRAFTING_SKILL
            })
    {
    }

    void OnPlayerLogin(Player* player) override
    {
        Publish(Fury::FuryEventFactory::PlayerLogin(player));
    }

    void OnPlayerLevelChanged(Player* player, uint8 oldLevel) override
    {
        Publish(Fury::FuryEventFactory::LevelChanged(player, oldLevel));
    }

    void OnPlayerUpdateZone(
        Player* player,
        uint32 newZone,
        uint32 newArea) override
    {
        Publish(Fury::FuryEventFactory::ZoneChanged(player, newZone, newArea));
    }

    void OnPlayerCompleteQuest(Player* player, Quest const* quest) override
    {
        Publish(Fury::FuryEventFactory::QuestCompleted(player, quest));
    }

    void OnPlayerCreatureKill(Player* player, Creature* creature) override
    {
        Publish(Fury::FuryEventFactory::CreatureKilled(player, creature, false));
    }

    void OnPlayerCreatureKilledByPet(Player* owner, Creature* creature) override
    {
        Publish(Fury::FuryEventFactory::CreatureKilled(owner, creature, true));
    }

    void OnPlayerLootItem(
        Player* player,
        Item* item,
        uint32 count,
        ObjectGuid lootGuid) override
    {
        Publish(Fury::FuryEventFactory::ItemLooted(
            player,
            item,
            count,
            lootGuid));
    }

    void OnPlayerCreateItem(Player* player, Item* item, uint32 count) override
    {
        Publish(Fury::FuryEventFactory::ItemCreated(player, item, count));

        if (!player || !item || !count)
            return;

        _pendingCreatedItems[player->GetGUID().GetRawValue()] = {
            item->GetEntry(),
            count
        };
    }

    void OnPlayerUpdateCraftingSkill(
        Player* player,
        SkillLineAbilityEntry const* skill,
        uint32 /*currentLevel*/,
        uint32& /*gain*/) override
    {
        if (!player || !skill || !skill->SkillLine || !skill->Spell)
            return;

        uint64 const playerGuid = player->GetGUID().GetRawValue();
        auto itr = _pendingCreatedItems.find(playerGuid);
        if (itr == _pendingCreatedItems.end())
            return;

        PendingCreatedItem const pending = itr->second;
        _pendingCreatedItems.erase(itr);

        uint32 const expectedItemId =
            ResolveCraftOutputItem(skill->Spell);
        if (!expectedItemId ||
            expectedItemId != pending.itemId)
        {
            return;
        }

        Publish(Fury::FuryEventFactory::ProfessionCrafted(
            player,
            skill->SkillLine,
            skill->Spell,
            pending.itemId,
            pending.count));
    }

private:
    std::unordered_map<uint64, PendingCreatedItem> _pendingCreatedItems;
};
}

void AddFuryPlayerScripts()
{
    new FuryPlayerScript();
}
