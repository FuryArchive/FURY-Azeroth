#ifndef MOD_FURY_CAMPAIGN_ACCESS_POLICY_H
#define MOD_FURY_CAMPAIGN_ACCESS_POLICY_H

#include "CampaignTypes.h"

namespace Fury
{
[[nodiscard]] constexpr CampaignCharacterAccessOutcome
EvaluateCampaignCharacterAccess(
    CampaignStatus householdStatus,
    IndividualProgressionGateOutcome gate)
{
    if (householdStatus == CampaignStatus::Locked)
        return CampaignCharacterAccessOutcome::HouseholdLocked;

    switch (gate)
    {
        case IndividualProgressionGateOutcome::Allowed:
        case IndividualProgressionGateOutcome::NotRequired:
            return CampaignCharacterAccessOutcome::Allowed;
        case IndividualProgressionGateOutcome::ModuleUnavailable:
            return CampaignCharacterAccessOutcome::IpUnavailable;
        case IndividualProgressionGateOutcome::ModuleDisabled:
            return CampaignCharacterAccessOutcome::IpDisabled;
        case IndividualProgressionGateOutcome::PlayerSettingsDisabled:
            return CampaignCharacterAccessOutcome::IpPlayerSettingsDisabled;
        case IndividualProgressionGateOutcome::InvalidRequiredState:
            return CampaignCharacterAccessOutcome::InvalidIpRequirement;
        case IndividualProgressionGateOutcome::PlayerUnavailable:
            return CampaignCharacterAccessOutcome::PlayerUnavailable;
        case IndividualProgressionGateOutcome::NotPassed:
            return CampaignCharacterAccessOutcome::CharacterProgressTooLow;
    }

    return CampaignCharacterAccessOutcome::CharacterProgressTooLow;
}

static_assert(
    EvaluateCampaignCharacterAccess(
        CampaignStatus::Locked,
        IndividualProgressionGateOutcome::Allowed) ==
    CampaignCharacterAccessOutcome::HouseholdLocked);

static_assert(
    EvaluateCampaignCharacterAccess(
        CampaignStatus::Available,
        IndividualProgressionGateOutcome::NotPassed) ==
    CampaignCharacterAccessOutcome::CharacterProgressTooLow);

static_assert(
    EvaluateCampaignCharacterAccess(
        CampaignStatus::Active,
        IndividualProgressionGateOutcome::Allowed) ==
    CampaignCharacterAccessOutcome::Allowed);

static_assert(
    EvaluateCampaignCharacterAccess(
        CampaignStatus::Complete,
        IndividualProgressionGateOutcome::NotRequired) ==
    CampaignCharacterAccessOutcome::Allowed);

static_assert(
    EvaluateCampaignCharacterAccess(
        CampaignStatus::Available,
        IndividualProgressionGateOutcome::ModuleUnavailable) ==
    CampaignCharacterAccessOutcome::IpUnavailable);
}

#endif
