#include "DefiasParticipation.h"

#include "DefiasContent.h"
#include "DefiasContracts.h"
#include "DefiasGraph.h"
#include "actors/ActorResolver.h"
#include "events/EventStore.h"
#include "events/FuryEventFactory.h"
#include "integrations/LivingWorldAdapter.h"

#include "Config.h"
#include "Creature.h"
#include "Group.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "StringFormat.h"
#include "Unit.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Fury::Defias
{
ParticipationService::ParticipationService(
    LivingWorldAdapter& livingWorld,
    ActorResolver& actors,
    EventStore const& events)
    : _livingWorld(livingWorld),
      _actors(actors),
      _events(events)
{
}

void ParticipationService::Initialize()
{
    Reset();

    _rules.windowSeconds = std::clamp(
        sConfigMgr->GetOption<uint32>(
            "Fury.Defias.ParticipationWindowSeconds",
            20),
        1u,
        300u);

    _rules.radiusYards = std::clamp(
        sConfigMgr->GetOption<float>(
            "Fury.Defias.ParticipationRadiusYards",
            60.0f),
        1.0f,
        200.0f);

    _rules.shareGroup = sConfigMgr->GetOption<bool>(
        "Fury.Defias.ParticipationGroupShare",
        true);

    LOG_INFO(
        "server.loading",
        "[FURY] Defias participation window={}s radius={}yd group_share={}.",
        _rules.windowSeconds,
        _rules.radiusYards,
        _rules.shareGroup);
}

void ParticipationService::Reset()
{
    _encounters.clear();
    _finalStagePresence.clear();
}

bool ParticipationService::ResolveRuntimeTarget(
    Creature* creature,
    LivingWorldEntityMetadata& metadata,
    LivingWorldRuntimeSnapshot* runtimeOut) const
{
    if (!creature)
        return false;

    std::optional<LivingWorldEntityMetadata> found =
        _livingWorld.FindEntity(creature->GetGUID());

    if (!found ||
        !IsHostileSpawnGroup(found->spawnGroupId))
    {
        return false;
    }

    std::optional<LivingWorldRuntimeSnapshot> runtime =
        _livingWorld.RuntimeForInvasion(InvasionId);

    if (!runtime ||
        !runtime->IsActive() ||
        runtime->runtimeId != found->runtimeId)
    {
        return false;
    }

    metadata = *found;
    if (runtimeOut)
        *runtimeOut = *runtime;
    return true;
}

bool ParticipationService::EmitFinalStagePresence(
    Player* player,
    LivingWorldEntityMetadata const& metadata,
    LivingWorldRuntimeSnapshot const& runtime)
{
    if (!player ||
        runtime.stageId != 1006 ||
        !runtime.runtimeId)
    {
        return true;
    }

    ActorContext actor = _actors.Resolve(player);
    if (actor.kind != ActorKind::Human ||
        !actor.householdId ||
        !*actor.householdId)
    {
        return true;
    }

    std::string const identity = Acore::StringFormat(
        "defias:final-stage-participation:v1:{}:{}",
        runtime.runtimeId,
        *actor.householdId);

    if (!_finalStagePresence.insert(identity).second)
        return true;

    FuryEvent event;
    event.type = "defias.final_stage.participated";
    event.actor = actor;
    event.mapId = player->GetMapId();
    event.zoneId = player->GetZoneId();
    event.areaId = player->GetAreaId();
    event.subjectType = "living_world_runtime";
    event.subjectId = runtime.runtimeId;
    event.sourceSystem = "fury.defias";
    event.correlationKey = GraphKey;
    event.dedupeIdentity = identity;
    event.payloadJson = Acore::StringFormat(
        "{{\"runtime_id\":{},\"stage_id\":{},"
        "\"spawn_group_id\":{},\"credited_player_guid\":{}}}",
        runtime.runtimeId,
        runtime.stageId,
        metadata.spawnGroupId,
        player->GetGUID().GetRawValue());

    if (_events.Append(event))
        return true;

    _finalStagePresence.erase(identity);
    return false;
}

void ParticipationService::ObserveDamage(
    Unit* attacker,
    Unit* victim,
    uint32 damage)
{
    if (!attacker || !victim || !damage)
        return;

    Creature* creature = victim->ToCreature();
    if (!creature)
        return;

    LivingWorldEntityMetadata metadata;
    if (!ResolveRuntimeTarget(creature, metadata))
        return;

    Player* player =
        attacker->GetCharmerOrOwnerPlayerOrPlayerItself();
    if (!player || !player->IsInWorld())
        return;

    ActorContext actor = _actors.Resolve(player);
    if (actor.kind != ActorKind::Human ||
        !actor.householdId ||
        !*actor.householdId)
    {
        return;
    }

    auto const now = std::chrono::steady_clock::now();
    Prune(now);

    uint64 const creatureGuid = creature->GetGUID().GetRawValue();
    Encounter& encounter = _encounters[creatureGuid];

    if (encounter.runtimeId != metadata.runtimeId ||
        encounter.runtimeGroupId != metadata.runtimeGroupId)
    {
        encounter = {};
        encounter.runtimeId = metadata.runtimeId;
        encounter.runtimeGroupId = metadata.runtimeGroupId;
        encounter.spawnGroupId = metadata.spawnGroupId;
    }

    Observation observation;
    observation.playerGuid = player->GetGUID();
    observation.householdId = *actor.householdId;
    if (Group* group = player->GetGroup())
        observation.groupGuid = group->GetGUID();
    observation.lastAction = now;

    encounter.directParticipants[player->GetGUID().GetRawValue()] =
        observation;
}

bool ParticipationService::EmitBestiaryParticipation(
    Player* player,
    Creature* creature,
    LivingWorldEntityMetadata const& metadata,
    ParticipationCreditKind kind)
{
    if (!player ||
        !creature ||
        kind == ParticipationCreditKind::None)
    {
        return true;
    }

    ActorContext actor = _actors.Resolve(player);
    if (actor.kind != ActorKind::Human ||
        !actor.accountId ||
        !actor.householdId ||
        !*actor.householdId)
    {
        return true;
    }

    uint64 const creatureGuid =
        creature->GetGUID().GetRawValue();

    FuryEvent event;
    event.type = "defias.bestiary.entity.participated";
    event.actor = actor;
    event.mapId = player->GetMapId();
    event.zoneId = player->GetZoneId();
    event.areaId = player->GetAreaId();
    event.subjectType = "living_world_spawn_group";
    event.subjectId = metadata.spawnGroupId;
    event.sourceSystem = "fury.defias";
    event.correlationKey = GraphKey;
    event.dedupeIdentity = Acore::StringFormat(
        "defias:bestiary-participation:v1:{}:{}:{}:{}",
        metadata.runtimeId,
        metadata.runtimeGroupId,
        creatureGuid,
        actor.accountId);
    event.payloadJson = Acore::StringFormat(
        "{{\"runtime_id\":{},\"runtime_group_id\":{},"
        "\"spawn_group_id\":{},\"member_id\":{},"
        "\"creature_guid\":{},\"account_id\":{},"
        "\"credit_kind\":\"{}\"}}",
        metadata.runtimeId,
        metadata.runtimeGroupId,
        metadata.spawnGroupId,
        metadata.memberId,
        creatureGuid,
        actor.accountId,
        kind == ParticipationCreditKind::Direct
            ? "direct"
            : "group_share");

    return _events.Append(event).has_value();
}

void ParticipationService::AddCredit(
    std::unordered_map<HouseholdId, HouseholdCredit>& credits,
    Player* player,
    ParticipationCreditKind kind,
    uint32 secondsSinceAnchorAction) const
{
    if (!player || kind == ParticipationCreditKind::None)
        return;

    ActorContext actor = _actors.Resolve(player);
    if (actor.kind != ActorKind::Human ||
        !actor.householdId ||
        !*actor.householdId)
    {
        return;
    }

    HouseholdCredit& credit = credits[*actor.householdId];

    bool const betterKind =
        BetterCredit(credit.kind, kind) != credit.kind;

    bool const sameKindEarlier =
        credit.kind == kind &&
        (!credit.player ||
         player->GetGUID().GetRawValue() <
             credit.player->GetGUID().GetRawValue());

    if (!credit.player || betterKind || sameKindEarlier)
    {
        credit.player = player;
        credit.kind = kind;
        credit.secondsSinceAnchorAction = secondsSinceAnchorAction;
    }
}

void ParticipationService::ObserveDeath(Unit* victim, Unit* killer)
{
    if (!victim)
        return;

    Creature* creature = victim->ToCreature();
    if (!creature)
        return;

    LivingWorldEntityMetadata metadata;
    LivingWorldRuntimeSnapshot runtime;
    if (!ResolveRuntimeTarget(
            creature,
            metadata,
            &runtime))
    {
        _encounters.erase(creature->GetGUID().GetRawValue());
        return;
    }

    uint64 const creatureGuid = creature->GetGUID().GetRawValue();
    auto encounterItr = _encounters.find(creatureGuid);
    if (encounterItr == _encounters.end())
        return;

    Encounter const encounter = encounterItr->second;
    _encounters.erase(encounterItr);

    if (encounter.runtimeId != metadata.runtimeId ||
        encounter.runtimeGroupId != metadata.runtimeGroupId)
    {
        return;
    }

    auto const now = std::chrono::steady_clock::now();
    std::unordered_map<HouseholdId, HouseholdCredit> credits;
    std::unordered_map<uint32, HouseholdCredit> individualCredits;
    std::vector<std::pair<Player*, uint32>> directPlayers;

    for (auto const& [guid, observation] :
         encounter.directParticipants)
    {
        (void)guid;

        auto const elapsed =
            std::chrono::duration_cast<std::chrono::seconds>(
                now - observation.lastAction).count();

        if (elapsed < 0)
            continue;

        Player* player =
            ObjectAccessor::FindPlayer(observation.playerGuid);
        if (!player ||
            !player->IsInWorld() ||
            !player->IsWithinDistInMap(
                creature,
                _rules.radiusYards))
        {
            continue;
        }

        ParticipationCandidate candidate;
        candidate.actorKind = _actors.Resolve(player).kind;
        candidate.secondsSinceAnchorAction =
            static_cast<uint32>(elapsed);
        candidate.distanceYards =
            player->GetDistance(creature);
        candidate.direct = true;

        ParticipationCreditKind kind =
            ResolveParticipationCredit(candidate, _rules);

        if (kind != ParticipationCreditKind::Direct)
            continue;

        AddCredit(
            credits,
            player,
            kind,
            candidate.secondsSinceAnchorAction);
        AddIndividualCredit(
            individualCredits,
            player,
            kind,
            candidate.secondsSinceAnchorAction);

        directPlayers.emplace_back(
            player,
            candidate.secondsSinceAnchorAction);
    }

    if (_rules.shareGroup)
    {
        for (auto const& [directPlayer, age] : directPlayers)
        {
            Group* group = directPlayer->GetGroup();
            if (!group)
                continue;

            auto observationItr =
                encounter.directParticipants.find(
                    directPlayer->GetGUID().GetRawValue());

            if (observationItr ==
                    encounter.directParticipants.end() ||
                observationItr->second.groupGuid.IsEmpty() ||
                group->GetGUID() !=
                    observationItr->second.groupGuid)
            {
                continue;
            }

            for (Group::MemberSlot const& slot :
                 group->GetMemberSlots())
            {
                Player* member =
                    ObjectAccessor::FindPlayer(slot.guid);

                if (!member ||
                    !member->IsInWorld() ||
                    !member->IsWithinDistInMap(
                        creature,
                        _rules.radiusYards))
                {
                    continue;
                }

                ParticipationCandidate candidate;
                candidate.actorKind =
                    _actors.Resolve(member).kind;
                candidate.secondsSinceAnchorAction = age;
                candidate.distanceYards =
                    member->GetDistance(creature);
                candidate.linkedToDirectGroup = true;

                ParticipationCreditKind kind =
                    ResolveParticipationCredit(
                        candidate,
                        _rules);

                AddCredit(
                    credits,
                    member,
                    kind,
                    age);
                AddIndividualCredit(
                    individualCredits,
                    member,
                    kind,
                    age);
            }
        }
    }

    if (credits.empty())
        return;

    uint32 const directParticipantCount =
        static_cast<uint32>(directPlayers.size());

    ObjectGuid killerGuid =
        killer ? killer->GetGUID() : ObjectGuid::Empty;

    ActorKind finalBlowActorKind = ActorKind::System;
    if (killer)
    {
        if (Player* killerPlayer =
                killer->GetCharmerOrOwnerPlayerOrPlayerItself())
        {
            finalBlowActorKind =
                _actors.Resolve(killerPlayer).kind;
        }
    }

    for (auto const& [accountId, credit] : individualCredits)
    {
        (void)accountId;

        if (!credit.player)
            continue;

        if (!EmitBestiaryParticipation(
                credit.player,
                creature,
                metadata,
                credit.kind))
        {
            LOG_ERROR(
                "server.loading",
                "[FURY] failed to persist Defias Bestiary participation "
                "(account={}, runtime={}, creature={}).",
                accountId,
                metadata.runtimeId,
                creatureGuid);
        }
    }

    for (auto const& [householdId, credit] : credits)
    {
        if (!credit.player)
            continue;

        FuryEvent event =
            FuryEventFactory::LivingWorldEntityKilled(
                credit.player,
                creature,
                metadata,
                killerGuid,
                finalBlowActorKind,
                credit.kind == ParticipationCreditKind::Direct,
                directParticipantCount);

        if (!_events.Append(event))
        {
            LOG_ERROR(
                "server.loading",
                "[FURY] failed to persist Defias participation credit "
                "(household={}, runtime={}, creature={}).",
                householdId,
                metadata.runtimeId,
                creatureGuid);
        }

        if (!EmitFinalStagePresence(
                credit.player,
                metadata,
                runtime))
        {
            LOG_ERROR(
                "server.loading",
                "[FURY] failed to persist final-stage participation "
                "(household={}, runtime={}).",
                householdId,
                metadata.runtimeId);
        }
    }
}

void ParticipationService::AddIndividualCredit(
    std::unordered_map<uint32, HouseholdCredit>& credits,
    Player* player,
    ParticipationCreditKind kind,
    uint32 secondsSinceAnchorAction) const
{
    if (!player || kind == ParticipationCreditKind::None)
        return;

    ActorContext actor = _actors.Resolve(player);
    if (actor.kind != ActorKind::Human || !actor.accountId)
        return;

    HouseholdCredit& credit = credits[actor.accountId];

    bool const betterKind =
        BetterCredit(credit.kind, kind) != credit.kind;

    bool const sameKindEarlier =
        credit.kind == kind &&
        (!credit.player ||
         player->GetGUID().GetRawValue() <
             credit.player->GetGUID().GetRawValue());

    if (!credit.player || betterKind || sameKindEarlier)
    {
        credit.player = player;
        credit.kind = kind;
        credit.secondsSinceAnchorAction = secondsSinceAnchorAction;
    }
}

void ParticipationService::Prune(
    std::chrono::steady_clock::time_point now)
{
    auto const maximumAge =
        std::chrono::seconds(_rules.windowSeconds);

    for (auto encounterItr = _encounters.begin();
         encounterItr != _encounters.end();)
    {
        Encounter& encounter = encounterItr->second;

        for (auto participantItr =
                 encounter.directParticipants.begin();
             participantItr !=
                 encounter.directParticipants.end();)
        {
            if (now - participantItr->second.lastAction >
                maximumAge)
            {
                participantItr =
                    encounter.directParticipants.erase(
                        participantItr);
            }
            else
            {
                ++participantItr;
            }
        }

        if (encounter.directParticipants.empty())
            encounterItr = _encounters.erase(encounterItr);
        else
            ++encounterItr;
    }
}
}
