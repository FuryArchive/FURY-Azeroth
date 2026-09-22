#ifndef MOD_FURY_REWARD_POLICY_H
#define MOD_FURY_REWARD_POLICY_H

#include "RewardTypes.h"

namespace Fury
{
class RewardRepository;

class RewardPolicy final
{
public:
    explicit RewardPolicy(RewardRepository const& repository);

    [[nodiscard]] RewardClaimOutcome Evaluate(
        RewardRequest const& request) const;

private:
    RewardRepository const& _repository;
};
}

#endif
