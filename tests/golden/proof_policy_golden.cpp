#include "proofs/ProofPolicy.h"

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

    Require(CanGrantHouseholdProof(ActorKind::Human),
        "Human may grant persistent household proofs");
    Require(!CanGrantHouseholdProof(ActorKind::System),
        "System may not grant persistent household proofs");
    Require(!CanGrantHouseholdProof(ActorKind::HouseholdAltBot),
        "HouseholdAltBot may not grant persistent household proofs");
    Require(!CanGrantHouseholdProof(ActorKind::RandomPlayerBot),
        "RandomPlayerBot may not grant persistent household proofs");

    std::cout << "[FURY][PASS] T14 Proof authority policy golden gate passed\n";
    return 0;
}
