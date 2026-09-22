#include "ProfessionOrderRepository.h"

#include "database/FuryDatabase.h"
#include "DatabaseEnv.h"

#include <string>
#include <utility>

namespace Fury
{
namespace
{
ProfessionOrderInstance ReadInstance(Field* fields)
{
    ProfessionOrderInstance instance;
    instance.id = fields[0].Get<ProfessionOrderInstanceId>();
    instance.householdId = fields[1].Get<HouseholdId>();
    instance.orderKey = fields[2].Get<std::string>();
    instance.optionOrdinal = fields[3].Get<uint16>();
    instance.status =
        static_cast<ProfessionOrderStatus>(fields[4].Get<uint8>());
    instance.progressCount = fields[5].Get<uint32>();
    instance.acceptedEventId = fields[6].Get<EventId>();
    instance.lastEventId = fields[7].Get<EventId>();

    if (!fields[8].IsNull())
        instance.completedEventId = fields[8].Get<EventId>();

    instance.revision = fields[9].Get<uint64>();
    instance.skillId = fields[10].Get<uint32>();
    instance.itemId = fields[11].Get<uint32>();
    instance.requiredCount = fields[12].Get<uint32>();
    return instance;
}
}

std::optional<ProfessionOrderDefinition>
ProfessionOrderRepository::FindDefinition(
    std::string_view orderKey) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_SEL_PROFESSION_ORDER);
    stmt->SetData(0, std::string(orderKey));

    PreparedQueryResult result = FuryDatabase.Query(stmt);
    if (!result)
        return std::nullopt;

    Field* fields = result->Fetch();

    ProfessionOrderDefinition definition;
    definition.orderKey = std::string(orderKey);
    definition.title = fields[0].Get<std::string>();
    definition.repeatPolicy =
        static_cast<ProfessionOrderRepeatPolicy>(fields[1].Get<uint8>());
    definition.enabled = fields[2].Get<uint8>() != 0;
    return definition;
}

std::optional<ProfessionOrderOption>
ProfessionOrderRepository::FindOption(
    std::string_view orderKey,
    uint16 optionOrdinal) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_SEL_PROFESSION_ORDER_OPTION);
    stmt->SetData(0, std::string(orderKey));
    stmt->SetData(1, optionOrdinal);

    PreparedQueryResult result = FuryDatabase.Query(stmt);
    if (!result)
        return std::nullopt;

    Field* fields = result->Fetch();

    ProfessionOrderOption option;
    option.orderKey = std::string(orderKey);
    option.ordinal = optionOrdinal;
    option.skillId = fields[0].Get<uint32>();
    option.itemId = fields[1].Get<uint32>();
    option.requiredCount = fields[2].Get<uint32>();
    return option;
}

std::optional<ProfessionOrderInstance>
ProfessionOrderRepository::FindActive(
    HouseholdId householdId,
    std::string_view orderKey) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_SEL_PROFESSION_ORDER_ACTIVE);
    stmt->SetData(0, householdId);
    stmt->SetData(1, std::string(orderKey));

    PreparedQueryResult result = FuryDatabase.Query(stmt);
    if (!result)
        return std::nullopt;

    return ReadInstance(result->Fetch());
}

std::optional<ProfessionOrderInstance>
ProfessionOrderRepository::FindInstance(
    ProfessionOrderInstanceId instanceId) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_SEL_PROFESSION_ORDER_INSTANCE);
    stmt->SetData(0, instanceId);

    PreparedQueryResult result = FuryDatabase.Query(stmt);
    if (!result)
        return std::nullopt;

    return ReadInstance(result->Fetch());
}

bool ProfessionOrderRepository::HasCompleted(
    HouseholdId householdId,
    std::string_view orderKey) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_SEL_PROFESSION_ORDER_COMPLETED);
    stmt->SetData(0, householdId);
    stmt->SetData(1, std::string(orderKey));
    return static_cast<bool>(FuryDatabase.Query(stmt));
}

void ProfessionOrderRepository::InsertActive(
    HouseholdId householdId,
    std::string_view orderKey,
    uint16 optionOrdinal,
    EventId acceptedEventId) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_INS_PROFESSION_ORDER_INSTANCE);
    stmt->SetData(0, householdId);
    stmt->SetData(1, std::string(orderKey));
    stmt->SetData(2, optionOrdinal);
    stmt->SetData(3, acceptedEventId);
    stmt->SetData(4, acceptedEventId);
    FuryDatabase.Execute(stmt);
}

std::vector<ProfessionOrderInstance>
ProfessionOrderRepository::FindMatchingActive(
    HouseholdId householdId,
    uint32 skillId,
    uint32 itemId) const
{
    std::vector<ProfessionOrderInstance> instances;

    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_SEL_PROFESSION_ORDER_MATCHES);
    stmt->SetData(0, householdId);
    stmt->SetData(1, skillId);
    stmt->SetData(2, itemId);

    PreparedQueryResult result = FuryDatabase.Query(stmt);
    if (!result)
        return instances;

    do
    {
        instances.push_back(ReadInstance(result->Fetch()));
    } while (result->NextRow());

    return instances;
}

void ProfessionOrderRepository::Advance(
    ProfessionOrderInstanceId instanceId,
    HouseholdId householdId,
    EventId eventId) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_UPD_PROFESSION_ORDER_PROGRESS);
    stmt->SetData(0, eventId);
    stmt->SetData(1, instanceId);
    stmt->SetData(2, householdId);
    stmt->SetData(3, eventId);
    FuryDatabase.Execute(stmt);
}

void ProfessionOrderRepository::MarkComplete(
    ProfessionOrderInstanceId instanceId,
    HouseholdId householdId,
    EventId completionEventId) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_UPD_PROFESSION_ORDER_COMPLETE);
    stmt->SetData(0, completionEventId);
    stmt->SetData(1, completionEventId);
    stmt->SetData(2, instanceId);
    stmt->SetData(3, householdId);
    FuryDatabase.Execute(stmt);
}
}
