#include "CampaignService.h"
#include "CampaignPolicy.h"

#include "core/FuryKey.h"
#include "events/EventStore.h"
#include "StringFormat.h"

namespace Fury
{
namespace
{
char const* TransitionEventType(CampaignStatus status)
{
    switch (status)
    {
        case CampaignStatus::Available:
            return "campaign.node.available";
        case CampaignStatus::Active:
            return "campaign.node.activated";
        case CampaignStatus::Complete:
            return "campaign.node.completed";
        case CampaignStatus::Locked:
            break;
    }

    return "campaign.node.changed";
}

char const* TransitionKey(CampaignStatus status)
{
    switch (status)
    {
        case CampaignStatus::Available:
            return "available";
        case CampaignStatus::Active:
            return "active";
        case CampaignStatus::Complete:
            return "complete";
        case CampaignStatus::Locked:
            return "locked";
    }

    return "unknown";
}
}

CampaignService::CampaignService(
    CampaignRepository const& repository,
    EventStore const& events)
    : _repository(repository),
      _events(events)
{
}

CampaignStatus CampaignService::GetStatus(
    HouseholdId householdId,
    std::string_view nodeKey) const
{
    std::optional<CampaignState> state =
        _repository.FindState(householdId, nodeKey);

    return state ? state->status : CampaignStatus::Locked;
}

std::optional<PowerBand> CampaignService::GetHouseholdPowerBand(
    HouseholdId householdId) const
{
    return _repository.CalculateHouseholdPowerBand(householdId);
}

CampaignTransitionResult CampaignService::MarkAvailable(
    FuryEvent const& source,
    std::string_view nodeKey) const
{
    return Transition(source, nodeKey, CampaignStatus::Available);
}

CampaignTransitionResult CampaignService::Activate(
    FuryEvent const& source,
    std::string_view nodeKey) const
{
    return Transition(source, nodeKey, CampaignStatus::Active);
}

CampaignTransitionResult CampaignService::Complete(
    FuryEvent const& source,
    std::string_view nodeKey) const
{
    return Transition(source, nodeKey, CampaignStatus::Complete);
}

CampaignTransitionResult CampaignService::Transition(
    FuryEvent const& source,
    std::string_view nodeKey,
    CampaignStatus target) const
{
    if (!IsAuthorizedSource(source))
        return {CampaignTransitionOutcome::InvalidSource, CampaignStatus::Locked};

    if (!IsCanonicalKey(nodeKey))
        return {CampaignTransitionOutcome::NodeNotFound, CampaignStatus::Locked};

    HouseholdId const householdId = *source.actor.householdId;

    std::optional<CampaignNodeDefinition> node = _repository.FindNode(nodeKey);
    if (!node)
        return {CampaignTransitionOutcome::NodeNotFound, CampaignStatus::Locked};

    if (!node->enabled)
        return {CampaignTransitionOutcome::NodeDisabled, CampaignStatus::Locked};

    // Canonical campaign completion state is the authority. The household
    // column is only a derived cache and must never authorize progression.
    std::optional<PowerBand> currentBand =
        _repository.CalculateHouseholdPowerBand(householdId);
    if (!currentBand)
        return {CampaignTransitionOutcome::PersistenceFailed, CampaignStatus::Locked};

    if (static_cast<uint16>(*currentBand) <
        static_cast<uint16>(node->requiredPowerBand))
    {
        return {
            CampaignTransitionOutcome::PowerBandTooLow,
            GetStatus(householdId, nodeKey)
        };
    }

    std::optional<CampaignState> state =
        _repository.FindState(householdId, nodeKey);
    CampaignStatus const current =
        state ? state->status : CampaignStatus::Locked;

    CampaignTransitionDisposition const disposition =
        EvaluateCampaignTransition(current, target);

    if (disposition == CampaignTransitionDisposition::AlreadyApplied)
    {
        if (target == CampaignStatus::Complete &&
            !_repository.RecalculateHouseholdPowerBand(householdId))
        {
            return {CampaignTransitionOutcome::PersistenceFailed, current};
        }

        if (!EmitTransitionEvent(source, *node, target))
            return {CampaignTransitionOutcome::PersistenceFailed, current};

        return {CampaignTransitionOutcome::AlreadyApplied, current};
    }

    if (disposition == CampaignTransitionDisposition::Reject)
        return {CampaignTransitionOutcome::InvalidTransition, current};

    bool persistedMutation = false;
    if (!state)
    {
        persistedMutation = _repository.InsertState(
            householdId,
            nodeKey,
            target,
            source.id);
    }
    else
    {
        persistedMutation = _repository.UpdateState(
            householdId,
            nodeKey,
            target,
            source.id,
            state->revision);
    }

    if (!persistedMutation)
    {
        std::optional<CampaignState> concurrent =
            _repository.FindState(householdId, nodeKey);
        if (!concurrent || concurrent->status != target)
            return {CampaignTransitionOutcome::PersistenceFailed, current};

        if (target == CampaignStatus::Complete &&
            !_repository.RecalculateHouseholdPowerBand(householdId))
        {
            return {CampaignTransitionOutcome::PersistenceFailed, target};
        }

        if (!EmitTransitionEvent(source, *node, target))
            return {CampaignTransitionOutcome::PersistenceFailed, target};

        return {CampaignTransitionOutcome::AlreadyApplied, target};
    }

    if (target == CampaignStatus::Complete &&
        !_repository.RecalculateHouseholdPowerBand(householdId))
    {
        return {CampaignTransitionOutcome::PersistenceFailed, target};
    }

    if (!EmitTransitionEvent(source, *node, target))
        return {CampaignTransitionOutcome::PersistenceFailed, target};

    return {CampaignTransitionOutcome::Updated, target};
}

bool CampaignService::EmitTransitionEvent(
    FuryEvent const& source,
    CampaignNodeDefinition const& node,
    CampaignStatus target) const
{
    if (!source.actor.householdId)
        return false;

    HouseholdId const householdId = *source.actor.householdId;

    FuryEvent event;
    event.type = TransitionEventType(target);
    event.actor = source.actor;
    event.mapId = source.mapId;
    event.zoneId = source.zoneId;
    event.areaId = source.areaId;
    event.subjectType = "campaign_node";
    event.sourceSystem = "fury.campaign";
    event.correlationKey = node.nodeKey;
    event.dedupeIdentity = Acore::StringFormat(
        "campaign:{}:v1:{}:{}",
        TransitionKey(target),
        householdId,
        node.nodeKey);
    event.payloadJson = Acore::StringFormat(
        "{{\"node_key\":\"{}\",\"source_event_id\":{},\"status\":{}}}",
        node.nodeKey,
        source.id,
        static_cast<uint8>(target));

    return _events.Append(event).has_value();
}

bool CampaignService::IsAuthorizedSource(FuryEvent const& source)
{
    return source.id != 0 &&
        source.actor.householdId.has_value() &&
        CanAuthorCampaignTransition(source.actor.kind);
}
}
