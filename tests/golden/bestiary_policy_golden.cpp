#include "bestiary/BestiaryPolicy.h"
#include "bestiary/BestiaryTypes.h"

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

    Require(CanAuthorBestiaryProgress(ActorKind::Human),
        "Human may author account Bestiary progress");
    Require(!CanAuthorBestiaryProgress(ActorKind::System),
        "System may not author account Bestiary progress");
    Require(!CanAuthorBestiaryProgress(ActorKind::HouseholdAltBot),
        "HouseholdAltBot may not author account Bestiary progress");
    Require(!CanAuthorBestiaryProgress(ActorKind::RandomPlayerBot),
        "RandomPlayerBot may not author account Bestiary progress");
    Require(!CanAuthorBestiaryProgress(ActorKind::NpcAssistant),
        "NpcAssistant may not author account Bestiary progress");

    Require(!IsValidBestiaryLevel(BestiaryDiscoveryLevel::Unknown),
        "Unknown is not a valid promotion target");
    Require(IsValidBestiaryLevel(BestiaryDiscoveryLevel::Encountered),
        "Encountered is a valid discovery level");
    Require(IsValidBestiaryLevel(BestiaryDiscoveryLevel::Studied),
        "Studied is a valid discovery level");
    Require(IsValidBestiaryLevel(BestiaryDiscoveryLevel::Mastered),
        "Mastered is a valid discovery level");

    std::cout << "[FURY][PASS] T18 Bestiary policy/level golden gate passed\n";
    return 0;
}
