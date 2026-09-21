#ifndef MOD_FURY_REWARD_TYPES_H
#define MOD_FURY_REWARD_TYPES_H

#include "events/FuryEvent.h"

#include <optional>
#include <string>

namespace Fury
{
enum class BeneficiaryKind : uint8
{
    Character = 1,
    Account = 2,
    Household = 3
};

enum class RewardClaimStatus : uint8
{
    Pending = 0,
    Delivered = 1,
    Failed = 2
};

enum class PowerBand : uint16
{
    None = 0,

    ClassicPreRaid = 100,
    ClassicMC = 110,
    ClassicBWL = 120,
    ClassicAQ = 130,
    ClassicNaxx = 140,

    TbcT4 = 200,
    TbcT5 = 210,
    TbcT6 = 220,
    TbcSunwell = 230,

    WrathT7 = 300,
    WrathT8 = 310,
    WrathT9 = 320,
    WrathT10 = 330,

    FuryIV1 = 400,
    FuryIV2 = 410,
    FuryIV3 = 420,
    FuryIV4 = 430
};

struct RewardPolicyBounds
{
    PowerBand minimum = PowerBand::None;
    std::optional<PowerBand> maximum;
};

struct RewardRequest
{
    EventId sourceEventId = 0;
    std::string rewardKey;

    BeneficiaryKind beneficiaryKind = BeneficiaryKind::Character;
    uint64 beneficiaryId = 0;

    PowerBand currentPowerBand = PowerBand::None;
};

enum class RewardClaimOutcome : uint8
{
    Created = 1,
    AlreadyExists = 2,
    UnknownReward = 3,
    BelowPowerBand = 4,
    AbovePowerBand = 5,
    InvalidRequest = 6,
    PersistenceFailed = 7
};

struct RewardClaimResult
{
    RewardClaimOutcome outcome = RewardClaimOutcome::InvalidRequest;
    std::optional<uint64> claimId;

    [[nodiscard]] bool Accepted() const
    {
        return outcome == RewardClaimOutcome::Created ||
            outcome == RewardClaimOutcome::AlreadyExists;
    }
};
}

#endif
