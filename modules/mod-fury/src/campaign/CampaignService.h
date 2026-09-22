#ifndef MOD_FURY_CAMPAIGN_SERVICE_H
#define MOD_FURY_CAMPAIGN_SERVICE_H

#include "CampaignRepository.h"

namespace Fury
{
class EventStore;

class CampaignService final
{
public:
    CampaignService(
        CampaignRepository const& repository,
        EventStore const& events);

    [[nodiscard]] CampaignStatus GetStatus(
        HouseholdId householdId,
        std::string_view nodeKey) const;

    [[nodiscard]] std::optional<PowerBand> CurrentPowerBand(
        HouseholdId householdId) const;

    [[nodiscard]] CampaignTransitionResult MarkAvailable(
        FuryEvent const& source,
        std::string_view nodeKey) const;

    [[nodiscard]] CampaignTransitionResult Activate(
        FuryEvent const& source,
        std::string_view nodeKey) const;

    [[nodiscard]] CampaignTransitionResult Complete(
        FuryEvent const& source,
        std::string_view nodeKey) const;

private:
    [[nodiscard]] CampaignTransitionResult Transition(
        FuryEvent const& source,
        std::string_view nodeKey,
        CampaignStatus target) const;

    void EmitTransitionEvent(
        FuryEvent const& source,
        CampaignNodeDefinition const& node,
        CampaignStatus target) const;

    static bool IsAuthorizedSource(FuryEvent const& source);

    CampaignRepository const& _repository;
    EventStore const& _events;
};
}

#endif
