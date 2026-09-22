#include "CampaignService.h"
#include "CampaignPolicy.h"

#include "core/FuryKey.h"
#include "integrations/IndividualProgressionAdapter.h"

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
    EventStore const& events,
    IndividualProgressionAdapter const& individualProgression)
    : _repository(repository),
      _events(events),
      _individualProgression(individualProgression)
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

std::optional<PowerBand> CampaignService::CurrentPowerBand(
    HouseholdId householdId) const
{
    return _repository.FindHouseholdPowerBand(householdId);
}

CampaignCharacterAccessResult CampaignService::CheckCharacterAccess(
    Player* player,
    HouseholdId householdId,
    std::string_view nodeKey) const
{
    if (!IsCanonicalKey(nodeKey))
    {
        return {
            CampaignCharacterAccessOutcome::NodeNotFound,
            CampaignStatus::Locked,
            0,
            0
        };
    }

    std::optional<CampaignNodeDefinition> node =
        _repository.FindNode(nodeKey);
    if (!node)
    {
        return {
            CampaignCharacterAccessOutcome::NodeNotFound,
            CampaignStatus::Locked,
            0,
            0
        };
    }

    if (!node->enabled)
    {
        return {
            CampaignCharacterAccessOutcome::NodeDisabled,
            CampaignStatus::Locked,
            node->ipRequiredState,
            0
        };
    }

    CampaignStatus const householdStatus =
        GetStatus(householdId, nodeKey);

    if (householdStatus == CampaignStatus::Locked)
    {
        return {
            CampaignCharacterAccessOutcome::HouseholdLocked,
            householdStatus,
            node->ipRequiredState,
            0
        };
    }

    IndividualProgressionGateResult const gate =
        _individualProgression.Check(
            player,
            node->ipRequiredState);

    CampaignCharacterAccessOutcome outcome =
        CampaignCharacterAccessOutcome::Allowed;

    switch (gate.outcome)
    {
        case IndividualProgressionGateOutcome::Allowed:
        case IndividualProgressionGateOutcome::NotRequired:
            outcome = CampaignCharacterAccessOutcome::Allowed;
            break;
        case IndividualProgressionGateOutcome::InvalidRequiredState:
            outcome = CampaignCharacterAccessOutcome::InvalidIpRequirement;
            break;
        case IndividualProgressionGateOutcome::ModuleUnavailable:
            outcome = CampaignCharacterAccessOutcome::IpUnavailable;
            break;
        case IndividualProgressionGateOutcome::ModuleDisabled:
            outcome = CampaignCharacterAccessOutcome::IpDisabled;
            break;
        case IndividualProgressionGateOutcome::PlayerUnavailable:
            outcome = CampaignCharacterAccessOutcome::PlayerUnavailable;
            break;
        case IndividualProgressionGateOutcome::NotPassed:
            outcome = CampaignCharacterAccessOutcome::CharacterProgressTooLow;
            break;
    }

    return {
        outcome,
        householdStatus,
        gate.requiredState,
        gate.currentState
    };
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

    std::optional<CampaignNodeDefinition> node =
        _repository.FindNode(nodeKey);
    if (!node)
        return {CampaignTransitionOutcome::NodeNotFound, CampaignStatus::Locked};

    if (!node->enabled)
        return {CampaignTransitionOutcome::NodeDisabled, CampaignStatus::Locked};

    std::optional<PowerBand> currentBand =
        _repository.FindHouseholdPowerBand(householdId);
    if (!currentBand)
        return {CampaignTransitionOutcome::PersistenceFailed, CampaignStatus::Locked};

    if (static_cast<uint16>(*currentBand) <
        static_cast<uint16>(node->requiredPowerBand))
    {
        return {CampaignTransitionOutcome::PowerBandTooLow, GetStatus(householdId, nodeKey)};
    }

    std::optional<CampaignState> state =
        _repository.FindState(householdId, nodeKey);
    CampaignStatus const current =
        state ? state->status : CampaignStatus::Locked;

    CampaignTransitionDisposition const disposition =
        EvaluateCampaignTransition(current, target);

    if (disposition == CampaignTransitionDisposition::AlreadyApplied)
    {
        // Heal derived state after a crash between the canonical transition
        // and its follow-up side effects.
        if (target == CampaignStatus::Complete)
        {
            _repository.RecalculateHouseholdPowerBand(householdId);
        }

        EmitTransitionEvent(source, *node, target);
        return {CampaignTransitionOutcome::AlreadyApplied, current};
    }

    if (disposition == CampaignTransitionDisposition::Reject)
        return {CampaignTransitionOutcome::InvalidTransition, current};

    if (!state)
    {
        _repository.InsertState(
            householdId,
            nodeKey,
            target,
            source.id);
    }
    else
    {
        _repository.UpdateState(
            householdId,
            nodeKey,
            target,
            source.id,
            state->revision);
    }

    std::optional<CampaignState> persisted =
        _repository.FindState(householdId, nodeKey);
    if (!persisted || persisted->status != target)
        return {CampaignTransitionOutcome::PersistenceFailed, current};

    if (target == CampaignStatus::Complete)
    {
        _repository.RecalculateHouseholdPowerBand(householdId);
    }

    EmitTransitionEvent(source, *node, target);
    return {CampaignTransitionOutcome::Updated, target};
}

void CampaignService::EmitTransitionEvent(
    FuryEvent const& source,
    CampaignNodeDefinition const& node,
    CampaignStatus target) const
{
    if (!source.actor.householdId)
        return;

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

    _events.Append(event);
}

bool CampaignService::IsAuthorizedSource(FuryEvent const& source)
{
    return source.id != 0 &&
        source.actor.householdId.has_value() &&
        source.actor.isEligibleForPersistentProgression;
}
}
