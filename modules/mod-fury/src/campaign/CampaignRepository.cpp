#include "CampaignRepository.h"

#include "database/FuryDatabase.h"
#include "DatabaseEnv.h"

#include <string>

namespace Fury
{
std::optional<CampaignNodeDefinition> CampaignRepository::FindNode(
    std::string_view nodeKey) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_SEL_CAMPAIGN_NODE);
    stmt->SetData(0, std::string(nodeKey));

    PreparedQueryResult result = FuryDatabase.Query(stmt);
    if (!result)
        return std::nullopt;

    Field* fields = result->Fetch();

    CampaignNodeDefinition node;
    node.nodeKey = std::string(nodeKey);
    node.era = fields[0].Get<uint8>();
    node.ordinal = fields[1].Get<uint16>();
    node.displayName = fields[2].Get<std::string>();
    node.requiredPowerBand = static_cast<PowerBand>(fields[3].Get<uint16>());
    node.grantsPowerBand = static_cast<PowerBand>(fields[4].Get<uint16>());
    node.ipRequiredState = fields[5].Get<uint8>();
    node.enabled = fields[6].Get<uint8>() != 0;
    return node;
}

std::optional<CampaignState> CampaignRepository::FindState(
    HouseholdId householdId,
    std::string_view nodeKey) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_SEL_CAMPAIGN_STATE);
    stmt->SetData(0, householdId);
    stmt->SetData(1, std::string(nodeKey));

    PreparedQueryResult result = FuryDatabase.Query(stmt);
    if (!result)
        return std::nullopt;

    Field* fields = result->Fetch();

    CampaignState state;
    state.householdId = householdId;
    state.nodeKey = std::string(nodeKey);
    state.status = static_cast<CampaignStatus>(fields[0].Get<uint8>());

    if (!fields[1].IsNull())
        state.sourceEventId = fields[1].Get<EventId>();

    state.revision = fields[2].Get<uint64>();
    return state;
}

std::optional<PowerBand> CampaignRepository::FindHouseholdPowerBand(
    HouseholdId householdId) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_SEL_HOUSEHOLD_POWER_BAND);
    stmt->SetData(0, householdId);

    PreparedQueryResult result = FuryDatabase.Query(stmt);
    if (!result)
        return std::nullopt;

    return static_cast<PowerBand>(result->Fetch()[0].Get<uint16>());
}

std::optional<PowerBand> CampaignRepository::CalculateHouseholdPowerBand(
    HouseholdId householdId) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_SEL_DERIVED_CAMPAIGN_POWER_BAND);
    stmt->SetData(0, householdId);

    PreparedQueryResult result = FuryDatabase.Query(stmt);
    if (!result)
        return std::nullopt;

    return static_cast<PowerBand>(result->Fetch()[0].Get<uint16>());
}

bool CampaignRepository::InsertState(
    HouseholdId householdId,
    std::string_view nodeKey,
    CampaignStatus status,
    EventId sourceEventId) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_INS_CAMPAIGN_STATE);
    stmt->SetData(0, householdId);
    stmt->SetData(1, std::string(nodeKey));
    stmt->SetData(2, status);
    stmt->SetData(3, sourceEventId);
    FuryDatabase.Execute(stmt);

    std::optional<CampaignState> persisted = FindState(householdId, nodeKey);
    return persisted && persisted->status == status;
}

bool CampaignRepository::UpdateState(
    HouseholdId householdId,
    std::string_view nodeKey,
    CampaignStatus status,
    EventId sourceEventId,
    uint64 expectedRevision) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_UPD_CAMPAIGN_STATE);

    uint8 index = 0;
    stmt->SetData(index++, status);
    stmt->SetData(index++, status);
    stmt->SetData(index++, status);
    stmt->SetData(index++, sourceEventId);
    stmt->SetData(index++, householdId);
    stmt->SetData(index++, std::string(nodeKey));
    stmt->SetData(index++, expectedRevision);
    FuryDatabase.Execute(stmt);

    std::optional<CampaignState> persisted = FindState(householdId, nodeKey);
    return persisted &&
        persisted->status == status &&
        persisted->revision > expectedRevision;
}

bool CampaignRepository::RecalculateHouseholdPowerBand(
    HouseholdId householdId) const
{
    // Derive and update in one SQL statement so a stale caller cannot
    // overwrite a newer completion with an older precomputed power band.
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_UPD_HOUSEHOLD_POWER_BAND);
    stmt->SetData(0, householdId);
    stmt->SetData(1, householdId);
    FuryDatabase.Execute(stmt);

    std::optional<PowerBand> derived = CalculateHouseholdPowerBand(householdId);
    std::optional<PowerBand> persisted = FindHouseholdPowerBand(householdId);
    return derived && persisted && *persisted == *derived;
}
}
