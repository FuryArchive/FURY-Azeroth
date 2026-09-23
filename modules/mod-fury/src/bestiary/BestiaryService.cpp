#include "BestiaryService.h"
#include "BestiaryPolicy.h"

#include "content/defias/DefiasBestiary.h"
#include "content/defias/DefiasGraph.h"
#include "core/FuryKey.h"
#include "director/DirectorRepository.h"
#include "events/EventStore.h"

#include "Log.h"
#include "StringFormat.h"

namespace Fury
{
BestiaryService::BestiaryService(
    BestiaryRepository const& repository,
    EventStore const& events,
    DirectorRepository const& director)
    : _repository(repository),
      _events(events),
      _director(director)
{
}

bool BestiaryService::Handle(FuryEvent const& event)
{
    if (event.type == "director.run.resolved")
        return ApplyDefiasSuccessMastery(event);

    if (!IsAuthorizedSource(event) || !event.subjectId)
        return true;

    std::vector<BestiaryMapping> mappings;

    if (event.type == "creature.killed" &&
        event.subjectType == "creature")
    {
        mappings = _repository.FindMappings(
            static_cast<uint32>(*event.subjectId));
    }
    else if (event.type == "living_world.entity.killed" &&
             event.sourceSystem == "living_world" &&
             event.subjectType == "living_world_spawn_group")
    {
        mappings = _repository.FindEventMappings(
            event.type,
            event.subjectType,
            *event.subjectId);
    }
    else
    {
        return true;
    }

    for (BestiaryMapping const& mapping : mappings)
    {
        if (!ApplyMappedKill(event, mapping))
            return false;
    }

    return true;
}

BestiaryAdvanceResult BestiaryService::Promote(
    FuryEvent const& source,
    std::string_view entryKey,
    BestiaryDiscoveryLevel level) const
{
    if (!IsAuthorizedSource(source))
        return {BestiaryAdvanceOutcome::InvalidSource, {}};

    if (!IsCanonicalKey(entryKey))
        return {BestiaryAdvanceOutcome::InvalidKey, {}};

    if (!IsValidBestiaryLevel(level))
        return {BestiaryAdvanceOutcome::InvalidLevel, {}};

    uint32 const accountId = source.actor.accountId;

    std::optional<BestiaryState> before =
        _repository.FindState(accountId, entryKey);

    _repository.ApplyLevel(
        accountId,
        entryKey,
        level,
        source.id);

    std::optional<BestiaryState> after =
        _repository.FindState(accountId, entryKey);
    if (!after ||
        after->lastEventId < source.id ||
        static_cast<uint8>(after->discoveryLevel) <
            static_cast<uint8>(level))
    {
        return {BestiaryAdvanceOutcome::PersistenceFailed, {}};
    }

    bool const changed =
        !before ||
        static_cast<uint8>(before->discoveryLevel) <
            static_cast<uint8>(after->discoveryLevel);

    // If this level is exactly the durable state, retry the stable event even
    // when the state update was already persisted before a crash. Never emit a
    // lower-level "advanced" event after the account has already surpassed it.
    if (after->discoveryLevel == level &&
        !EmitAdvancedEvent(source, *after))
    {
        return {BestiaryAdvanceOutcome::PersistenceFailed, *after};
    }

    return {
        changed
            ? BestiaryAdvanceOutcome::Updated
            : BestiaryAdvanceOutcome::AlreadyApplied,
        *after
    };
}

bool BestiaryService::ApplyMappedKill(
    FuryEvent const& source,
    BestiaryMapping const& mapping) const
{
    if (!IsCanonicalKey(mapping.entryKey) ||
        !IsValidBestiaryLevel(mapping.discoveryLevel))
    {
        LOG_ERROR(
            "server.loading",
            "[FURY] invalid Bestiary mapping "
            "(entry={}, level={}, source_event={}).",
            mapping.entryKey,
            static_cast<uint8>(mapping.discoveryLevel),
            source.id);
        return false;
    }

    uint32 const accountId = source.actor.accountId;

    _repository.ApplyKill(
        accountId,
        mapping.entryKey,
        mapping.discoveryLevel,
        source.id);

    std::optional<BestiaryState> state =
        _repository.FindState(accountId, mapping.entryKey);
    if (!state ||
        state->lastEventId < source.id ||
        static_cast<uint8>(state->discoveryLevel) <
            static_cast<uint8>(mapping.discoveryLevel))
    {
        LOG_ERROR(
            "server.loading",
            "[FURY] Bestiary projection verification failed "
            "(account={}, entry={}, source_event={}).",
            accountId,
            mapping.entryKey,
            source.id);
        return false;
    }

    // Replay repairs a missing level event after state persistence. If the
    // account has already surpassed this mapping, do not emit a stale
    // lower-level transition.
    if (state->discoveryLevel == mapping.discoveryLevel)
        return EmitAdvancedEvent(source, *state);

    return true;
}

bool BestiaryService::ApplyDefiasSuccessMastery(
    FuryEvent const& source) const
{
    if (!source.id ||
        source.sourceSystem != "fury.director" ||
        source.subjectType != "director_run" ||
        !source.subjectId ||
        !source.actor.householdId ||
        !*source.actor.householdId ||
        source.correlationKey != Defias::GraphKey)
    {
        return true;
    }

    std::optional<DirectorRun> run =
        _director.FindRun(*source.subjectId);
    if (!run ||
        run->householdId != *source.actor.householdId ||
        run->graphKey != Defias::GraphKey ||
        run->status != DirectorRunStatus::Complete ||
        !run->outcomeKey ||
        *run->outcomeKey != "success")
    {
        return true;
    }

    std::vector<uint32> accounts =
        _repository.FindHouseholdAccountsAtLeastLevel(
            run->householdId,
            Defias::CommanderBestiaryEntry,
            BestiaryDiscoveryLevel::Studied);

    for (uint32 accountId : accounts)
    {
        _repository.ApplyLevel(
            accountId,
            Defias::CommanderBestiaryEntry,
            BestiaryDiscoveryLevel::Mastered,
            source.id);

        std::optional<BestiaryState> state =
            _repository.FindState(
                accountId,
                Defias::CommanderBestiaryEntry);

        if (!state ||
            state->lastEventId < source.id ||
            state->discoveryLevel !=
                BestiaryDiscoveryLevel::Mastered)
        {
            LOG_ERROR(
                "server.loading",
                "[FURY] Defias commander mastery verification failed "
                "(account={}, run={}, event={}).",
                accountId,
                run->id,
                source.id);
            return false;
        }

        if (!EmitAdvancedEvent(source, *state))
            return false;
    }

    return true;
}

bool BestiaryService::EmitAdvancedEvent(
    FuryEvent const& source,
    BestiaryState const& state) const
{
    FuryEvent event;
    event.type = "bestiary.entry.advanced";
    event.actor = source.actor;
    if (event.actor.accountId != state.accountId)
    {
        event.actor.kind = ActorKind::System;
        event.actor.characterGuid = ObjectGuid::Empty;
        event.actor.accountId = state.accountId;
        event.actor.isEligibleForPersistentProgression = false;
    }
    event.mapId = source.mapId;
    event.zoneId = source.zoneId;
    event.areaId = source.areaId;
    event.subjectType = "bestiary_entry";
    event.sourceSystem = "fury.bestiary";
    event.correlationKey = state.entryKey;
    event.dedupeIdentity = Acore::StringFormat(
        "bestiary:advance:v1:{}:{}:{}",
        state.accountId,
        state.entryKey,
        static_cast<uint8>(state.discoveryLevel));
    event.payloadJson = Acore::StringFormat(
        "{{\"entry_key\":\"{}\",\"level\":{},"
        "\"kill_count\":{},\"source_event_id\":{}}}",
        state.entryKey,
        static_cast<uint8>(state.discoveryLevel),
        state.killCount,
        source.id);

    return _events.Append(event).has_value();
}

bool BestiaryService::IsAuthorizedSource(
    FuryEvent const& source)
{
    return source.id != 0 &&
        source.actor.accountId != 0 &&
        CanAuthorBestiaryProgress(source.actor.kind);
}
}

