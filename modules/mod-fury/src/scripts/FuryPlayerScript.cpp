#include "core/FuryApp.h"
#include "actors/ActorPolicy.h"
#include "events/FuryEventFactory.h"

#include "PlayerScript.h"

namespace
{
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
    }
};
}

void AddFuryPlayerScripts()
{
    new FuryPlayerScript();
}
