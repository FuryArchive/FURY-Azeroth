#include "DefiasRecoveryService.h"

#include "DefiasContent.h"
#include "DefiasGraph.h"
#include "director/DirectorService.h"
#include "events/EventStore.h"
#include "integrations/LivingWorldAdapter.h"

#include "Log.h"
#include "StringFormat.h"

#include <boost/bind/placeholders.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <sstream>
#include <string>

namespace Fury::Defias
{
namespace
{
std::optional<uint64> Number(
    std::string const& json,
    char const* field)
{
    try
    {
        boost::property_tree::ptree tree;
        std::istringstream stream(json);
        boost::property_tree::read_json(stream, tree);
        auto value = tree.get_optional<uint64>(field);
        if (value)
            return *value;
    }
    catch (boost::property_tree::ptree_error const&)
    {
    }

    return std::nullopt;
}

char const* ActionName(DirectorReconcileAction action)
{
    switch (action)
    {
        case DirectorReconcileAction::None:
            return "none";
        case DirectorReconcileAction::AttachExternalRuntime:
            return "attach";
        case DirectorReconcileAction::ReattachExistingRuntime:
            return "verified";
        case DirectorReconcileAction::AbortMissingRuntime:
            return "abort_missing";
        case DirectorReconcileAction::ResolveFromExternal:
            return "external_complete";
        case DirectorReconcileAction::FailFromExternal:
            return "external_failed";
        case DirectorReconcileAction::RuntimeConflict:
            return "abort_conflict";
    }

    return "unknown";
}
}

RecoveryService::RecoveryService(
    DirectorReconciliationService& reconciliation,
    DirectorService& director,
    LivingWorldAdapter& livingWorld,
    EventStore const& events)
    : _reconciliation(reconciliation),
      _director(director),
      _livingWorld(livingWorld),
      _events(events)
{
}

void RecoveryService::Tick()
{
    // Poll first. Complete/failed external states are recovered by the normal
    // durable Living World observation -> Defias graph event path.
    _livingWorld.PollManagedRuntimes();

    std::vector<DirectorReconcileDecision> const plan =
        _reconciliation.BuildPlan(_livingWorld);

    bool hasActiveDefiasRun = false;

    for (DirectorReconcileDecision const& decision : plan)
    {
        if (decision.run.graphKey != GraphKey)
            continue;

        hasActiveDefiasRun = true;

        if (!QueueDecision(decision))
        {
            LOG_ERROR(
                "server.loading",
                "[FURY] failed to queue Defias recovery action '{}' "
                "(run={}, expected_runtime={}, actual_runtime={}).",
                ActionName(decision.action),
                decision.run.id,
                decision.run.externalRuntimeId.value_or(0),
                decision.external.runtimeId.value_or(0));
        }
    }

    // T24 disables random start for this invasion. Therefore a managed Defias
    // runtime with no active Defias Director run is an orphan (including a
    // terminal Director run or a missing Director row) and may be failed
    // safely through the patched Living World cleanup path.
    if (!hasActiveDefiasRun)
    {
        std::optional<LivingWorldRuntimeSnapshot> runtime =
            _livingWorld.RuntimeForInvasion(InvasionId);

        if (runtime && runtime->runtimeId)
        {
            if (!QueueOrphanRuntime(runtime->runtimeId))
            {
                LOG_ERROR(
                    "server.loading",
                    "[FURY] failed to queue orphan Defias runtime cleanup "
                    "(runtime={}).",
                    runtime->runtimeId);
            }
        }
    }
}

bool RecoveryService::QueueDecision(
    DirectorReconcileDecision const& decision) const
{
    uint64 const runtimeId =
        decision.external.runtimeId.value_or(0);

    switch (decision.action)
    {
        case DirectorReconcileAction::None:
            return true;

        case DirectorReconcileAction::AttachExternalRuntime:
            LOG_WARN(
                "server.loading",
                "[FURY] Defias recovery found unbound active runtime {} "
                "for Director run {}; queueing attach.",
                runtimeId,
                decision.run.id);
            return QueueRunAction(
                decision.run,
                "director.recovery.attach_runtime",
                runtimeId,
                decision.action);

        case DirectorReconcileAction::ReattachExistingRuntime:
            return QueueRunAction(
                decision.run,
                "director.recovery.verified",
                runtimeId,
                decision.action);

        case DirectorReconcileAction::AbortMissingRuntime:
            LOG_WARN(
                "server.loading",
                "[FURY] Defias recovery found missing bound runtime {} "
                "for Director run {}; queueing safe abort.",
                decision.run.externalRuntimeId.value_or(0),
                decision.run.id);
            return QueueRunAction(
                decision.run,
                "director.recovery.abort_missing",
                decision.run.externalRuntimeId.value_or(0),
                decision.action);

        case DirectorReconcileAction::RuntimeConflict:
            LOG_ERROR(
                "server.loading",
                "[FURY] Defias recovery runtime conflict for run {} "
                "(expected={}, actual={}); queueing external cleanup + abort.",
                decision.run.id,
                decision.run.externalRuntimeId.value_or(0),
                runtimeId);
            return QueueRunAction(
                decision.run,
                "director.recovery.abort_conflict",
                runtimeId,
                decision.action);

        case DirectorReconcileAction::ResolveFromExternal:
        case DirectorReconcileAction::FailFromExternal:
            // PollManagedRuntimes() has already persisted the authoritative
            // external state. The normal event stream owns resolution so
            // recovery never invents a second terminal path.
            return true;
    }

    return false;
}

bool RecoveryService::QueueRunAction(
    DirectorRun const& run,
    std::string_view eventType,
    uint64 runtimeId,
    DirectorReconcileAction action) const
{
    FuryEvent event;
    event.type = std::string(eventType);
    event.actor.kind = ActorKind::System;
    event.actor.householdId = run.householdId;
    event.subjectType = "director_run";
    event.subjectId = run.id;
    event.sourceSystem = "fury.recovery";
    event.correlationKey = GraphKey;
    event.dedupeIdentity = Acore::StringFormat(
        "director:recovery:v1:{}:{}:{}",
        run.id,
        static_cast<uint32>(action),
        runtimeId);
    event.payloadJson = Acore::StringFormat(
        "{{\"run_id\":{},\"action\":{},"
        "\"expected_runtime_id\":{},\"runtime_id\":{}}}",
        run.id,
        static_cast<uint32>(action),
        run.externalRuntimeId.value_or(0),
        runtimeId);

    return _events.Append(event).has_value();
}

bool RecoveryService::QueueOrphanRuntime(
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

    LOG_WARN(
        "server.loading",
        "[FURY] Defias recovery found orphan managed runtime {}; "
        "queueing Living World cleanup.",
        runtimeId);

    return _events.Append(event).has_value();
}

std::optional<uint64> RecoveryService::RuntimeId(
    FuryEvent const& event)
{
    return Number(event.payloadJson, "runtime_id");
}

bool RecoveryService::HandleRunAction(
    FuryEvent const& event,
    std::string_view outcomeKey,
    bool failExternalRuntime) const
{
    if (!event.subjectId ||
        !event.actor.householdId)
    {
        return true;
    }

    std::optional<DirectorRun> run =
        _director.FindRun(*event.subjectId);
    if (!run ||
        run->graphKey != GraphKey ||
        run->householdId != *event.actor.householdId)
    {
        return true;
    }

    if (run->status == DirectorRunStatus::Complete ||
        run->status == DirectorRunStatus::Failed)
    {
        // Another authoritative path won the race. The recovery request is
        // stale and must not block the event-stream checkpoint.
        return true;
    }

    if (failExternalRuntime)
    {
        uint64 const runtimeId =
            RuntimeId(event).value_or(0);

        if (runtimeId &&
            !_livingWorld.FailManagedRuntime(
                runtimeId,
                "FURY Director recovery conflict"))
        {
            return false;
        }
    }

    DirectorResult result = _director.Abort(
        event,
        run->id,
        run->revision,
        outcomeKey);

    return result.Accepted();
}

bool RecoveryService::Handle(FuryEvent const& event)
{
    if (!event.id ||
        event.sourceSystem != "fury.recovery" ||
        event.correlationKey != GraphKey ||
        event.actor.kind != ActorKind::System)
    {
        return true;
    }

    if (event.type == "director.recovery.verified")
    {
        // Durable evidence that restart reconciliation saw the same runtime id.
        return true;
    }

    if (event.type == "director.recovery.attach_runtime")
    {
        if (!event.subjectId ||
            !event.actor.householdId)
        {
            return true;
        }

        std::optional<uint64> runtimeId =
            RuntimeId(event);
        if (!runtimeId || !*runtimeId)
            return true;

        std::optional<DirectorRun> run =
            _director.FindRun(*event.subjectId);
        if (!run ||
            run->graphKey != GraphKey ||
            run->householdId != *event.actor.householdId)
        {
            return true;
        }

        if (run->status == DirectorRunStatus::Complete ||
            run->status == DirectorRunStatus::Failed ||
            run->status == DirectorRunStatus::Aborted)
        {
            // The recovery intent became stale because another durable path
            // terminalized the run before this event replayed.
            return true;
        }

        DirectorResult result = _director.AttachRuntime(
            event,
            run->id,
            run->revision,
            *runtimeId);

        return result.Accepted();
    }

    if (event.type == "director.recovery.abort_missing")
    {
        return HandleRunAction(
            event,
            "recovery.runtime_missing",
            false);
    }

    if (event.type == "director.recovery.abort_conflict")
    {
        return HandleRunAction(
            event,
            "recovery.runtime_conflict",
            true);
    }

    if (event.type == "director.recovery.orphan_runtime")
    {
        if (event.subjectType != "living_world_runtime" ||
            !event.subjectId)
        {
            return true;
        }

        return _livingWorld.FailManagedRuntime(
            *event.subjectId,
            "FURY orphan managed runtime");
    }

    return true;
}
