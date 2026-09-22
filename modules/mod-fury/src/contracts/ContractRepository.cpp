#include "ContractRepository.h"

#include "database/FuryDatabase.h"
#include "DatabaseEnv.h"

#include <string>
#include <utility>

namespace Fury
{
std::optional<ContractDefinition> ContractRepository::FindDefinition(
    std::string_view contractKey) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_SEL_CONTRACT_DEFINITION);
    stmt->SetData(0, std::string(contractKey));

    PreparedQueryResult result = FuryDatabase.Query(stmt);
    if (!result)
        return std::nullopt;

    Field* fields = result->Fetch();

    ContractDefinition definition;
    definition.contractKey = std::string(contractKey);
    definition.boardKey = fields[0].Get<std::string>();
    definition.title = fields[1].Get<std::string>();

    if (!fields[2].IsNull())
        definition.campaignNodeKey = fields[2].Get<std::string>();

    if (!fields[3].IsNull())
        definition.directorPhase = fields[3].Get<std::string>();

    definition.repeatPolicy =
        static_cast<ContractRepeatPolicy>(fields[4].Get<uint8>());

    if (!fields[5].IsNull())
        definition.rewardKey = fields[5].Get<std::string>();

    definition.enabled = fields[6].Get<uint8>() != 0;
    return definition;
}

std::optional<ContractInstance> ContractRepository::FindActiveInstance(
    HouseholdId householdId,
    std::string_view contractKey) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_SEL_CONTRACT_ACTIVE_INSTANCE);
    stmt->SetData(0, householdId);
    stmt->SetData(1, std::string(contractKey));

    PreparedQueryResult result = FuryDatabase.Query(stmt);
    if (!result)
        return std::nullopt;

    Field* fields = result->Fetch();

    ContractInstance instance;
    instance.id = fields[0].Get<ContractInstanceId>();
    instance.householdId = householdId;
    instance.contractKey = std::string(contractKey);
    instance.status = static_cast<ContractStatus>(fields[1].Get<uint8>());
    instance.acceptedEventId = fields[2].Get<EventId>();

    if (!fields[3].IsNull())
        instance.completedEventId = fields[3].Get<EventId>();

    instance.revision = fields[4].Get<uint64>();
    return instance;
}

std::optional<ContractInstance> ContractRepository::FindInstance(
    ContractInstanceId instanceId) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_SEL_CONTRACT_INSTANCE_BY_ID);
    stmt->SetData(0, instanceId);

    PreparedQueryResult result = FuryDatabase.Query(stmt);
    if (!result)
        return std::nullopt;

    Field* fields = result->Fetch();

    ContractInstance instance;
    instance.id = instanceId;
    instance.householdId = fields[0].Get<HouseholdId>();
    instance.contractKey = fields[1].Get<std::string>();
    instance.status = static_cast<ContractStatus>(fields[2].Get<uint8>());
    instance.acceptedEventId = fields[3].Get<EventId>();

    if (!fields[4].IsNull())
        instance.completedEventId = fields[4].Get<EventId>();

    instance.revision = fields[5].Get<uint64>();
    return instance;
}

bool ContractRepository::HasCompletedInstance(
    HouseholdId householdId,
    std::string_view contractKey) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_SEL_CONTRACT_COMPLETED_INSTANCE);
    stmt->SetData(0, householdId);
    stmt->SetData(1, std::string(contractKey));
    return static_cast<bool>(FuryDatabase.Query(stmt));
}

std::optional<uint32> ContractRepository::CountObjectives(
    std::string_view contractKey) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_SEL_CONTRACT_OBJECTIVE_COUNT);
    stmt->SetData(0, std::string(contractKey));

    PreparedQueryResult result = FuryDatabase.Query(stmt);
    if (!result)
        return std::nullopt;

    return result->Fetch()[0].Get<uint32>();
}

