#include "FuryEventFactory.h"

#include "core/FuryApp.h"
#include "core/FuryTargetId.h"
#include "integrations/LivingWorldAdapter.h"
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
    event.payloadJson = Acore::StringFormat("{{\"actor_level\":{}}}", player ? player->GetLevel() : 0);
    event.dedupeIdentity = OccurrenceIdentity("login", player);
    return event;
}

FuryEvent FuryEventFactory::PlayerLogout(Player* player)
{
    FuryEvent event = Base(player, "player.logout");
    event.dedupeIdentity = OccurrenceIdentity("logout", player);
    return event;
}

FuryEvent FuryEventFactory::LevelChanged(Player* player, uint8 oldLevel)
{
    FuryEvent event = Base(player, "player.level.changed");

    uint8 const newLevel = player ? player->GetLevel() : 0;
    event.subjectType = "level";
    event.subjectId = newLevel;
    event.payloadJson = Acore::StringFormat(
        "{{\"old_level\":{},\"new_level\":{},\"actor_level\":{}}}",
        oldLevel,
        newLevel, newLevel);

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
        "{{\"zone_id\":{},\"area_id\":{},\"actor_level\":{}}}",
        newZone,
        newArea, player ? player->GetLevel() : 0);
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

FuryEvent FuryEventFactory::LivingWorldEntityKilled(
    Player* creditedPlayer,
    Creature* creature,
    LivingWorldEntityMetadata const& metadata,
    ObjectGuid killerGuid,
    ActorKind finalBlowActorKind,
    bool directCredit,
    uint32 directParticipantCount)
{
    FuryEvent event = Base(
        creditedPlayer,
        "living_world.entity.killed");

    uint64 const creatureGuid =
        creature ? creature->GetGUID().GetRawValue() : 0;
    uint64 const creditedPlayerGuid =
        creditedPlayer
            ? creditedPlayer->GetGUID().GetRawValue()
            : 0;
    HouseholdId const householdId =
        event.actor.householdId.value_or(0);

    event.subjectType = "living_world_spawn_group";
    event.subjectId = metadata.spawnGroupId;
    event.sourceSystem = "living_world";
    event.correlationKey = Acore::StringFormat(
        "runtime:{}",
        metadata.runtimeId);
    event.payloadJson = Acore::StringFormat(
        "{{\"runtime_id\":{},\"runtime_group_id\":{},"
        "\"spawn_group_id\":{},\"member_id\":{},\"entry\":{},"
        "\"living_world_template_id\":{},\"tactical_role\":{},"
        "\"creature_guid\":{},\"killer_guid\":{},"
        "\"final_blow_actor_kind\":{},\"credited_player_guid\":{},"
        "\"credit_kind\":\"{}\",\"direct_participant_count\":{}}}",
        metadata.runtimeId,
        metadata.runtimeGroupId,
        metadata.spawnGroupId,
        metadata.memberId,
        metadata.entry,
        metadata.livingWorldTemplateId,
        metadata.tacticalRole,
        creatureGuid,
        killerGuid.GetRawValue(),
        static_cast<uint32>(finalBlowActorKind),
        creditedPlayerGuid,
        directCredit ? "direct" : "group_share",
        directParticipantCount);

    // One physical kill can credit each participating household once.
    // Household-scoped identity prevents the first household from globally
    // suppressing legitimate participation by another household.
    event.dedupeIdentity = Acore::StringFormat(
        "living-world:entity-killed:v2:{}:{}:{}:{}",
        metadata.runtimeId,
        metadata.runtimeGroupId,
        creatureGuid,
        householdId);

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

FuryEvent FuryEventFactory::ProfessionCrafted(
    Player* player,
    uint32 skillId,
    uint32 recipeSpellId,
    uint32 itemId,
    uint32 unitOrdinal,
    uint32 producedCount)
{
    FuryEvent event = Base(player, "profession.crafted");
    event.subjectType = "profession_craft";
    event.subjectId = MakeProfessionCraftTarget(skillId, itemId);
    event.payloadJson = Acore::StringFormat(
        "{{\"skill_id\":{},\"recipe_spell_id\":{},"
        "\"item_id\":{},\"unit_ordinal\":{},\"produced_count\":{}}}",
        skillId,
        recipeSpellId,
        itemId,
        unitOrdinal,
        producedCount);
    event.dedupeIdentity =
        OccurrenceIdentity("profession-craft", player);
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

    // Crafting may merge into an existing stack and therefore reuse an item
    // GUID. Treat each creation callback as a distinct occurrence.
    event.dedupeIdentity = OccurrenceIdentity("item-create", player);

    return event;
}
}
