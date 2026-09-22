#include "ContractService.h"

#include "core/FuryKey.h"
#include "events/EventStore.h"

#include "Log.h"
#include "StringFormat.h"

namespace Fury
{
ContractService::ContractService(
    ContractRepository const& repository,
    EventStore const& events)
    : _repository(repository),
      _events(events)
{
}

ContractAcceptResult ContractService::Accept(
    FuryEvent const& source,
    std::string_view contractKey,
    std::optional<DirectorRunId> directorRunId) const
{
    if (!IsAuthorizedSource(source))
        return {ContractAcceptOutcome::InvalidSource, std::nullopt};

    if (!IsCanonicalKey(contractKey))
        return {ContractAcceptOutcome::InvalidKey, std::nullopt};

    HouseholdId const householdId = *source.actor.householdId;

    std::optional<ContractDefinition> definition =
        _repository.FindDefinition(contractKey);
    if (!definition)
        return {ContractAcceptOutcome::NotFound, std::nullopt};

    if (!definition->enabled)
        return {ContractAcceptOutcome::Disabled, std::nullopt};

    if (definition->repeatPolicy == ContractRepeatPolicy::DirectorRun &&
        !directorRunId)
    {
        return {ContractAcceptOutcome::DirectorRunRequired, std::nullopt};
    }

    if (std::optional<ContractInstance> active =
            _repository.FindActiveInstance(householdId, contractKey))
    {
        // Heal a crash after instance creation but before progress rows or the
        // audit event were written.
        _repository.InitializeProgress(active->id, active->contractKey);
        if (!EmitAcceptedEvent(source, *active))
            return {ContractAcceptOutcome::PersistenceFailed, active->id};

        return {ContractAcceptOutcome::AlreadyActive, active->id};
    }

    if (definition->repeatPolicy == ContractRepeatPolicy::Once &&
        _repository.HasCompletedInstance(householdId, contractKey))
    {
        return {ContractAcceptOutcome::AlreadyCompleted, std::nullopt};
    }

    if (_repository.CountObjectives(contractKey) == 0)
        return {ContractAcceptOutcome::MissingObjectives, std::nullopt};

    _repository.InsertActiveInstance(
        householdId,
        contractKey,
        directorRunId,
        source.id);

    std::optional<ContractInstance> persisted =
        _repository.FindActiveInstance(householdId, contractKey);
    if (!persisted)
        return {ContractAcceptOutcome::PersistenceFailed, std::nullopt};

    _repository.InitializeProgress(persisted->id, persisted->contractKey);

    if (!EmitAcceptedEvent(source, *persisted))
        return {ContractAcceptOutcome::PersistenceFailed, persisted->id};

    return {ContractAcceptOutcome::Accepted, persisted->id};
}

bool ContractService::Handle(FuryEvent const& event)
{
    if (!event.id)
        return true;

    if (!IsAuthorizedSource(event))
        return true;

    std::vector<ContractObjectiveMatch> matches =
        _repository.FindMatchingObjectives(event);

    for (ContractObjectiveMatch const& objective : matches)
    {
        if (objective.lastEventId >= event.id)
            continue;

        _repository.AdvanceObjective(objective, event.id);

        std::optional<ContractObjectiveProgress> progress =
            _repository.FindProgress(objective.instanceId, objective.ordinal);
        if (!progress)
        {
            LOG_ERROR(
                "server.loading",
                "[FURY] contract objective verification failed "
                "(instance={}, objective={}, event={}).",
                objective.instanceId,
                objective.ordinal,
                event.id);
            return false;
        }

        if (progress->lastEventId < event.id)
        {
            LOG_ERROR(
                "server.loading",
                "[FURY] contract objective did not persist event "
                "(instance={}, objective={}, expected_event={}, actual_event={}).",
                objective.instanceId,
                objective.ordinal,
                event.id,
                progress->lastEventId);
            return false;
        }

        std::optional<uint32> incomplete =
            _repository.CountIncompleteObjectives(objective.instanceId);
        if (!incomplete)
        {
            LOG_ERROR(
                "server.loading",
                "[FURY] could not verify contract completion "
                "(instance={}, event={}).",
                objective.instanceId,
                event.id);
            return false;
        }

        if (*incomplete != 0)
            continue;

        std::optional<EventId> completionEventId =
            EmitCompletedEvent(event, objective);
        if (!completionEventId)
        {
            LOG_ERROR(
                "server.loading",
                "[FURY] could not persist contract completion event "
                "(instance={}, source_event={}).",
                objective.instanceId,
                event.id);
            return false;
        }

        _repository.MarkComplete(
            objective.instanceId,
            *completionEventId);

        std::optional<ContractInstance> completed =
            _repository.FindInstance(objective.instanceId);
        if (!completed ||
            completed->status != ContractStatus::Complete ||
            completed->completedEventId != completionEventId)
        {
            LOG_ERROR(
                "server.loading",
                "[FURY] contract completion state verification failed "
                "(instance={}, completion_event={}).",
                objective.instanceId,
                *completionEventId);
            return false;
        }
    }

    return true;
}

std::optional<EventId> ContractService::EmitAcceptedEvent(
    FuryEvent const& source,
    ContractInstance const& instance) const
{
    FuryEvent event;
    event.type = "contract.accepted";
    event.actor = source.actor;
    event.mapId = source.mapId;
    event.zoneId = source.zoneId;
    event.areaId = source.areaId;
    event.subjectType = "contract_instance";
    event.subjectId = instance.id;
    event.sourceSystem = "fury.contracts";
    event.correlationKey = instance.contractKey;
    event.dedupeIdentity = Acore::StringFormat(
        "contract:accepted:v1:{}",
        instance.id);
    event.payloadJson = Acore::StringFormat(
        "{{\"instance_id\":{},\"contract_key\":\"{}\","
        "\"source_event_id\":{}}}",
        instance.id,
        instance.contractKey,
        source.id);

    return _events.Append(event);
}

std::optional<EventId> ContractService::EmitCompletedEvent(
    FuryEvent const& source,
    ContractObjectiveMatch const& objective) const
{
    FuryEvent event;
    event.type = "contract.completed";
    event.actor = source.actor;
    event.mapId = source.mapId;
    event.zoneId = source.zoneId;
    event.areaId = source.areaId;
    event.subjectType = "contract_instance";
    event.subjectId = objective.instanceId;
    event.sourceSystem = "fury.contracts";
    event.correlationKey = objective.contractKey;
    event.dedupeIdentity = Acore::StringFormat(
        "contract:completed:v1:{}",
        objective.instanceId);
    event.payloadJson = Acore::StringFormat(
        "{{\"instance_id\":{},\"contract_key\":\"{}\","
        "\"source_event_id\":{}}}",
        objective.instanceId,
        objective.contractKey,
        source.id);

    return _events.Append(event);
}

bool ContractService::IsAuthorizedSource(FuryEvent const& source)
{
    return source.id != 0 &&
        source.actor.householdId.has_value() &&
        source.actor.isEligibleForPersistentProgression;
}
}
