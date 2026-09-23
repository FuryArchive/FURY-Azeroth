#include "DefiasParticipation.h"

#include "DefiasContent.h"
#include "DefiasContracts.h"
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
}

bool ParticipationService::ResolveRuntimeTarget(
    Creature* creature,
    LivingWorldEntityMetadata& metadata) const
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
    return true;
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
    if (!ResolveRuntimeTarget(creature, metadata))
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

                AddCredit(
                    credits,
                    member,
                    ResolveParticipationCredit(
                        candidate,
                        _rules),
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
