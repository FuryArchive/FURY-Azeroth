#include "RewardRepository.h"

#include "database/FuryDatabase.h"
#include "DatabaseEnv.h"

#include <string>

namespace Fury
{
std::optional<RewardPolicyBounds> RewardRepository::FindPolicy(
    std::string_view rewardKey) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_SEL_REWARD_POLICY);
    stmt->SetData(0, std::string(rewardKey));

    PreparedQueryResult result = FuryDatabase.Query(stmt);
    if (!result)
        return std::nullopt;

    Field* fields = result->Fetch();

    RewardPolicyBounds bounds;
    bounds.minimum = static_cast<PowerBand>(fields[0].Get<uint16>());

    if (!fields[1].IsNull())
        bounds.maximum = static_cast<PowerBand>(fields[1].Get<uint16>());

    return bounds;
}

std::optional<std::pair<uint64, RewardClaimStatus>> RewardRepository::FindClaim(
    RewardRequest const& request) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_SEL_REWARD_CLAIM);
    stmt->SetData(0, request.sourceEventId);
    stmt->SetData(1, request.rewardKey);
    stmt->SetData(2, request.beneficiaryKind);
    stmt->SetData(3, request.beneficiaryId);

    PreparedQueryResult result = FuryDatabase.Query(stmt);
    if (!result)
        return std::nullopt;

    Field* fields = result->Fetch();
    return std::make_pair(
        fields[0].Get<uint64>(),
        static_cast<RewardClaimStatus>(fields[1].Get<uint8>()));
}

void RewardRepository::InsertClaim(RewardRequest const& request) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_INS_REWARD_CLAIM);
    stmt->SetData(0, request.sourceEventId);
    stmt->SetData(1, request.rewardKey);
    stmt->SetData(2, request.beneficiaryKind);
    stmt->SetData(3, request.beneficiaryId);
    stmt->SetData(4, RewardClaimStatus::Pending);
    FuryDatabase.Execute(stmt);
}
}
