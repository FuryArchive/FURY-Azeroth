#include "core/FuryApp.h"
#include "actors/ActorPolicy.h"
#include "events/FuryEventFactory.h"

#include "Creature.h"
#include "DBCStructure.h"
#include "Player.h"
#include "PlayerScript.h"
#include "UnitScript.h"
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
            // Ambiguous DBC mapping: do not guess which profession owns the
            // recipe. A future content validator can surface the bad mapping.
            return 0;
        }

        resolvedSkill = ability->SkillLine;
    }

    return resolvedSkill;
}

void Publish(Fury::FuryEvent event)
{
    Fury::App& app = Fury::App::Instance();

    // DatabaseScript intentionally does not open acore_fury when the module is
    // disabled. Player hooks remain registered, so they must never touch the
    // event store unless FuryApp completed an enabled startup.
    if (!app.IsInitialized() || !app.IsEnabled())
        return;

    // Random world-population bots are intentionally not written to the
    // general durable spine. Recording every bot login/zone/kill with hundreds
    // of bots would create useless write amplification.
    if (!Fury::ShouldPersistGeneralEvent(event.actor.kind))
        return;

    app.Events().Append(event);
}

class FuryPlayerScript final : public PlayerScript
{
public:
    FuryPlayerScript()
        : PlayerScript(
            "FuryPlayerScript",
            {
                PLAYERHOOK_ON_LOGIN,
                PLAYERHOOK_ON_LOGOUT,
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

    void OnPlayerLogout(Player* player) override
    {
        Publish(Fury::FuryEventFactory::PlayerLogout(player));
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

        // Profession Orders count physical crafted output. Multi-output recipes
        // therefore produce one durable craft event per item unit.
        for (uint32 unit = 0; unit < count; ++unit)
        {
            Publish(Fury::FuryEventFactory::ProfessionCrafted(
                player,
                skillId,
                spellId,
                item->GetEntry(),
                unit + 1,
                count));
        }
    }
};

class FuryParticipationUnitScript final : public UnitScript
{
public:
    FuryParticipationUnitScript()
        : UnitScript(
            "FuryParticipationUnitScript",
            true,
            {
                UNITHOOK_ON_DAMAGE,
                UNITHOOK_ON_UNIT_DEATH
            })
    {
    }

    void OnDamage(
        Unit* attacker,
        Unit* victim,
        uint32& damage) override
    {
        Fury::App& app = Fury::App::Instance();
        if (!app.IsInitialized() || !app.IsEnabled())
            return;

        app.DefiasParticipation().ObserveDamage(
            attacker,
            victim,
            damage);
    }

    void OnUnitDeath(Unit* unit, Unit* killer) override
    {
        Fury::App& app = Fury::App::Instance();
        if (!app.IsInitialized() || !app.IsEnabled())
            return;

        app.DefiasParticipation().ObserveDeath(
            unit,
            killer);
    }
};
}

void AddFuryPlayerScripts()
{
    new FuryPlayerScript();
    new FuryParticipationUnitScript();
}
