#include "ProofService.h"

#include "core/FuryKey.h"

#include "events/EventStore.h"
#include "StringFormat.h"

namespace Fury
{
ProofService::ProofService(
    ProofRepository const& repository,
    EventStore const& events)
    : _repository(repository),
      _events(events)
{
}

bool ProofService::Has(
    HouseholdId householdId,
    std::string_view proofKey) const
{
    return _repository.Find(householdId, proofKey).has_value();
}

ProofGrantResult ProofService::Grant(
    FuryEvent const& source,
    std::string_view proofKey,
    std::string_view metadataJson) const
{
    if (!source.id ||
        !source.actor.householdId ||
        !source.actor.isEligibleForPersistentProgression)
    {
        return {ProofGrantOutcome::InvalidSource, 0};
    }

    if (!IsCanonicalKey(proofKey))
        return {ProofGrantOutcome::InvalidProofKey, 0};

    HouseholdId const householdId = *source.actor.householdId;

    if (std::optional<ProofRecord> existing =
            _repository.Find(householdId, proofKey))
    {
        EmitGrantedEvent(
            source,
            proofKey,
            existing->sourceEventId,
            metadataJson);

        return {
            ProofGrantOutcome::AlreadyGranted,
            existing->sourceEventId
        };
    }

    _repository.Insert(
        householdId,
        proofKey,
        source.id,
        metadataJson);

    std::optional<ProofRecord> persisted =
        _repository.Find(householdId, proofKey);
    if (!persisted)
        return {ProofGrantOutcome::PersistenceFailed, 0};

    EmitGrantedEvent(
        source,
        proofKey,
        persisted->sourceEventId,
        metadataJson);

    return {
        ProofGrantOutcome::Granted,
        persisted->sourceEventId
    };
}

void ProofService::EmitGrantedEvent(
    FuryEvent const& source,
    std::string_view proofKey,
    EventId originalSourceEventId,
    std::string_view metadataJson) const
{
    if (!source.actor.householdId)
        return;

    HouseholdId const householdId = *source.actor.householdId;

    FuryEvent event;
    event.type = "proof.granted";
    event.actor = source.actor;
    event.mapId = source.mapId;
    event.zoneId = source.zoneId;
    event.areaId = source.areaId;
    event.subjectType = "proof";
    event.sourceSystem = "fury.proofs";
    event.correlationKey = std::string(proofKey);
    event.dedupeIdentity = Acore::StringFormat(
        "proof:v1:{}:{}",
        householdId,
        proofKey);
    event.payloadJson = Acore::StringFormat(
        "{{\"proof_key\":\"{}\",\"source_event_id\":{},\"metadata\":{}}}",
        proofKey,
        originalSourceEventId,
        metadataJson.empty() ? "{}" : metadataJson);

    _events.Append(event);
}
}
