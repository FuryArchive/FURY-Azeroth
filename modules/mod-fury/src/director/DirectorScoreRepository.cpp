#include "DirectorScoreRepository.h"

#include "database/FuryDatabase.h"
#include "DatabaseEnv.h"

#include <string>
#include <utility>

namespace Fury
{
std::vector<DirectorScoreComponent>
DirectorScoreRepository::FindMatchingComponents(
    std::string_view graphKey,
    FuryEvent const& event) const
{
    std::vector<DirectorScoreComponent> components;

    auto* stmt =
        FuryDatabase.GetPreparedStatement(
            FURY_SEL_DIRECTOR_SCORE_COMPONENT_MATCHES);
    stmt->SetData(0, std::string(graphKey));
    stmt->SetData(1, event.type);

    if (!event.correlationKey.empty())
        stmt->SetData(2, event.correlationKey);
    else
        stmt->SetData(2, nullptr);

    PreparedQueryResult result = FuryDatabase.Query(stmt);
    if (!result)
        return components;

    do
    {
        Field* fields = result->Fetch();

        DirectorScoreComponent component;
        component.graphKey = std::string(graphKey);
        component.componentKey =
            fields[0].Get<std::string>();
        component.scoreValue =
            fields[1].Get<uint16>();
        component.sourceEventType = event.type;

        if (!fields[2].IsNull())
        {
            component.sourceCorrelationKey =
                fields[2].Get<std::string>();
        }

        component.enabled = true;
        components.push_back(std::move(component));
    } while (result->NextRow());

    return components;
}

std::optional<uint32>
DirectorScoreRepository::DefinitionTotal(
    std::string_view graphKey) const
{
    auto* stmt =
        FuryDatabase.GetPreparedStatement(
            FURY_SEL_DIRECTOR_SCORE_DEFINITION_TOTAL);
    stmt->SetData(0, std::string(graphKey));

    PreparedQueryResult result = FuryDatabase.Query(stmt);
    if (!result)
        return std::nullopt;

    return result->Fetch()[0].Get<uint32>();
}

std::optional<DirectorRunId>
DirectorScoreRepository::ResolveRunForEvent(
    std::string_view graphKey,
    FuryEvent const& event) const
{
    if (!event.actor.householdId ||
        !*event.actor.householdId ||
        !event.subjectId)
    {
        return std::nullopt;
    }

    DatabasePreparedStatement* stmt = nullptr;

    if (event.type == "contract.completed" &&
        event.sourceSystem == "fury.contracts" &&
        event.subjectType == "contract_instance" &&
        !event.correlationKey.empty())
    {
        stmt = FuryDatabase.GetPreparedStatement(
            FURY_SEL_DIRECTOR_SCORE_RUN_FOR_CONTRACT);
        stmt->SetData(0, *event.subjectId);
        stmt->SetData(1, *event.actor.householdId);
        stmt->SetData(2, event.correlationKey);
        stmt->SetData(3, std::string(graphKey));
        stmt->SetData(4, event.id);
    }
    else if (event.type == "defias.final_stage.participated" &&
             event.sourceSystem == "fury.defias" &&
             event.subjectType == "living_world_runtime" &&
             std::string_view(event.correlationKey) == graphKey)
    {
        stmt = FuryDatabase.GetPreparedStatement(
            FURY_SEL_DIRECTOR_SCORE_RUN_FOR_RUNTIME);
        stmt->SetData(0, *event.actor.householdId);
        stmt->SetData(1, std::string(graphKey));
        stmt->SetData(2, *event.subjectId);
    }
    else
    {
        return std::nullopt;
    }

    PreparedQueryResult result = FuryDatabase.Query(stmt);
    if (!result)
        return std::nullopt;

    return result->Fetch()[0].Get<DirectorRunId>();
}

void DirectorScoreRepository::Award(
    DirectorRunId directorRunId,
    std::string_view graphKey,
    DirectorScoreComponent const& component,
    EventId sourceEventId) const
{
    auto* stmt =
        FuryDatabase.GetPreparedStatement(
            FURY_INS_DIRECTOR_SCORE_AWARD);

    uint8 index = 0;
    stmt->SetData(index++, directorRunId);
    stmt->SetData(index++, std::string(graphKey));
    stmt->SetData(index++, component.componentKey);
    stmt->SetData(index++, component.scoreValue);
    stmt->SetData(index++, sourceEventId);
    stmt->SetData(index++, directorRunId);
    stmt->SetData(index++, std::string(graphKey));

    FuryDatabase.Execute(stmt);
}

std::optional<DirectorScoreAward>
DirectorScoreRepository::FindAward(
    DirectorRunId directorRunId,
    std::string_view componentKey) const
{
    auto* stmt =
        FuryDatabase.GetPreparedStatement(
            FURY_SEL_DIRECTOR_SCORE_AWARD);
    stmt->SetData(0, directorRunId);
    stmt->SetData(1, std::string(componentKey));

    PreparedQueryResult result = FuryDatabase.Query(stmt);
    if (!result)
        return std::nullopt;

    Field* fields = result->Fetch();

    DirectorScoreAward award;
    award.directorRunId = directorRunId;
    award.graphKey = fields[0].Get<std::string>();
    award.componentKey = std::string(componentKey);
    award.scoreValue = fields[1].Get<uint16>();
    award.sourceEventId = fields[2].Get<EventId>();
    return award;
}

std::optional<uint32>
DirectorScoreRepository::Score(
    DirectorRunId directorRunId) const
{
    auto* stmt =
        FuryDatabase.GetPreparedStatement(
            FURY_SEL_DIRECTOR_SCORE_TOTAL);
    stmt->SetData(0, directorRunId);

    PreparedQueryResult result = FuryDatabase.Query(stmt);
    if (!result)
        return std::nullopt;

    return result->Fetch()[0].Get<uint32>();
}
}
