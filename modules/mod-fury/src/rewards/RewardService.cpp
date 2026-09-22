#include "RewardService.h"

#include "RewardPolicy.h"
#include "RewardRepository.h"

namespace Fury
{
RewardService::RewardService(
    RewardRepository const& repository,
    RewardPolicy const& policy)
    : _repository(repository),
      _policy(policy)
{
}

RewardClaimResult RewardService::Claim(
    RewardRequest const& request) const
{
    if (auto existing = _repository.FindClaim(request))
    {
        return {
            RewardClaimOutcome::AlreadyExists,
            existing->first
        };
    }

    RewardClaimOutcome const policyResult = _policy.Evaluate(request);
    if (policyResult != RewardClaimOutcome::Created)
        return {policyResult, std::nullopt};

    _repository.InsertClaim(request);

    if (auto persisted = _repository.FindClaim(request))
    {
        return {
            RewardClaimOutcome::Created,
            persisted->first
        };
    }

    return {
        RewardClaimOutcome::PersistenceFailed,
        std::nullopt
    };
}

std::vector<RewardClaimView> RewardService::TailClaims(uint32 limit) const
{
    return _repository.TailClaims(limit);
}
}
