#include "integrations/LivingWorldAdapter.h"

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
    Fury::LivingWorldRuntimeSnapshot running;
    running.runtimeId = 42; running.stageId = 1006;
    running.state = Fury::LivingWorldRuntimeState::Running;
    auto completed = running;
    completed.state = Fury::LivingWorldRuntimeState::Complete;
    if (Fury::LivingWorldAdapter::ObservationIdentity("living_world.runtime.observed", running) ==
        Fury::LivingWorldAdapter::ObservationIdentity("living_world.runtime.observed", completed))
        return 1;

    using namespace Fury;

    Require(
        LivingWorldAdapter::ToExternalState(
            LivingWorldRuntimeState::Initializing) ==
            ExternalRuntimeState::Active,
        "initializing Living World runtime is externally active");

    Require(
        LivingWorldAdapter::ToExternalState(
            LivingWorldRuntimeState::Running) ==
            ExternalRuntimeState::Active,
        "running Living World runtime is externally active");

    Require(
        LivingWorldAdapter::ToExternalState(
            LivingWorldRuntimeState::Complete) ==
            ExternalRuntimeState::Complete,
        "completed Living World runtime resolves externally");

    Require(
        LivingWorldAdapter::ToExternalState(
            LivingWorldRuntimeState::Failed) ==
            ExternalRuntimeState::Failed,
        "failed Living World runtime propagates failure");

    Require(
        LivingWorldAdapter::ToExternalState(
            LivingWorldRuntimeState::Missing) ==
            ExternalRuntimeState::Missing,
        "missing Living World runtime remains missing");

    std::cout
        << "[FURY][PASS] T23 Living World adapter policy golden gate passed\n";
    return 0;
}
