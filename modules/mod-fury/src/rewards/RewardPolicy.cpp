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

    return EvaluatePowerBand(
        request.currentPowerBand,
        bounds->minimum,
        bounds->maximum.has_value(),
        bounds->maximum.value_or(PowerBand::None));
}
}
