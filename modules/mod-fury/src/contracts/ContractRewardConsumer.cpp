#include "ContractRewardConsumer.h"

#include "campaign/CampaignService.h"
#include "rewards/RewardService.h"

#include "Log.h"

namespace Fury
{
ContractRewardConsumer::ContractRewardConsumer(
    ContractRepository const& repository,
    CampaignService const& campaign,
    RewardService const& rewards)
    : _repository(repository),
      _campaign(campaign),
      _rewards(rewards)
{
}

bool ContractRewardConsumer::Handle(FuryEvent const& event)
{
    if (event.type != "contract.completed")
        return true;

    if (!event.id ||
        !event.actor.householdId ||
        !event.subjectId ||
        event.correlationKey.empty())
    {
        LOG_ERROR(
            "server.loading",
            "[FURY] malformed contract.completed event id={}.",
            event.id);
        return false;
    }

    HouseholdId const householdId = *event.actor.householdId;
    ContractInstanceId const instanceId = *event.subjectId;

    std::optional<ContractInstance> instance =
        _repository.FindInstance(instanceId);
    if (!instance ||
        instance->householdId != householdId ||
        instance->contractKey != event.correlationKey ||
        instance->status != ContractStatus::Complete)
    {
        LOG_ERROR(
            "server.loading",
            "[FURY] contract reward event does not match completed instance "
            "(event={}, instance={}).",
            event.id,
            instanceId);
        return false;
    }

    std::optional<ContractDefinition> definition =
        _repository.FindDefinition(instance->contractKey);
    if (!definition || !definition->enabled)
    {
        LOG_ERROR(
            "server.loading",
            "[FURY] contract definition missing/disabled during reward claim "
            "(contract={}, event={}).",
            instance->contractKey,
            event.id);
        return false;
    }

    if (!definition->rewardKey)
        return true;

    std::optional<PowerBand> powerBand =
        _campaign.CurrentPowerBand(householdId);
    if (!powerBand)
    {
        LOG_ERROR(
            "server.loading",
            "[FURY] household power band unavailable for contract reward "
            "(household={}, event={}).",
            householdId,
            event.id);
        return false;
    }

    RewardRequest request;
    request.sourceEventId = event.id;
    request.rewardKey = *definition->rewardKey;
    request.beneficiaryKind = BeneficiaryKind::Household;
    request.beneficiaryId = householdId;
    request.currentPowerBand = *powerBand;

    RewardClaimResult claim = _rewards.Claim(request);
    if (claim.Accepted())
        return true;

    LOG_ERROR(
        "server.loading",
        "[FURY] contract reward claim rejected "
        "(contract={}, reward={}, event={}, outcome={}).",
        instance->contractKey,
        *definition->rewardKey,
        event.id,
        static_cast<uint8>(claim.outcome));
    return false;
}
}
