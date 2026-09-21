#ifndef MOD_FURY_REWARD_SERVICE_H
#define MOD_FURY_REWARD_SERVICE_H

#include "RewardTypes.h"

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

private:
    RewardRepository const& _repository;
    RewardPolicy const& _policy;
};
}

#endif
