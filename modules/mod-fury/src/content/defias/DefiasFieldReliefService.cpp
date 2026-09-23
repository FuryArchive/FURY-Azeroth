#include "DefiasFieldReliefService.h"

#include "DefiasContracts.h"
#include "DefiasFieldRelief.h"
#include "events/EventStore.h"

#include "StringFormat.h"

namespace Fury::Defias
{
FieldReliefService::FieldReliefService(EventStore const& events)
    : _events(events)
{
}

bool FieldReliefService::Handle(FuryEvent const& source)
{
    if (source.type != "profession.order.completed" ||
        source.sourceSystem != "fury.professions" ||
        source.correlationKey != FieldReliefOrderKey ||
        source.subjectType != "profession_order" ||
        !source.subjectId ||
        source.actor.kind != ActorKind::Human ||
        !source.actor.householdId)
    {
        return true;
    }

    FuryEvent event;
    event.type = "defias.field_relief.completed";
    event.actor = source.actor;
    event.mapId = source.mapId;
    event.zoneId = source.zoneId;
    event.areaId = source.areaId;
    event.subjectType = "defias_contract_signal";
    event.subjectId = FieldReliefSignalId;
    event.sourceSystem = "fury.defias";
    event.correlationKey = FieldReliefContract;
    event.dedupeIdentity = Acore::StringFormat(
        "defias:field-relief:completed:v1:{}",
        *source.subjectId);
    event.payloadJson = Acore::StringFormat(
        "{{\"profession_order_instance_id\":{},"
        "\"profession_completion_event_id\":{}}}",
        *source.subjectId,
        source.id);

    return _events.Append(event).has_value();
}
}
