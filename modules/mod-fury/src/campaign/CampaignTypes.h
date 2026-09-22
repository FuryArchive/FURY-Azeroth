#ifndef MOD_FURY_CAMPAIGN_TYPES_H
#define MOD_FURY_CAMPAIGN_TYPES_H

#include "events/FuryEvent.h"
#include "rewards/RewardTypes.h"
#include "integrations/IndividualProgressionAdapter.h"

#include <optional>
#include <string>

namespace Fury
{
enum class CampaignStatus : uint8
{
    Locked = 1,
    Available = 2,
    Active = 3,
    Complete = 4
};

struct CampaignNodeDefinition
{
    std::string nodeKey;
    uint8 era = 0;
    uint16 ordinal = 0;
    std::string displayName;
    PowerBand requiredPowerBand = PowerBand::None;
    PowerBand grantsPowerBand = PowerBand::None;
    uint8 ipRequiredState = 0;
    bool enabled = false;
};

struct CampaignState
{
    HouseholdId householdId = 0;
    std::string nodeKey;
    CampaignStatus status = CampaignStatus::Locked;
    std::optional<EventId> sourceEventId;
    uint64 revision = 0;
};

enum class CampaignTransitionOutcome : uint8
{
    Updated = 1,
    AlreadyApplied = 2,
    InvalidSource = 3,
    NodeNotFound = 4,
    NodeDisabled = 5,
    PowerBandTooLow = 6,
    InvalidTransition = 7,
    PersistenceFailed = 8
};

enum class CampaignCharacterAccessOutcome : uint8
{
    Allowed = 1,
    HouseholdLocked = 2,
    NodeNotFound = 3,
    NodeDisabled = 4,
    InvalidIpRequirement = 5,
    IpUnavailable = 6,
    IpDisabled = 7,
    IpPlayerSettingsDisabled = 8,
    PlayerUnavailable = 9,
    CharacterProgressTooLow = 10
};

struct CampaignCharacterAccessResult
{
    CampaignCharacterAccessOutcome outcome =
        CampaignCharacterAccessOutcome::HouseholdLocked;
    CampaignStatus householdStatus = CampaignStatus::Locked;
    uint8 requiredIpState = 0;
    uint8 currentIpState = 0;

    [[nodiscard]] bool Allowed() const
    {
        return outcome == CampaignCharacterAccessOutcome::Allowed;
    }
};

struct CampaignTransitionResult
{
    CampaignTransitionOutcome outcome = CampaignTransitionOutcome::PersistenceFailed;
    CampaignStatus status = CampaignStatus::Locked;

    [[nodiscard]] bool Accepted() const
    {
        return outcome == CampaignTransitionOutcome::Updated ||
            outcome == CampaignTransitionOutcome::AlreadyApplied;
    }
};
}

#endif
