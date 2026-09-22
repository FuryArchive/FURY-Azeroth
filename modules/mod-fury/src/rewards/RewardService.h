#ifndef MOD_FURY_REWARD_SERVICE_H
#define MOD_FURY_REWARD_SERVICE_H

#include "RewardTypes.h"

#include <vector>

namespace Fury
{
class RewardPolicy;
class RewardRepository;

class RewardService final
{
public:
    RewardService(
        RewardRepository const& repository,
        RewardPolicy const& policy);

    [[nodiscard]] RewardClaimResult Claim(
        RewardRequest const& request) const;

    [[nodiscard]] std::vector<RewardClaimView> TailClaims(uint32 limit) const;
    [[nodiscard]] std::vector<RewardClaimView> PendingClaims(uint32 limit) const;

    [[nodiscard]] bool ResolvePendingClaim(
        uint64 claimId,
        RewardClaimStatus terminalStatus) const;

private:
    RewardRepository const& _repository;
    RewardPolicy const& _policy;
};
}

#endif
