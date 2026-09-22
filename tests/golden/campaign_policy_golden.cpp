#include "campaign/CampaignPolicy.h"

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

    Require(
        EvaluateCampaignTransition(CampaignStatus::Locked, CampaignStatus::Available) ==
            CampaignTransitionDisposition::Apply,
        "Locked -> Available applies");
    Require(
        EvaluateCampaignTransition(CampaignStatus::Available, CampaignStatus::Active) ==
            CampaignTransitionDisposition::Apply,
        "Available -> Active applies");
    Require(
        EvaluateCampaignTransition(CampaignStatus::Active, CampaignStatus::Complete) ==
            CampaignTransitionDisposition::Apply,
        "Active -> Complete applies");
    Require(
        EvaluateCampaignTransition(CampaignStatus::Complete, CampaignStatus::Complete) ==
            CampaignTransitionDisposition::AlreadyApplied,
        "duplicate Complete is idempotent");
    Require(
        EvaluateCampaignTransition(CampaignStatus::Complete, CampaignStatus::Available) ==
            CampaignTransitionDisposition::Reject,
        "backward transition is rejected");
    Require(
        EvaluateCampaignTransition(CampaignStatus::Locked, CampaignStatus::Complete) ==
            CampaignTransitionDisposition::Reject,
        "skipped transition is rejected");

    Require(CanAuthorCampaignTransition(ActorKind::Human),
        "Human may author household campaign transitions");
    Require(CanAuthorCampaignTransition(ActorKind::System),
        "System durable events may reconcile household campaign transitions");
    Require(!CanAuthorCampaignTransition(ActorKind::HouseholdAltBot),
        "HouseholdAltBot may not author campaign progression");
    Require(!CanAuthorCampaignTransition(ActorKind::RandomPlayerBot),
        "RandomPlayerBot may not author campaign progression");

    std::cout << "[FURY][PASS] T13 Campaign policy golden gate passed\n";
    return 0;
}
