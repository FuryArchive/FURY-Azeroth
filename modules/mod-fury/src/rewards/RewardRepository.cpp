#include "RewardRepository.h"

#include "database/FuryDatabase.h"
#include "DatabaseEnv.h"

#include <string>

namespace Fury
{
namespace
{
RewardClaimView ReadClaimRow(Field* fields)
{
    RewardClaimView claim;
    claim.id = fields[0].Get<uint64>();
    claim.sourceEventId = fields[1].Get<EventId>();
    claim.rewardKey = fields[2].Get<std::string>();
    claim.beneficiaryKind =
        static_cast<BeneficiaryKind>(fields[3].Get<uint8>());
    claim.beneficiaryId = fields[4].Get<uint64>();
    claim.status =
        static_cast<RewardClaimStatus>(fields[5].Get<uint8>());
    return claim;
}

std::vector<RewardClaimView> ReadClaimRows(PreparedQueryResult const& result)
{
    std::vector<RewardClaimView> claims;
    if (!result)
        return claims;

    do
    {
        claims.push_back(ReadClaimRow(result->Fetch()));
    } while (result->NextRow());

    return claims;
}
}

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

std::optional<RewardClaimView> RewardRepository::FindClaimById(uint64 claimId) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_SEL_REWARD_CLAIM_BY_ID);
    stmt->SetData(0, claimId);

    PreparedQueryResult result = FuryDatabase.Query(stmt);
    if (!result)
        return std::nullopt;

    return ReadClaimRow(result->Fetch());
}

std::vector<RewardClaimView> RewardRepository::TailClaims(uint32 limit) const
{
    if (!limit)
        return {};

    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_SEL_REWARD_CLAIM_TAIL);
    stmt->SetData(0, limit);

    return ReadClaimRows(FuryDatabase.Query(stmt));
}

std::vector<RewardClaimView> RewardRepository::PendingClaims(uint32 limit) const
{
    if (!limit)
        return {};

    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_SEL_PENDING_REWARD_CLAIMS);
    stmt->SetData(0, RewardClaimStatus::Pending);
    stmt->SetData(1, limit);

    return ReadClaimRows(FuryDatabase.Query(stmt));
}

bool RewardRepository::ResolvePendingClaim(
    uint64 claimId,
    RewardClaimStatus terminalStatus) const
{
    if (!claimId || terminalStatus == RewardClaimStatus::Pending)
        return false;

    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_UPD_REWARD_CLAIM_STATUS);
    stmt->SetData(0, terminalStatus);
    stmt->SetData(1, terminalStatus);
    stmt->SetData(2, claimId);
    FuryDatabase.Execute(stmt);

    std::optional<RewardClaimView> claim = FindClaimById(claimId);
    return claim && claim->status == terminalStatus;
}
}
