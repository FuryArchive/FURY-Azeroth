#include "BestiaryService.h"

#include "core/FuryKey.h"
#include "events/EventStore.h"

#include "Log.h"
#include "StringFormat.h"

namespace Fury
{
BestiaryService::BestiaryService(
    BestiaryRepository const& repository,
    EventStore const& events)
    : _repository(repository),
      _events(events)
{
}

bool BestiaryService::Handle(FuryEvent const& event)
{
    if (event.type != "creature.killed")
        return true;

    if (!IsAuthorizedSource(event) ||
        event.subjectType != "creature" ||
        !event.subjectId)
    {
        return true;
    }

    uint32 const creatureEntry =
        static_cast<uint32>(*event.subjectId);

    std::vector<BestiaryMapping> mappings =
        _repository.FindMappings(creatureEntry);

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

    if (!EmitAdvancedEvent(source, entryKey, level))
        return {BestiaryAdvanceOutcome::PersistenceFailed, *after};

    bool const changed =
        !before ||
        static_cast<uint8>(before->discoveryLevel) <
            static_cast<uint8>(after->discoveryLevel);

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

    // Always attempt the idempotent level event. If the server crashed after
    // state persistence but before this append, replay repairs the missing
    // event without duplicating it.
    return EmitAdvancedEvent(
        source,
        mapping.entryKey,
        mapping.discoveryLevel);
}

bool BestiaryService::EmitAdvancedEvent(
    FuryEvent const& source,
    std::string_view entryKey,
    BestiaryDiscoveryLevel level) const
{
    FuryEvent event;
    event.type = "bestiary.entry.advanced";
    event.actor = source.actor;
    event.mapId = source.mapId;
    event.zoneId = source.zoneId;
    event.areaId = source.areaId;
    event.subjectType = "bestiary_entry";
    event.sourceSystem = "fury.bestiary";
    event.correlationKey = std::string(entryKey);
    event.dedupeIdentity = Acore::StringFormat(
        "bestiary:advance:v1:{}:{}:{}",
        source.actor.accountId,
        entryKey,
        static_cast<uint8>(level));
    event.payloadJson = Acore::StringFormat(
        "{{\"entry_key\":\"{}\",\"level\":{},"
        "\"source_event_id\":{}}}",
        entryKey,
        static_cast<uint8>(level),
        source.id);

    return _events.Append(event).has_value();
}

bool BestiaryService::IsAuthorizedSource(
    FuryEvent const& source)
{
    return source.id != 0 &&
        source.actor.accountId != 0 &&
        source.actor.isEligibleForPersistentProgression;
}
}
