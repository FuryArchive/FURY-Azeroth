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
        decisions.push_back({run, external, Evaluate(run, external)});
    }

    return decisions;
}

DirectorReconcileAction DirectorReconciliationService::Evaluate(
    DirectorRun const& run,
    ExternalRuntimeSnapshot const& external)
{
    return EvaluateRuntime(
        run.externalRuntimeId.value_or(0),
        external.state,
        external.runtimeId.value_or(0));
}
}

