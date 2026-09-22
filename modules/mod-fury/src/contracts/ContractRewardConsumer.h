#ifndef MOD_FURY_CONTRACT_REWARD_CONSUMER_H
#define MOD_FURY_CONTRACT_REWARD_CONSUMER_H

#include "ContractRepository.h"
#include "events/EventConsumer.h"

namespace Fury
{
class CampaignService;
class RewardService;

class ContractRewardConsumer final : public EventConsumer
{
public:
    ContractRewardConsumer(
        ContractRepository const& repository,
        CampaignService const& campaign,
        RewardService const& rewards);

    [[nodiscard]] std::string_view Key() const override
    {
        return "contracts.rewards.v1";
    }

    bool Handle(FuryEvent const& event) override;

private:
    ContractRepository const& _repository;
    CampaignService const& _campaign;
    RewardService const& _rewards;
};
}

#endif
