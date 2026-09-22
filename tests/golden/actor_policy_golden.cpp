#include "ActorPolicy.h"

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

    Require(ClassifyActor(false, false, false) == ActorKind::System,
        "GS actor: null/system event resolves to System");
    Require(ClassifyActor(true, true, false) == ActorKind::Human,
        "GS02: real player resolves to Human");
    Require(ClassifyActor(true, true, true) == ActorKind::Human,
        "GS02: real player remains Human even when household-owned");
    Require(ClassifyActor(true, false, true) == ActorKind::HouseholdAltBot,
        "GS04: non-real household account resolves to HouseholdAltBot");
    Require(ClassifyActor(true, false, false) == ActorKind::RandomPlayerBot,
        "GS03: non-real non-household player resolves to RandomPlayerBot");

    Require(IsPersistentProgressionAuthority(ActorKind::Human),
        "Human may author persistent progression");
    Require(!IsPersistentProgressionAuthority(ActorKind::HouseholdAltBot),
        "HouseholdAltBot may not author persistent progression");
    Require(!IsPersistentProgressionAuthority(ActorKind::RandomPlayerBot),
        "RandomPlayerBot may not author persistent progression");

    Require(ShouldPersistGeneralEvent(ActorKind::Human),
        "Human general events are durable");
    Require(ShouldPersistGeneralEvent(ActorKind::HouseholdAltBot),
        "HouseholdAltBot general events are durable");
    Require(!ShouldPersistGeneralEvent(ActorKind::RandomPlayerBot),
        "RandomPlayerBot general events are filtered");

    std::cout << "[FURY][PASS] M1 actor-policy golden gate passed\n";
    return 0;
}
