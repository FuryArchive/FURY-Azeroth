#include "RewardPolicy.h"

#include "RewardRepository.h"

namespace Fury
{
RewardPolicy::RewardPolicy(RewardRepository const& repository)
    : _repository(repository)
{
}

RewardClaimOutcome RewardPolicy::Evaluate(
    RewardRequest const& request) const
{
    if (!request.sourceEventId ||
        request.rewardKey.empty() ||
        !request.beneficiaryId)
    {
        return RewardClaimOutcome::InvalidRequest;
    }

    std::optional<RewardPolicyBounds> bounds =
        _repository.FindPolicy(request.rewardKey);
    if (!bounds)
        return RewardClaimOutcome::UnknownReward;

    uint16 const current = static_cast<uint16>(request.currentPowerBand);
    uint16 const minimum = static_cast<uint16>(bounds->minimum);

    if (current < minimum)
        return RewardClaimOutcome::BelowPowerBand;

    if (bounds->maximum &&
        current > static_cast<uint16>(*bounds->maximum))
    {
        return RewardClaimOutcome::AbovePowerBand;
    }

    return RewardClaimOutcome::Created;
}
}
