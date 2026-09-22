#include "director/DirectorPolicy.h"
#include "director/DirectorReconciliation.h"

#include <cstdlib>
#include <iostream>

namespace
{
void Require(bool condition, char const* label)
{
    if (!condition)
    {
        std::cerr << "[FURY][FAIL] " << label << '\n';
        std::exit(1);
    }

    std::cout << "[FURY][PASS] " << label << '\n';
}
}

int main()
{
    using namespace Fury;

    Require(CanStartDirectorRun(ActorKind::Human),
        "Human may start Director runs");
    Require(!CanStartDirectorRun(ActorKind::System),
        "System may not start Director runs");
    Require(!CanStartDirectorRun(ActorKind::HouseholdAltBot),
        "HouseholdAltBot may not start Director runs");
    Require(!CanStartDirectorRun(ActorKind::RandomPlayerBot),
        "RandomPlayerBot may not start Director runs");
    Require(!CanStartDirectorRun(ActorKind::NpcAssistant),
        "NpcAssistant may not start Director runs");

    Require(CanMutateDirectorRun(ActorKind::Human),
        "Human may mutate Director runs");
    Require(CanMutateDirectorRun(ActorKind::System),
        "System may mutate Director runs");
    Require(!CanMutateDirectorRun(ActorKind::HouseholdAltBot),
        "HouseholdAltBot may not mutate Director runs");
    Require(!CanMutateDirectorRun(ActorKind::RandomPlayerBot),
        "RandomPlayerBot may not mutate Director runs");
    Require(!CanMutateDirectorRun(ActorKind::NpcAssistant),
        "NpcAssistant may not mutate Director runs");

    Require(
        DirectorReconciliationService::EvaluateRuntime(
            0,
            ExternalRuntimeState::Missing,
            0) == DirectorReconcileAction::None,
        "unbound run + missing runtime needs no action");

    Require(
        DirectorReconciliationService::EvaluateRuntime(
            0,
            ExternalRuntimeState::Active,
            42) == DirectorReconcileAction::AttachExternalRuntime,
        "unbound run can attach discovered active runtime");

    Require(
        DirectorReconciliationService::EvaluateRuntime(
            42,
            ExternalRuntimeState::Missing,
            0) == DirectorReconcileAction::RestartMissingRuntime,
        "bound run + missing runtime requests restart");

    Require(
        DirectorReconciliationService::EvaluateRuntime(
            42,
            ExternalRuntimeState::Active,
            42) == DirectorReconcileAction::ReattachExistingRuntime,
        "matching active runtime reattaches");

    Require(
        DirectorReconciliationService::EvaluateRuntime(
            42,
            ExternalRuntimeState::Complete,
            42) == DirectorReconcileAction::ResolveFromExternal,
        "matching completed runtime resolves");

    Require(
        DirectorReconciliationService::EvaluateRuntime(
            42,
            ExternalRuntimeState::Failed,
            42) == DirectorReconcileAction::FailFromExternal,
        "matching failed runtime propagates failure");

    Require(
        DirectorReconciliationService::EvaluateRuntime(
            42,
            ExternalRuntimeState::Active,
            99) == DirectorReconcileAction::RuntimeConflict,
        "mismatched runtime id is a conflict");

    std::cout << "[FURY][PASS] T16 Director policy/reconciliation golden gate passed\n";
    return 0;
}
