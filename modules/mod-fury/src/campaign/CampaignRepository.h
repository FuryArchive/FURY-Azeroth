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
    [[nodiscard]] std::optional<CampaignNodeDefinition> FindNode(std::string_view nodeKey) const;
    [[nodiscard]] std::optional<CampaignState> FindState(HouseholdId householdId, std::string_view nodeKey) const;
    [[nodiscard]] std::optional<PowerBand> FindHouseholdPowerBand(HouseholdId householdId) const;
    [[nodiscard]] std::optional<PowerBand> CalculateHouseholdPowerBand(HouseholdId householdId) const;

    [[nodiscard]] bool InsertState(
        HouseholdId householdId,
        std::string_view nodeKey,
        CampaignStatus status,
        EventId sourceEventId) const;

    [[nodiscard]] bool UpdateState(
        HouseholdId householdId,
        std::string_view nodeKey,
        CampaignStatus status,
        EventId sourceEventId,
        uint64 expectedRevision) const;

    [[nodiscard]] bool RecalculateHouseholdPowerBand(HouseholdId householdId) const;
};
}

#endif
