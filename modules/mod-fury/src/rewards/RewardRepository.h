#ifndef MOD_FURY_REWARD_REPOSITORY_H
#define MOD_FURY_REWARD_REPOSITORY_H

#include "RewardTypes.h"

#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace Fury
{
class RewardRepository final
{
public:
    [[nodiscard]] std::optional<RewardPolicyBounds> FindPolicy(
        std::string_view rewardKey) const;

    [[nodiscard]] std::optional<std::pair<uint64, RewardClaimStatus>> FindClaim(
        RewardRequest const& request) const;

    void InsertClaim(RewardRequest const& request) const;

    [[nodiscard]] std::optional<RewardClaimView> FindClaimById(uint64 claimId) const;
    [[nodiscard]] std::vector<RewardClaimView> TailClaims(uint32 limit) const;
    [[nodiscard]] std::vector<RewardClaimView> PendingClaims(uint32 limit) const;

    [[nodiscard]] bool ResolvePendingClaim(
        uint64 claimId,
        RewardClaimStatus terminalStatus) const;
};
}

#endif
