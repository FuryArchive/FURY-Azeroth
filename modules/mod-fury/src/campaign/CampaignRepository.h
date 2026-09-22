#ifndef MOD_FURY_CAMPAIGN_REPOSITORY_H
#define MOD_FURY_CAMPAIGN_REPOSITORY_H

#include "CampaignTypes.h"

#include <optional>
#include <string_view>

namespace Fury
{
class CampaignRepository final
{
public:
    [[nodiscard]] std::optional<CampaignNodeDefinition> FindNode(
        std::string_view nodeKey) const;

    [[nodiscard]] std::optional<CampaignState> FindState(
        HouseholdId householdId,
        std::string_view nodeKey) const;

    [[nodiscard]] std::optional<PowerBand> FindHouseholdPowerBand(
        HouseholdId householdId) const;

    void InsertState(
        HouseholdId householdId,
        std::string_view nodeKey,
        CampaignStatus status,
        EventId sourceEventId) const;

    void UpdateState(
        HouseholdId householdId,
        std::string_view nodeKey,
        CampaignStatus status,
        EventId sourceEventId,
        uint64 expectedRevision) const;

    void RaiseHouseholdPowerBand(
        HouseholdId householdId,
        PowerBand powerBand) const;
};
}

#endif
