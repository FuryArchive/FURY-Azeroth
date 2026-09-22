#include "contracts/ContractPolicy.h"

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

    Require(CanAuthorContractProgress(ActorKind::Human),
        "Human may accept/progress persistent contracts");
    Require(!CanAuthorContractProgress(ActorKind::System),
        "System may not author persistent contract progress");
    Require(!CanAuthorContractProgress(ActorKind::HouseholdAltBot),
        "HouseholdAltBot may not author persistent contract progress");
    Require(!CanAuthorContractProgress(ActorKind::RandomPlayerBot),
        "RandomPlayerBot may not author persistent contract progress");
    Require(!CanAuthorContractProgress(ActorKind::NpcAssistant),
        "NpcAssistant may not author persistent contract progress");

    std::cout << "[FURY][PASS] T15 Contract authority policy golden gate passed\n";
    return 0;
}
