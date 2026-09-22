#include "campaign/CampaignAccessPolicy.h"
#include "integrations/IndividualProgressionAdapter.h"

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

    Require(IndividualProgressionAdapter::IsKnownState(0),
        "IP state 0 is known");
    Require(!IndividualProgressionAdapter::IsKnownState(11),
        "IP gap state 11 is rejected");
    Require(IndividualProgressionAdapter::Passes(12, 10),
        "higher character progression passes lower requirement");
    Require(!IndividualProgressionAdapter::Passes(3, 4),
        "lower character progression fails higher requirement");

    Require(
        EvaluateCampaignCharacterAccess(
            CampaignStatus::Locked,
            IndividualProgressionGateOutcome::Allowed) ==
        CampaignCharacterAccessOutcome::HouseholdLocked,
        "household lock wins before character gate");

    Require(
        EvaluateCampaignCharacterAccess(
            CampaignStatus::Available,
            IndividualProgressionGateOutcome::NotRequired) ==
        CampaignCharacterAccessOutcome::Allowed,
        "unlocked node with no IP requirement is allowed");

    Require(
        EvaluateCampaignCharacterAccess(
            CampaignStatus::Available,
            IndividualProgressionGateOutcome::ModuleUnavailable) ==
        CampaignCharacterAccessOutcome::IpUnavailable,
        "required IP fails closed when module is unavailable");

    Require(
        EvaluateCampaignCharacterAccess(
            CampaignStatus::Available,
            IndividualProgressionGateOutcome::ModuleIncompatible) ==
        CampaignCharacterAccessOutcome::IpIncompatible,
        "required IP fails closed when module contract is incompatible");

    Require(
        EvaluateCampaignCharacterAccess(
            CampaignStatus::Active,
            IndividualProgressionGateOutcome::NotPassed) ==
        CampaignCharacterAccessOutcome::CharacterProgressTooLow,
        "household unlock does not bypass character gate");

    std::cout << "[FURY][PASS] T19 IP/Campaign access policy golden gate passed\n";
    return 0;
}
