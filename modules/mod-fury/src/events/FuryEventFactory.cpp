#include "FuryEventFactory.h"

#include "core/FuryApp.h"
#include "Creature.h"
#include "Item.h"
#include "Player.h"
#include "QuestDef.h"
#include "StringFormat.h"

#include <atomic>
#include <random>

namespace
{
uint64 BootNonce()
{
    static uint64 const nonce = []()
    {
        std::random_device random;
        return (static_cast<uint64>(random()) << 32) |
            static_cast<uint64>(random());
    }();

    return nonce;
}

uint64 NextOccurrence()
{
    static std::atomic<uint64> sequence{1};
    return sequence.fetch_add(1, std::memory_order_relaxed);
}
}

namespace Fury
{
FuryEvent FuryEventFactory::Base(Player* player, std::string type)
{
    FuryEvent event;
    event.type = std::move(type);
    event.actor = App::Instance().Actors().Resolve(player);
    event.sourceSystem = "azerothcore";

    if (player)
    {
        event.mapId = player->GetMapId();
        event.zoneId = player->GetZoneId();
        event.areaId = player->GetAreaId();
    }

    return event;
}

std::string FuryEventFactory::OccurrenceIdentity(
    std::string_view scope,
    Player* player)
{
    uint64 const guid = player ? player->GetGUID().GetRawValue() : 0;
    return Acore::StringFormat(
        "{}:{}:{}:{}",
        scope,
        BootNonce(),
        guid,
        NextOccurrence());
}

FuryEvent FuryEventFactory::PlayerLogin(Player* player)
{
    FuryEvent event = Base(player, "player.login");
    event.dedupeIdentity = OccurrenceIdentity("login", player);
    return event;
}

FuryEvent FuryEventFactory::LevelChanged(Player* player, uint8 oldLevel)
{
    FuryEvent event = Base(player, "player.level.changed");

    uint8 const newLevel = player ? player->GetLevel() : 0;
    event.subjectType = "level";
    event.subjectId = newLevel;
    event.payloadJson = Acore::StringFormat(
        "{{\"old_level\":{},\"new_level\":{}}}",
        oldLevel,
        newLevel);

    uint64 const guid = player ? player->GetGUID().GetRawValue() : 0;
    event.dedupeIdentity = Acore::StringFormat(
        "level:v1:{}:{}",
        guid,
        newLevel);

    return event;
}

FuryEvent FuryEventFactory::ZoneChanged(
    Player* player,
    uint32 newZone,
    uint32 newArea)
{
    FuryEvent event = Base(player, "player.zone.changed");
    event.zoneId = newZone;
    event.areaId = newArea;
    event.subjectType = "zone";
    event.subjectId = newZone;
    event.payloadJson = Acore::StringFormat(
        "{{\"zone_id\":{},\"area_id\":{}}}",
        newZone,
        newArea);
    event.dedupeIdentity = OccurrenceIdentity("zone", player);
    return event;
}

FuryEvent FuryEventFactory::QuestCompleted(Player* player, Quest const* quest)
{
    FuryEvent event = Base(player, "quest.completed");

    uint32 const questId = quest ? quest->GetQuestId() : 0;
    bool const repeatable = quest && quest->IsRepeatable();

    event.subjectType = "quest";
    event.subjectId = questId;
    event.payloadJson = Acore::StringFormat(
        "{{\"quest_id\":{},\"repeatable\":{}}}",
        questId,
        repeatable ? "true" : "false");

    if (repeatable)
    {
        event.dedupeIdentity = OccurrenceIdentity("quest-repeatable", player);
    }
    else
    {
        uint64 const guid = player ? player->GetGUID().GetRawValue() : 0;
        event.dedupeIdentity = Acore::StringFormat(
            "quest:v1:{}:{}",
            guid,
            questId);
    }

    return event;
}

FuryEvent FuryEventFactory::CreatureKilled(
    Player* player,
    Creature* creature,
    bool viaPet)
{
    FuryEvent event = Base(player, "creature.killed");

    uint32 const entry = creature ? creature->GetEntry() : 0;
    uint64 const creatureGuid = creature ? creature->GetGUID().GetRawValue() : 0;

    event.subjectType = "creature";
    event.subjectId = entry;
    event.payloadJson = Acore::StringFormat(
        "{{\"entry\":{},\"creature_guid\":{},\"via_pet\":{}}}",
        entry,
        creatureGuid,
        viaPet ? "true" : "false");

    // Spawn GUIDs are reused after respawn, so a kill is an occurrence rather
    // than a globally stable entity identity.
    event.dedupeIdentity = OccurrenceIdentity(
        viaPet ? "creature-kill-pet" : "creature-kill",
        player);

    return event;
}

FuryEvent FuryEventFactory::ItemLooted(
    Player* player,
    Item* item,
    uint32 count,
    ObjectGuid lootGuid)
{
    FuryEvent event = Base(player, "item.looted");

    uint32 const entry = item ? item->GetEntry() : 0;
    uint64 const itemGuid = item ? item->GetGUID().GetRawValue() : 0;

    event.subjectType = "item";
    event.subjectId = entry;
    event.payloadJson = Acore::StringFormat(
        "{{\"entry\":{},\"item_guid\":{},\"loot_guid\":{},\"count\":{}}}",
        entry,
        itemGuid,
        lootGuid.GetRawValue(),
        count);

    if (itemGuid)
    {
        uint64 const playerGuid = player ? player->GetGUID().GetRawValue() : 0;
        event.dedupeIdentity = Acore::StringFormat(
            "item-loot:v1:{}:{}:{}:{}",
            playerGuid,
            itemGuid,
            lootGuid.GetRawValue(),
            count);
    }
    else
    {
        event.dedupeIdentity = OccurrenceIdentity("item-loot", player);
    }

    return event;
}

FuryEvent FuryEventFactory::ItemCreated(
    Player* player,
    Item* item,
    uint32 count)
{
    FuryEvent event = Base(player, "item.created");

    uint32 const entry = item ? item->GetEntry() : 0;
    uint64 const itemGuid = item ? item->GetGUID().GetRawValue() : 0;

    event.subjectType = "item";
    event.subjectId = entry;
    event.payloadJson = Acore::StringFormat(
        "{{\"entry\":{},\"item_guid\":{},\"count\":{}}}",
        entry,
        itemGuid,
        count);

    if (itemGuid)
    {
        uint64 const playerGuid = player ? player->GetGUID().GetRawValue() : 0;
        event.dedupeIdentity = Acore::StringFormat(
            "item-create:v1:{}:{}:{}",
            playerGuid,
            itemGuid,
            count);
    }
    else
    {
        event.dedupeIdentity = OccurrenceIdentity("item-create", player);
    }

    return event;
}
}
