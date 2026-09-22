#include "DirectorReconciliation.h"

namespace Fury
{
std::vector<DirectorReconcileDecision>
DirectorReconciliationService::BuildPlan(
    DirectorRuntimeProbe const& probe) const
{
    std::vector<DirectorReconcileDecision> decisions;

    for (DirectorRun const& run : _repository.LoadActiveRuns())
    {
        ExternalRuntimeSnapshot external = probe.Inspect(run);

        decisions.push_back({
            run,
            external,
            Evaluate(run, external)
        });
    }

    return decisions;
}

DirectorReconcileAction DirectorReconciliationService::Evaluate(
    DirectorRun const& run,
    ExternalRuntimeSnapshot const& external)
{
    if (external.state == ExternalRuntimeState::NotManaged)
        return DirectorReconcileAction::None;

    if (!run.externalRuntimeId)
    {
        if (external.state == ExternalRuntimeState::Active &&
            external.runtimeId)
        {
            return DirectorReconcileAction::AttachExternalRuntime;
        }

        if (external.state == ExternalRuntimeState::Missing)
            return DirectorReconcileAction::None;

        return DirectorReconcileAction::RuntimeConflict;
    }

    if (external.state == ExternalRuntimeState::Missing)
        return DirectorReconcileAction::RestartMissingRuntime;

    if (!external.runtimeId ||
        *external.runtimeId != *run.externalRuntimeId)
    {
        return DirectorReconcileAction::RuntimeConflict;
    }

    switch (external.state)
    {
        case ExternalRuntimeState::Active:
            return DirectorReconcileAction::ReattachExistingRuntime;
        case ExternalRuntimeState::Complete:
            return DirectorReconcileAction::ResolveFromExternal;
        case ExternalRuntimeState::Failed:
            return DirectorReconcileAction::FailFromExternal;
        case ExternalRuntimeState::NotManaged:
        case ExternalRuntimeState::Missing:
            break;
    }

    return DirectorReconcileAction::RuntimeConflict;
}

namespace
{
constexpr DirectorRun RunWithoutRuntime()
{
    return {};
}

constexpr DirectorRun RunWithRuntime()
{
    DirectorRun run;
    run.externalRuntimeId = 42;
    return run;
}

constexpr ExternalRuntimeSnapshot External(
    ExternalRuntimeState state,
    std::optional<uint64> runtimeId = std::nullopt)
{
    return {state, runtimeId};
}
}

static_assert(
    DirectorReconciliationService::Evaluate(
        RunWithoutRuntime(),
        External(ExternalRuntimeState::Missing)) ==
    DirectorReconcileAction::None);

static_assert(
    DirectorReconciliationService::Evaluate(
        RunWithoutRuntime(),
        External(ExternalRuntimeState::Active, 42)) ==
    DirectorReconcileAction::AttachExternalRuntime);

static_assert(
    DirectorReconciliationService::Evaluate(
        RunWithRuntime(),
        External(ExternalRuntimeState::Missing)) ==
    DirectorReconcileAction::RestartMissingRuntime);

static_assert(
    DirectorReconciliationService::Evaluate(
        RunWithRuntime(),
        External(ExternalRuntimeState::Active, 42)) ==
    DirectorReconcileAction::ReattachExistingRuntime);

static_assert(
    DirectorReconciliationService::Evaluate(
        RunWithRuntime(),
        External(ExternalRuntimeState::Complete, 42)) ==
    DirectorReconcileAction::ResolveFromExternal);

static_assert(
    DirectorReconciliationService::Evaluate(
        RunWithRuntime(),
        External(ExternalRuntimeState::Active, 99)) ==
    DirectorReconcileAction::RuntimeConflict);
}
