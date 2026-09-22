#include "core/FuryApp.h"
#include "actors/ActorPolicy.h"
#include "events/FuryEventFactory.h"

#include "DBCStructure.h"
#include "Player.h"
#include "PlayerScript.h"
#include "Spell.h"
#include "SpellMgr.h"

namespace
{
uint32 ResolveProfessionSkill(uint32 spellId)
{
    SkillLineAbilityMapBounds const bounds =
        sSpellMgr->GetSkillLineAbilityMapBounds(spellId);

    uint32 resolvedSkill = 0;

    for (auto itr = bounds.first; itr != bounds.second; ++itr)
    {
        SkillLineAbilityEntry const* ability = itr->second;
        if (!ability ||
            !ability->SkillLine ||
            !IsProfessionSkill(ability->SkillLine))
        {
            continue;
        }

        if (resolvedSkill != 0 &&
            resolvedSkill != ability->SkillLine)
        {
            // Ambiguous DBC mapping: never guess which profession owns the
            // craft. Content validation can surface the recipe explicitly.
            return 0;
        }

        resolvedSkill = ability->SkillLine;
    }

    return resolvedSkill;
}

void Publish(Fury::FuryEvent event)
{
    // Random world-population bots are intentionally not written to the
    // general durable spine. Recording every bot login/zone/kill with hundreds
    // of bots would create useless write amplification.
    if (!Fury::ShouldPersistGeneralEvent(event.actor.kind))
        return;

    (void)Fury::App::Instance().Events().Append(event);
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
                PLAYERHOOK_ON_CREATE_ITEM
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

        if (!player || !item || count == 0)
            return;

        Spell* spell = player->GetCurrentSpell(CURRENT_GENERIC_SPELL);
        if (!spell || !spell->GetSpellInfo())
            return;

        uint32 const spellId = spell->GetSpellInfo()->Id;
        uint32 const skillId = ResolveProfessionSkill(spellId);
        if (!skillId)
            return;

        // Profession orders count physical output units. A recipe producing
        // several items therefore becomes several durable unit events, each
        // replay-safe through its own occurrence identity.
        for (uint32 unit = 0; unit < count; ++unit)
        {
            Publish(Fury::FuryEventFactory::ProfessionCrafted(
                player,
                skillId,
                spellId,
                item->GetEntry()));
        }
    }
};
}

void AddFuryPlayerScripts()
{
    new FuryPlayerScript();
}
