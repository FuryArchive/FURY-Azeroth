#include "DefiasRecoveryService.h"

#include "DefiasContent.h"
#include "DefiasGraph.h"
#include "director/DirectorReconciliation.h"
#include "director/DirectorRepository.h"
#include "director/DirectorService.h"
#include "events/EventStore.h"
#include "integrations/LivingWorldAdapter.h"

#include "Log.h"
#include "StringFormat.h"

namespace Fury::Defias
{
RecoveryService::RecoveryService(
    DirectorService& director,
    DirectorRepository const& repository,
    LivingWorldAdapter& livingWorld,
    EventStore const& events)
    : _director(director),
      _repository(repository),
      _livingWorld(livingWorld),
      _events(events)
{
}

std::optional<FuryEvent> RecoveryService::EmitRecoveryEvent(
    DirectorRun const& run,
    std::string_view action,
    uint64 runtimeId) const
{
    FuryEvent event;
    event.type = Acore::StringFormat(
        "director.recovery.{}",
        action);
    event.actor.kind = ActorKind::System;
    event.actor.householdId = run.householdId;
    event.subjectType = "director_run";
    event.subjectId = run.id;
    event.sourceSystem = "fury.recovery";
    event.correlationKey = run.graphKey;
    event.dedupeIdentity = Acore::StringFormat(
        "director:recovery:v1:{}:{}:{}",
        run.id,
        action,
        runtimeId);
    event.payloadJson = Acore::StringFormat(
        "{{\"run_id\":{},\"graph_key\":\"{}\","
        "\"action\":\"{}\",\"runtime_id\":{},"
        "\"expected_runtime_id\":{},\"revision\":{}}}",
        run.id,
        run.graphKey,
        action,
        runtimeId,
        run.externalRuntimeId.value_or(0),
        run.revision);

    std::optional<EventId> id = _events.Append(event);
    if (!id)
        return std::nullopt;

    event.id = *id;
    return event;
}

bool RecoveryService::HandleRun(
    DirectorRun const& run) const
{
    ExternalRuntimeSnapshot const external =
        _livingWorld.Inspect(run);
    DirectorReconcileAction const action =
        DirectorReconciliationService::Evaluate(
            run,
            external);

    uint64 const actualRuntimeId =
        external.runtimeId.value_or(0);
    uint64 const expectedRuntimeId =
        run.externalRuntimeId.value_or(0);

    switch (action)
    {
        case DirectorReconcileAction::None:
            return true;

        case DirectorReconcileAction::AttachExternalRuntime:
        {
            std::optional<FuryEvent> source =
                EmitRecoveryEvent(
                    run,
                    "attach_existing",
                    actualRuntimeId);
            if (!source)
                return false;

            DirectorResult const result =
                _director.AttachRuntime(
                    *source,
                    run.id,
                    run.revision,
                    actualRuntimeId);

            if (!result.Accepted())
                return false;

            LOG_INFO(
                "server.loading",
                "[FURY Recovery] Director run {} attached existing Living World runtime {}.",
                run.id,
                actualRuntimeId);
            return true;
        }

        case DirectorReconcileAction::ReattachExistingRuntime:
        {
            if (!EmitRecoveryEvent(
                    run,
                    "verified",
                    actualRuntimeId))
            {
                return false;
            }

            LOG_DEBUG(
                "server.loading",
                "[FURY Recovery] Director run {} and Living World runtime {} are coherent.",
                run.id,
                actualRuntimeId);
            return true;
        }

        case DirectorReconcileAction::RestartMissingRuntime:
        {
            std::optional<FuryEvent> source =
                EmitRecoveryEvent(
                    run,
                    "restart_missing",
                    expectedRuntimeId);
            if (!source)
                return false;

            LivingWorldStartResult const started =
                _livingWorld.StartInvasion(
                    *source,
                    InvasionId);
            if (!started.Accepted() || !started.runtime)
                return false;

            uint64 const replacementRuntimeId =
                started.runtime->runtimeId;

            DirectorResult const rebound =
                _director.RecoverRuntime(
                    *source,
                    run.id,
                    run.revision,
                    expectedRuntimeId,
                    replacementRuntimeId);
            if (!rebound.Accepted())
                return false;

            LOG_WARN(
                "server.loading",
                "[FURY Recovery] Director run {} replaced missing Living World runtime {} with {}.",
                run.id,
                expectedRuntimeId,
                replacementRuntimeId);
            return true;
        }

        case DirectorReconcileAction::AdoptReplacementRuntime:
        {
            std::optional<FuryEvent> source =
                EmitRecoveryEvent(
                    run,
                    "adopt_replacement",
                    actualRuntimeId);
            if (!source)
                return false;

            DirectorResult const rebound =
                _director.RecoverRuntime(
                    *source,
                    run.id,
                    run.revision,
                    expectedRuntimeId,
                    actualRuntimeId);
            if (!rebound.Accepted())
                return false;

            LOG_WARN(
                "server.loading",
                "[FURY Recovery] Director run {} adopted active replacement Living World runtime {} (previous {}).",
                run.id,
                actualRuntimeId,
                expectedRuntimeId);
            return true;
        }

        case DirectorReconcileAction::ResolveFromExternal:
        case DirectorReconcileAction::FailFromExternal:
        {
            // PollManagedRuntimes() has already durably normalized this
            // terminal executor state. The EventBus/Defias graph owns the
            // phase/outcome transition; reconciliation must not race it.
            return EmitRecoveryEvent(
                run,
                action == DirectorReconcileAction::ResolveFromExternal
                    ? "terminal_observed"
                    : "failure_observed",
                actualRuntimeId).has_value();
        }

        case DirectorReconcileAction::RuntimeConflict:
            LOG_ERROR(
                "server.loading",
                "[FURY Recovery] unresolved runtime conflict for Director run {}: expected {}, observed {}.",
                run.id,
                expectedRuntimeId,
                actualRuntimeId);
            return EmitRecoveryEvent(
                run,
                "runtime_conflict",
                actualRuntimeId).has_value();
    }

    return false;
}

bool RecoveryService::HandleOrphanRuntime(
    uint64 runtimeId) const
{
    FuryEvent event;
    event.type = "director.recovery.orphan_runtime";
    event.actor.kind = ActorKind::System;
    event.subjectType = "living_world_runtime";
    event.subjectId = runtimeId;
    event.sourceSystem = "fury.recovery";
    event.correlationKey = GraphKey;
    event.dedupeIdentity = Acore::StringFormat(
        "director:recovery:orphan:v1:{}",
        runtimeId);
    event.payloadJson = Acore::StringFormat(
        "{{\"runtime_id\":{},\"invasion_id\":{}}}",
        runtimeId,
        InvasionId);

    if (!_events.Append(event))
        return false;

    if (!_livingWorld.AbortRuntime(
            runtimeId,
            "FURY orphan runtime reconciliation"))
    {
        return false;
    }

    LOG_WARN(
        "server.loading",
        "[FURY Recovery] aborted orphan Living World runtime {} for Defias invasion {}.",
        runtimeId,
        InvasionId);
    return true;
}

bool RecoveryService::Reconcile() const
{
    _livingWorld.PollManagedRuntimes();

    bool hasActiveDefiasRun = false;
    bool ok = true;

    for (DirectorRun const& run :
         _repository.LoadActiveRuns())
    {
        if (run.graphKey != GraphKey)
            continue;

        hasActiveDefiasRun = true;
        if (!HandleRun(run))
            ok = false;
    }

    std::optional<LivingWorldRuntimeSnapshot> runtime =
        _livingWorld.RuntimeForInvasion(InvasionId);

    if (!hasActiveDefiasRun &&
        runtime &&
        runtime->IsActive())
    {
        if (!HandleOrphanRuntime(runtime->runtimeId))
            ok = false;
    }

    return ok;
}
}
