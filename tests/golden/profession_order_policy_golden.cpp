#include "professions/ProfessionOrderPolicy.h"
#include "core/FuryTargetId.h"

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

    Require(CanAuthorProfessionOrderProgress(ActorKind::Human),
        "Human may author profession order progress");
    Require(!CanAuthorProfessionOrderProgress(ActorKind::System),
        "System may not author profession order progress");
    Require(!CanAuthorProfessionOrderProgress(ActorKind::HouseholdAltBot),
        "HouseholdAltBot may not author profession order progress");
    Require(!CanAuthorProfessionOrderProgress(ActorKind::RandomPlayerBot),
        "RandomPlayerBot may not author profession order progress");
    Require(!CanAuthorProfessionOrderProgress(ActorKind::NpcAssistant),
        "NpcAssistant may not author profession order progress");

    uint64 const target = MakeProfessionCraftTarget(164, 2840);
    Require(ProfessionCraftSkill(target) == 164,
        "craft target preserves profession skill");
    Require(ProfessionCraftItem(target) == 2840,
        "craft target preserves item id");

    std::cout << "[FURY][PASS] T17 Profession Order policy/target golden gate passed\n";
    return 0;
}
