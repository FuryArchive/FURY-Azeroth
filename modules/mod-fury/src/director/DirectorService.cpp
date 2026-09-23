#include "DirectorService.h"
#include "DirectorPolicy.h"

#include "core/FuryKey.h"
#include "events/EventStore.h"

#include "StringFormat.h"

namespace Fury
{
namespace
{
bool IsTerminal(DirectorRunStatus status)
{
    return status == DirectorRunStatus::Complete ||
        status == DirectorRunStatus::Failed ||
        status == DirectorRunStatus::Aborted;
}
}

DirectorService::DirectorService(
    DirectorRepository const& repository,
    EventStore const& events)
    : _repository(repository),
      _events(events)
{
}

DirectorResult DirectorService::Start(
    FuryEvent const& source,
    std::string_view graphKey,
    std::string_view initialPhase) const
{
    if (!IsStartSource(source))
        return {DirectorOutcome::InvalidSource, std::nullopt};

    if (!IsCanonicalKey(graphKey) || !IsCanonicalKey(initialPhase))
        return {DirectorOutcome::InvalidKey, std::nullopt};

    std::optional<DirectorGraphDefinition> graph =
        _repository.FindGraph(graphKey);
    if (!graph)
        return {DirectorOutcome::GraphNotFound, std::nullopt};

    if (!graph->enabled)
        return {DirectorOutcome::GraphDisabled, std::nullopt};

    if (!IsCanonicalKey(graph->scopeKey))
        return {DirectorOutcome::InvalidKey, std::nullopt};

    HouseholdId const householdId = *source.actor.householdId;

    if (std::optional<DirectorRun> active =
            _repository.FindActiveScope(householdId, graph->scopeKey))
    {
        if (active->graphKey != graphKey)
            return {DirectorOutcome::ScopeBusy, active};

        if (!EmitStartedEvent(source, *active))
            return {DirectorOutcome::PersistenceFailed, active};

        return {DirectorOutcome::AlreadyActive, active};
    }

    _repository.InsertRun(
        householdId,
        *graph,
        initialPhase,
        source.id);

    std::optional<DirectorRun> persisted =
        _repository.FindActiveScope(householdId, graph->scopeKey);
    if (!persisted)
        return {DirectorOutcome::PersistenceFailed, std::nullopt};

    if (persisted->graphKey != graphKey)
        return {DirectorOutcome::ScopeBusy, persisted};

    if (!EmitStartedEvent(source, *persisted))
        return {DirectorOutcome::PersistenceFailed, persisted};

    DirectorOutcome const outcome =
        persisted->startedEventId == source.id
            ? DirectorOutcome::Started
            : DirectorOutcome::AlreadyActive;

    return {outcome, persisted};
}

DirectorResult DirectorService::AdvancePhase(
    FuryEvent const& source,
    DirectorRunId runId,
    uint64 expectedRevision,
    std::string_view phaseKey) const
{
    if (!IsCanonicalKey(phaseKey))
        return {DirectorOutcome::InvalidKey, std::nullopt};

    std::optional<DirectorRun> run = _repository.FindRun(runId);
    if (!run)
        return {DirectorOutcome::RunNotFound, std::nullopt};

    if (!IsMutationSource(source, run->householdId))
        return {DirectorOutcome::InvalidSource, run};

    if (IsTerminal(run->status) ||
        run->status == DirectorRunStatus::Resolving)
    {
        return {DirectorOutcome::InvalidState, run};
    }

    if (run->revision != expectedRevision)
    {
        if (run->phaseKey == phaseKey &&
            run->revision > expectedRevision)
        {
            if (!EmitPhaseEvent(source, *run))
                return {DirectorOutcome::PersistenceFailed, run};

            return {DirectorOutcome::AlreadyApplied, run};
        }

        return {DirectorOutcome::RevisionConflict, run};
    }

    if (run->phaseKey == phaseKey)
    {
        // A restart reloads the post-mutation revision. Repair an event append
        // that failed after the phase row was committed, even at that revision.
        if (!EmitPhaseEvent(source, *run))
            return {DirectorOutcome::PersistenceFailed, run};
        return {DirectorOutcome::AlreadyApplied, run};
    }

    _repository.UpdatePhase(
        runId,
        run->householdId,
        expectedRevision,
        phaseKey,
        source.id);

    std::optional<DirectorRun> persisted =
        _repository.FindRun(runId);
    if (!persisted)
        return {DirectorOutcome::PersistenceFailed, std::nullopt};

    if (persisted->phaseKey != phaseKey ||
        persisted->revision <= expectedRevision)
    {
        return {DirectorOutcome::RevisionConflict, persisted};
    }

    if (!EmitPhaseEvent(source, *persisted))
        return {DirectorOutcome::PersistenceFailed, persisted};

    return {DirectorOutcome::Updated, persisted};
}

DirectorResult DirectorService::AttachRuntime(
    FuryEvent const& source,
    DirectorRunId runId,
    uint64 expectedRevision,
    uint64 externalRuntimeId) const
{
    if (!externalRuntimeId)
        return {DirectorOutcome::InvalidKey, std::nullopt};

    std::optional<DirectorRun> run = _repository.FindRun(runId);
    if (!run)
        return {DirectorOutcome::RunNotFound, std::nullopt};

    if (!IsMutationSource(source, run->householdId))
        return {DirectorOutcome::InvalidSource, run};

    if (IsTerminal(run->status))
        return {DirectorOutcome::InvalidState, run};

    if (run->externalRuntimeId)
    {
        if (*run->externalRuntimeId != externalRuntimeId)
            return {DirectorOutcome::RuntimeConflict, run};

        if (!EmitRuntimeEvent(source, *run))
            return {DirectorOutcome::PersistenceFailed, run};

        return {DirectorOutcome::AlreadyApplied, run};
    }

    if (run->revision != expectedRevision)
        return {DirectorOutcome::RevisionConflict, run};

    _repository.AttachRuntime(
        runId,
        run->householdId,
        expectedRevision,
        externalRuntimeId,
        source.id);

    std::optional<DirectorRun> persisted =
        _repository.FindRun(runId);
    if (!persisted)
        return {DirectorOutcome::PersistenceFailed, std::nullopt};

    if (persisted->externalRuntimeId &&
        *persisted->externalRuntimeId != externalRuntimeId)
    {
        return {DirectorOutcome::RuntimeConflict, persisted};
    }

    if (!persisted->externalRuntimeId ||
        persisted->revision <= expectedRevision)
    {
        return {DirectorOutcome::RevisionConflict, persisted};
    }

    if (!EmitRuntimeEvent(source, *persisted))
        return {DirectorOutcome::PersistenceFailed, persisted};

    return {DirectorOutcome::Updated, persisted};
}

DirectorResult DirectorService::RecoverRuntime(
    FuryEvent const& source,
    DirectorRunId runId,
    uint64 expectedRevision,
    uint64 expectedRuntimeId,
    uint64 replacementRuntimeId) const
{
    if (!replacementRuntimeId)
        return {DirectorOutcome::InvalidKey, std::nullopt};

    std::optional<DirectorRun> run = _repository.FindRun(runId);
    if (!run)
        return {DirectorOutcome::RunNotFound, std::nullopt};

    if (!IsMutationSource(source, run->householdId))
        return {DirectorOutcome::InvalidSource, run};

    if (IsTerminal(run->status))
        return {DirectorOutcome::InvalidState, run};

    uint64 const currentRuntimeId =
        run->externalRuntimeId.value_or(0);

    if (currentRuntimeId == replacementRuntimeId)
    {
        if (!EmitRuntimeRecoveryEvent(
                source,
                *run,
                expectedRuntimeId))
        {
            return {DirectorOutcome::PersistenceFailed, run};
        }

        return {DirectorOutcome::AlreadyApplied, run};
    }

    if (currentRuntimeId != expectedRuntimeId)
        return {DirectorOutcome::RuntimeConflict, run};

    if (run->revision != expectedRevision)
        return {DirectorOutcome::RevisionConflict, run};

    _repository.RecoverRuntime(
        runId,
        run->householdId,
        expectedRevision,
        expectedRuntimeId,
        replacementRuntimeId,
        source.id);

    std::optional<DirectorRun> persisted =
        _repository.FindRun(runId);
    if (!persisted)
        return {DirectorOutcome::PersistenceFailed, std::nullopt};

    if (!persisted->externalRuntimeId ||
        *persisted->externalRuntimeId != replacementRuntimeId ||
        persisted->revision <= expectedRevision)
    {
        return {DirectorOutcome::RevisionConflict, persisted};
    }

    if (!EmitRuntimeRecoveryEvent(
            source,
            *persisted,
            expectedRuntimeId))
    {
        return {DirectorOutcome::PersistenceFailed, persisted};
    }

    return {DirectorOutcome::Updated, persisted};
}

DirectorResult DirectorService::Resolve(
    FuryEvent const& source,
    DirectorRunId runId,
    uint64 expectedRevision,
    std::string_view outcomeKey) const
{
    if (!IsCanonicalKey(outcomeKey))
        return {DirectorOutcome::InvalidKey, std::nullopt};

    std::optional<DirectorRun> run = _repository.FindRun(runId);
    if (!run)
        return {DirectorOutcome::RunNotFound, std::nullopt};

    if (!IsMutationSource(source, run->householdId))
        return {DirectorOutcome::InvalidSource, run};

    if (run->status == DirectorRunStatus::Complete)
    {
        if (!run->outcomeKey || *run->outcomeKey != outcomeKey)
            return {DirectorOutcome::InvalidState, run};

        std::optional<EventId> terminal =
            EmitTerminalEvent(source, *run, "director.run.resolved");
        if (!terminal)
            return {DirectorOutcome::PersistenceFailed, run};

        _repository.BindTerminalEvent(run->id, run->householdId, *terminal);
        std::optional<DirectorRun> rebound = _repository.FindRun(run->id);
        if (!rebound || rebound->resolvedEventId != terminal)
            return {DirectorOutcome::PersistenceFailed, rebound};

        return {DirectorOutcome::AlreadyApplied, rebound};
    }

    if (IsTerminal(run->status))
        return {DirectorOutcome::InvalidState, run};

    if (run->revision != expectedRevision)
        return {DirectorOutcome::RevisionConflict, run};

    _repository.Resolve(
        runId,
        run->householdId,
        expectedRevision,
        outcomeKey,
        source.id);

    std::optional<DirectorRun> persisted =
        _repository.FindRun(runId);
    if (!persisted)
        return {DirectorOutcome::PersistenceFailed, std::nullopt};

    if (persisted->status != DirectorRunStatus::Complete ||
        !persisted->outcomeKey ||
        *persisted->outcomeKey != outcomeKey ||
        persisted->revision <= expectedRevision)
    {
        return {DirectorOutcome::RevisionConflict, persisted};
    }

    std::optional<EventId> terminal =
        EmitTerminalEvent(source, *persisted, "director.run.resolved");
    if (!terminal)
        return {DirectorOutcome::PersistenceFailed, persisted};

    _repository.BindTerminalEvent(
        persisted->id,
        persisted->householdId,
        *terminal);

    std::optional<DirectorRun> rebound =
        _repository.FindRun(persisted->id);
    if (!rebound || rebound->resolvedEventId != terminal)
        return {DirectorOutcome::PersistenceFailed, rebound};

    return {DirectorOutcome::Updated, rebound};
}

DirectorResult DirectorService::Abort(
    FuryEvent const& source,
    DirectorRunId runId,
    uint64 expectedRevision,
    std::string_view outcomeKey) const
{
    if (!IsCanonicalKey(outcomeKey))
        return {DirectorOutcome::InvalidKey, std::nullopt};

    std::optional<DirectorRun> run = _repository.FindRun(runId);
    if (!run)
        return {DirectorOutcome::RunNotFound, std::nullopt};

    if (!IsMutationSource(source, run->householdId))
        return {DirectorOutcome::InvalidSource, run};

    if (run->status == DirectorRunStatus::Aborted)
    {
        if (!run->outcomeKey || *run->outcomeKey != outcomeKey)
            return {DirectorOutcome::InvalidState, run};

        std::optional<EventId> terminal =
            EmitTerminalEvent(source, *run, "director.run.aborted");
        if (!terminal)
            return {DirectorOutcome::PersistenceFailed, run};

        _repository.BindTerminalEvent(run->id, run->householdId, *terminal);
        std::optional<DirectorRun> rebound = _repository.FindRun(run->id);
        if (!rebound || rebound->resolvedEventId != terminal)
            return {DirectorOutcome::PersistenceFailed, rebound};

        return {DirectorOutcome::AlreadyApplied, rebound};
    }

    if (IsTerminal(run->status))
        return {DirectorOutcome::InvalidState, run};

    if (run->revision != expectedRevision)
        return {DirectorOutcome::RevisionConflict, run};

    _repository.Abort(
        runId,
        run->householdId,
        expectedRevision,
        outcomeKey,
        source.id);

    std::optional<DirectorRun> persisted =
        _repository.FindRun(runId);
    if (!persisted)
        return {DirectorOutcome::PersistenceFailed, std::nullopt};

    if (persisted->status != DirectorRunStatus::Aborted ||
        !persisted->outcomeKey ||
        *persisted->outcomeKey != outcomeKey ||
        persisted->revision <= expectedRevision)
    {
        return {DirectorOutcome::RevisionConflict, persisted};
    }

    std::optional<EventId> terminal =
        EmitTerminalEvent(source, *persisted, "director.run.aborted");
    if (!terminal)
        return {DirectorOutcome::PersistenceFailed, persisted};

    _repository.BindTerminalEvent(
        persisted->id,
        persisted->householdId,
        *terminal);

    std::optional<DirectorRun> rebound =
        _repository.FindRun(persisted->id);
    if (!rebound || rebound->resolvedEventId != terminal)
        return {DirectorOutcome::PersistenceFailed, rebound};

    return {DirectorOutcome::Updated, rebound};
}

std::optional<EventId> DirectorService::EmitStartedEvent(
    FuryEvent const& source,
    DirectorRun const& run) const
{
    FuryEvent event;
    event.type = "director.run.started";
    event.actor = source.actor;
    event.mapId = source.mapId;
    event.zoneId = source.zoneId;
    event.areaId = source.areaId;
    event.subjectType = "director_run";
    event.subjectId = run.id;
    event.sourceSystem = "fury.director";
    event.correlationKey = run.graphKey;
    event.dedupeIdentity = Acore::StringFormat(
        "director:start:v1:{}",
        run.id);
    event.payloadJson = Acore::StringFormat(
        "{{\"run_id\":{},\"graph_key\":\"{}\","
        "\"scope_key\":\"{}\",\"phase_key\":\"{}\","
        "\"started_event_id\":{}}}",
        run.id,
        run.graphKey,
        run.scopeKey,
        run.phaseKey,
        run.startedEventId);
    return _events.Append(event);
}

std::optional<EventId> DirectorService::EmitPhaseEvent(
    FuryEvent const& source,
    DirectorRun const& run) const
{
    FuryEvent event;
    event.type = "director.phase.changed";
    event.actor = source.actor;
    event.mapId = source.mapId;
    event.zoneId = source.zoneId;
    event.areaId = source.areaId;
    event.subjectType = "director_run";
    event.subjectId = run.id;
    event.sourceSystem = "fury.director";
    event.correlationKey = run.graphKey;
    event.dedupeIdentity = Acore::StringFormat(
        "director:phase:v1:{}:{}",
        run.id,
        run.revision);
    event.payloadJson = Acore::StringFormat(
        "{{\"run_id\":{},\"graph_key\":\"{}\","
        "\"phase_key\":\"{}\",\"revision\":{},"
        "\"source_event_id\":{}}}",
        run.id,
        run.graphKey,
        run.phaseKey,
        run.revision,
        run.lastEventId);
    return _events.Append(event);
}

std::optional<EventId> DirectorService::EmitRuntimeEvent(
    FuryEvent const& source,
    DirectorRun const& run) const
{
    if (!run.externalRuntimeId)
        return std::nullopt;

    FuryEvent event;
    event.type = "director.runtime.attached";
    event.actor = source.actor;
    event.mapId = source.mapId;
    event.zoneId = source.zoneId;
    event.areaId = source.areaId;
    event.subjectType = "director_run";
    event.subjectId = run.id;
    event.sourceSystem = "fury.director";
    event.correlationKey = run.graphKey;
    event.dedupeIdentity = Acore::StringFormat(
        "director:runtime:v1:{}:{}",
        run.id,
        *run.externalRuntimeId);
    event.payloadJson = Acore::StringFormat(
        "{{\"run_id\":{},\"graph_key\":\"{}\","
        "\"external_runtime_id\":{},\"revision\":{},"
        "\"source_event_id\":{}}}",
        run.id,
        run.graphKey,
        *run.externalRuntimeId,
        run.revision,
        run.lastEventId);
    return _events.Append(event);
}

std::optional<EventId> DirectorService::EmitRuntimeRecoveryEvent(
    FuryEvent const& source,
    DirectorRun const& run,
    uint64 previousRuntimeId) const
{
    if (!run.externalRuntimeId)
        return std::nullopt;

    FuryEvent event;
    event.type = "director.runtime.recovered";
    event.actor = source.actor;
    event.mapId = source.mapId;
    event.zoneId = source.zoneId;
    event.areaId = source.areaId;
    event.subjectType = "director_run";
    event.subjectId = run.id;
    event.sourceSystem = "fury.director";
    event.correlationKey = run.graphKey;
    event.dedupeIdentity = Acore::StringFormat(
        "director:runtime-recovered:v1:{}:{}",
        run.id,
        *run.externalRuntimeId);
    event.payloadJson = Acore::StringFormat(
        "{{\"run_id\":{},\"graph_key\":\"{}\","
        "\"previous_runtime_id\":{},\"external_runtime_id\":{},"
        "\"revision\":{},\"source_event_id\":{}}}",
        run.id,
        run.graphKey,
        previousRuntimeId,
        *run.externalRuntimeId,
        run.revision,
        run.lastEventId);
    return _events.Append(event);
}

std::optional<EventId> DirectorService::EmitTerminalEvent(
    FuryEvent const& source,
    DirectorRun const& run,
    std::string_view eventType) const
{
    if (!run.outcomeKey)
        return std::nullopt;

    FuryEvent event;
    event.type = std::string(eventType);
    event.actor = source.actor;
    event.mapId = source.mapId;
    event.zoneId = source.zoneId;
    event.areaId = source.areaId;
    event.subjectType = "director_run";
    event.subjectId = run.id;
    event.sourceSystem = "fury.director";
    event.correlationKey = run.graphKey;
    event.dedupeIdentity = Acore::StringFormat(
        "director:terminal:v1:{}",
        run.id);
    event.payloadJson = Acore::StringFormat(
        "{{\"run_id\":{},\"graph_key\":\"{}\","
        "\"outcome_key\":\"{}\",\"status\":{},"
        "\"revision\":{},\"source_event_id\":{}}}",
        run.id,
        run.graphKey,
        *run.outcomeKey,
        static_cast<uint8>(run.status),
        run.revision,
        run.lastEventId);
    return _events.Append(event);
}

bool DirectorService::IsStartSource(FuryEvent const& source)
{
    return source.id != 0 &&
        source.actor.householdId.has_value() &&
        CanStartDirectorRun(source.actor.kind);
}

bool DirectorService::IsMutationSource(
    FuryEvent const& source,
    HouseholdId householdId)
{
    return source.id != 0 &&
        source.actor.householdId &&
        *source.actor.householdId == householdId &&
        CanMutateDirectorRun(source.actor.kind);
}
}
