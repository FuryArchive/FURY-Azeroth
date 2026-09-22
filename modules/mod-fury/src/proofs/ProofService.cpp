#include "ProofService.h"
#include "ProofPolicy.h"

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

std::optional<ProofRecord> ProofService::Find(
    HouseholdId householdId,
    std::string_view proofKey) const
{
    if (!householdId || !IsCanonicalKey(proofKey))
        return std::nullopt;

    return _repository.Find(householdId, proofKey);
}

bool ProofService::Has(
    HouseholdId householdId,
    std::string_view proofKey) const
{
    return Find(householdId, proofKey).has_value();
}

ProofGrantResult ProofService::Grant(
    FuryEvent const& source,
    std::string_view proofKey,
    std::string_view metadataJson) const
{
    if (!source.id ||
        !source.actor.householdId ||
        !CanGrantHouseholdProof(source.actor.kind))
    {
        return {ProofGrantOutcome::InvalidSource, 0};
    }

    if (!IsCanonicalKey(proofKey))
        return {ProofGrantOutcome::InvalidProofKey, 0};

    HouseholdId const householdId = *source.actor.householdId;

    if (std::optional<ProofRecord> existing =
            _repository.Find(householdId, proofKey))
    {
        if (!EmitGrantedEvent(source, *existing))
        {
            return {
                ProofGrantOutcome::PersistenceFailed,
                existing->sourceEventId
            };
        }

        return {
            ProofGrantOutcome::AlreadyGranted,
            existing->sourceEventId
        };
    }

    _repository.InsertIgnore(
        householdId,
        proofKey,
        source.id,
        metadataJson);

    std::optional<ProofRecord> persisted =
        _repository.Find(householdId, proofKey);
    if (!persisted)
        return {ProofGrantOutcome::PersistenceFailed, 0};

    // INSERT IGNORE makes the household/key pair the arbitration point.
    // If another writer won concurrently, preserve that writer's original
    // source event and report this call as an idempotent duplicate.
    ProofGrantOutcome const outcome =
        persisted->sourceEventId == source.id
            ? ProofGrantOutcome::Granted
            : ProofGrantOutcome::AlreadyGranted;

    if (!EmitGrantedEvent(source, *persisted))
    {
        return {
            ProofGrantOutcome::PersistenceFailed,
            persisted->sourceEventId
        };
    }

    return {outcome, persisted->sourceEventId};
}

bool ProofService::EmitGrantedEvent(
    FuryEvent const& source,
    ProofRecord const& proof) const
{
    FuryEvent event;
    event.type = "proof.granted";
    event.actor = source.actor;
    event.mapId = source.mapId;
    event.zoneId = source.zoneId;
    event.areaId = source.areaId;
    event.subjectType = "proof";
    event.sourceSystem = "fury.proofs";
    event.correlationKey = proof.proofKey;
    event.dedupeIdentity = Acore::StringFormat(
        "proof:v1:{}:{}",
        proof.householdId,
        proof.proofKey);
    event.payloadJson = Acore::StringFormat(
        "{{\"proof_key\":\"{}\",\"source_event_id\":{},\"metadata\":{}}}",
        proof.proofKey,
        proof.sourceEventId,
        proof.metadataJson.empty() ? "{}" : proof.metadataJson);

    return _events.Append(event).has_value();
}
}