void ContractRepository::InsertActiveInstance(
    HouseholdId householdId,
    std::string_view contractKey,
    std::optional<DirectorRunId> directorRunId,
    EventId acceptedEventId) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_INS_CONTRACT_INSTANCE);

    stmt->SetData(0, householdId);
    stmt->SetData(1, std::string(contractKey));

    if (directorRunId)
        stmt->SetData(2, *directorRunId);
    else
        stmt->SetData(2, nullptr);

    stmt->SetData(3, acceptedEventId);
    FuryDatabase.Execute(stmt);
}

void ContractRepository::InitializeProgress(
    ContractInstanceId instanceId,
    std::string_view contractKey) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_INS_CONTRACT_PROGRESS_ROWS);
    stmt->SetData(0, instanceId);
    stmt->SetData(1, std::string(contractKey));
    FuryDatabase.Execute(stmt);
}

std::vector<ContractObjectiveMatch> ContractRepository::FindMatchingObjectives(
    FuryEvent const& event) const
{
    std::vector<ContractObjectiveMatch> matches;
    if (!event.actor.householdId)
        return matches;

    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_SEL_CONTRACT_MATCHING_OBJECTIVES);

    stmt->SetData(0, event.type);

    if (!event.subjectType.empty())
        stmt->SetData(1, event.subjectType);
    else
        stmt->SetData(1, nullptr);

    if (event.subjectId)
        stmt->SetData(2, *event.subjectId);
    else
        stmt->SetData(2, nullptr);

    stmt->SetData(3, *event.actor.householdId);

    PreparedQueryResult result = FuryDatabase.Query(stmt);
    if (!result)
        return matches;

    do
    {
        Field* fields = result->Fetch();

        ContractObjectiveMatch match;
        match.instanceId = fields[0].Get<ContractInstanceId>();
        match.contractKey = fields[1].Get<std::string>();
        match.ordinal = fields[2].Get<uint16>();
        match.requiredCount = fields[3].Get<uint32>();
        match.progressCount = fields[4].Get<uint32>();
        match.lastEventId = fields[5].Get<EventId>();

        matches.push_back(std::move(match));
    } while (result->NextRow());

    return matches;
}

void ContractRepository::AdvanceObjective(
    ContractObjectiveMatch const& objective,
    EventId eventId) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_UPD_CONTRACT_PROGRESS);

    uint8 index = 0;
    stmt->SetData(index++, objective.contractKey);
    stmt->SetData(index++, eventId);
    stmt->SetData(index++, objective.instanceId);
    stmt->SetData(index++, objective.ordinal);
    stmt->SetData(index++, eventId);
    FuryDatabase.Execute(stmt);
}

std::optional<ContractObjectiveProgress> ContractRepository::FindProgress(
    ContractInstanceId instanceId,
    uint16 objectiveOrdinal) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_SEL_CONTRACT_PROGRESS_ROW);
    stmt->SetData(0, instanceId);
    stmt->SetData(1, objectiveOrdinal);

    PreparedQueryResult result = FuryDatabase.Query(stmt);
    if (!result)
        return std::nullopt;

    Field* fields = result->Fetch();

    ContractObjectiveProgress progress;
    progress.progressCount = fields[0].Get<uint32>();
    progress.lastEventId = fields[1].Get<EventId>();
    return progress;
}

std::optional<uint32> ContractRepository::CountIncompleteObjectives(
    ContractInstanceId instanceId) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_SEL_CONTRACT_INCOMPLETE_COUNT);
    stmt->SetData(0, instanceId);

    PreparedQueryResult result = FuryDatabase.Query(stmt);
    if (!result)
        return std::nullopt;

    return result->Fetch()[0].Get<uint32>();
}

void ContractRepository::MarkComplete(
    ContractInstanceId instanceId,
    EventId completionEventId) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_UPD_CONTRACT_COMPLETE);
    stmt->SetData(0, completionEventId);
    stmt->SetData(1, instanceId);
    FuryDatabase.Execute(stmt);
}
}
