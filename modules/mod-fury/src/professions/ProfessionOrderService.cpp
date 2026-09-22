#include "ProfessionOrderService.h"
#include "ProfessionOrderPolicy.h"

#include "core/FuryKey.h"
#include "core/FuryTargetId.h"
#include "events/EventStore.h"

#include "Log.h"
#include "StringFormat.h"

namespace Fury
{
ProfessionOrderService::ProfessionOrderService(
    ProfessionOrderRepository const& repository,
    EventStore const& events)
    : _repository(repository),
      _events(events)
{
}

ProfessionOrderStartResult ProfessionOrderService::Start(
    FuryEvent const& source,
    std::string_view orderKey,
    uint16 optionOrdinal) const
{
    if (!IsAuthorizedSource(source))
        return {ProfessionOrderStartOutcome::InvalidSource, std::nullopt};

    if (!IsCanonicalKey(orderKey))
        return {ProfessionOrderStartOutcome::InvalidKey, std::nullopt};

    std::optional<ProfessionOrderDefinition> definition =
        _repository.FindDefinition(orderKey);
    if (!definition)
        return {ProfessionOrderStartOutcome::OrderNotFound, std::nullopt};

    if (!definition->enabled)
        return {ProfessionOrderStartOutcome::Disabled, std::nullopt};

    std::optional<ProfessionOrderOption> option =
        _repository.FindOption(orderKey, optionOrdinal);
    if (!option || !option->skillId || !option->itemId ||
        !option->requiredCount)
    {
        return {ProfessionOrderStartOutcome::OptionNotFound, std::nullopt};
    }

    HouseholdId const householdId = *source.actor.householdId;

    if (std::optional<ProfessionOrderInstance> active =
            _repository.FindActive(householdId, orderKey))
    {
        if (!EmitAcceptedEvent(source, *active))
        {
            return {
                ProfessionOrderStartOutcome::PersistenceFailed,
                active
            };
        }

        return {ProfessionOrderStartOutcome::AlreadyActive, active};
    }

    if (definition->repeatPolicy == ProfessionOrderRepeatPolicy::Once &&
        _repository.HasCompleted(householdId, orderKey))
    {
        return {
            ProfessionOrderStartOutcome::AlreadyCompleted,
            std::nullopt
        };
    }

    _repository.InsertActive(
        householdId,
        orderKey,
        optionOrdinal,
        source.id);

    std::optional<ProfessionOrderInstance> persisted =
        _repository.FindActive(householdId, orderKey);
    if (!persisted)
    {
        return {
            ProfessionOrderStartOutcome::PersistenceFailed,
            std::nullopt
        };
    }

    if (!EmitAcceptedEvent(source, *persisted))
    {
        return {
            ProfessionOrderStartOutcome::PersistenceFailed,
            persisted
        };
    }

    ProfessionOrderStartOutcome const outcome =
        persisted->acceptedEventId == source.id &&
        persisted->optionOrdinal == optionOrdinal
            ? ProfessionOrderStartOutcome::Started
            : ProfessionOrderStartOutcome::AlreadyActive;

    return {outcome, persisted};
}

bool ProfessionOrderService::Handle(FuryEvent const& event)
{
    if (event.type != "profession.crafted")
        return true;

    if (!IsAuthorizedSource(event) ||
        event.subjectType != "profession_craft" ||
        !event.subjectId)
    {
        return true;
    }

    uint32 const skillId =
        ProfessionCraftSkill(*event.subjectId);
    uint32 const itemId =
        ProfessionCraftItem(*event.subjectId);

    if (!skillId || !itemId)
        return true;

    HouseholdId const householdId = *event.actor.householdId;

    std::vector<ProfessionOrderInstance> matches =
        _repository.FindMatchingActive(
            householdId,
            skillId,
            itemId);

    for (ProfessionOrderInstance const& instance : matches)
    {
        if (instance.lastEventId < event.id)
        {
            _repository.Advance(
                instance.id,
                householdId,
                event.id);
        }

        std::optional<ProfessionOrderInstance> progressed =
            _repository.FindInstance(instance.id);
        if (!progressed)
        {
            LOG_ERROR(
                "server.loading",
                "[FURY] profession order progress verification failed "
                "(instance={}, event={}).",
                instance.id,
                event.id);
            return false;
        }

        if (progressed->lastEventId < event.id)
        {
            LOG_ERROR(
                "server.loading",
                "[FURY] profession order did not persist craft event "
                "(instance={}, expected_event={}, actual_event={}).",
                instance.id,
                event.id,
                progressed->lastEventId);
            return false;
        }

        // Replay must also reconcile completion. This heals a crash after
        // progress reached required_count but before profession.order.completed
        // and the terminal instance update became durable.
        if (!FinalizeIfComplete(event, *progressed))
            return false;
    }

    return true;
}

bool ProfessionOrderService::FinalizeIfComplete(
    FuryEvent const& source,
    ProfessionOrderInstance const& instance) const
{
    if (instance.progressCount < instance.requiredCount)
        return true;

    std::optional<EventId> completionEventId =
        EmitCompletedEvent(source, instance);
    if (!completionEventId)
    {
        LOG_ERROR(
            "server.loading",
            "[FURY] could not persist profession order completion "
            "(instance={}, source_event={}).",
            instance.id,
            source.id);
        return false;
    }

    _repository.MarkComplete(
        instance.id,
        instance.householdId,
        *completionEventId);

    std::optional<ProfessionOrderInstance> completed =
        _repository.FindInstance(instance.id);
    if (!completed ||
        completed->status != ProfessionOrderStatus::Complete ||
        completed->completedEventId != completionEventId)
    {
        LOG_ERROR(
            "server.loading",
            "[FURY] profession order completion verification failed "
            "(instance={}, completion_event={}).",
            instance.id,
            *completionEventId);
        return false;
    }

    return true;
}

std::optional<EventId> ProfessionOrderService::EmitAcceptedEvent(
    FuryEvent const& source,
    ProfessionOrderInstance const& instance) const
{
    FuryEvent event;
    event.type = "profession.order.accepted";
    event.actor = source.actor;
    event.mapId = source.mapId;
    event.zoneId = source.zoneId;
    event.areaId = source.areaId;
    event.subjectType = "profession_order";
    event.subjectId = instance.id;
    event.sourceSystem = "fury.professions";
    event.correlationKey = instance.orderKey;
    event.dedupeIdentity = Acore::StringFormat(
        "profession-order:accepted:v1:{}",
        instance.id);
    event.payloadJson = Acore::StringFormat(
        "{{\"instance_id\":{},\"order_key\":\"{}\","
        "\"option\":{},\"skill_id\":{},\"item_id\":{},"
        "\"required_count\":{},\"accepted_event_id\":{}}}",
        instance.id,
        instance.orderKey,
        instance.optionOrdinal,
        instance.skillId,
        instance.itemId,
        instance.requiredCount,
        instance.acceptedEventId);

    return _events.Append(event);
}

std::optional<EventId> ProfessionOrderService::EmitCompletedEvent(
    FuryEvent const& source,
    ProfessionOrderInstance const& instance) const
{
    FuryEvent event;
    event.type = "profession.order.completed";
    event.actor = source.actor;
    event.mapId = source.mapId;
    event.zoneId = source.zoneId;
    event.areaId = source.areaId;
    event.subjectType = "profession_order";
    event.subjectId = instance.id;
    event.sourceSystem = "fury.professions";
    event.correlationKey = instance.orderKey;
    event.dedupeIdentity = Acore::StringFormat(
        "profession-order:completed:v1:{}",
        instance.id);
    event.payloadJson = Acore::StringFormat(
        "{{\"instance_id\":{},\"order_key\":\"{}\","
        "\"skill_id\":{},\"item_id\":{},"
        "\"progress\":{},\"source_event_id\":{}}}",
        instance.id,
        instance.orderKey,
        instance.skillId,
        instance.itemId,
        instance.progressCount,
        instance.lastEventId);

    return _events.Append(event);
}

bool ProfessionOrderService::IsAuthorizedSource(
    FuryEvent const& source)
{
    return source.id != 0 &&
        source.actor.householdId.has_value() &&
        CanAuthorProfessionOrderProgress(source.actor.kind);
}
}

