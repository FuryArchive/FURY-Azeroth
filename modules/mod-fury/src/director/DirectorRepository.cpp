#include "DirectorRepository.h"

#include "database/FuryDatabase.h"
#include "DatabaseEnv.h"

#include <string>
#include <utility>

namespace Fury
{
namespace
{
DirectorRun ReadRun(Field* fields)
{
    DirectorRun run;
    run.id = fields[0].Get<DirectorRunId>();
    run.householdId = fields[1].Get<HouseholdId>();
    run.graphKey = fields[2].Get<std::string>();
    run.scopeKey = fields[3].Get<std::string>();
    run.status = static_cast<DirectorRunStatus>(fields[4].Get<uint8>());
    run.phaseKey = fields[5].Get<std::string>();

    if (!fields[6].IsNull())
        run.externalRuntimeId = fields[6].Get<uint64>();

    run.startedEventId = fields[7].Get<EventId>();
    run.lastEventId = fields[8].Get<EventId>();

    if (!fields[9].IsNull())
        run.resolvedEventId = fields[9].Get<EventId>();

    if (!fields[10].IsNull())
        run.outcomeKey = fields[10].Get<std::string>();

    run.revision = fields[11].Get<uint64>();
    return run;
}
}

std::optional<DirectorGraphDefinition> DirectorRepository::FindGraph(
    std::string_view graphKey) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_SEL_DIRECTOR_GRAPH);
    stmt->SetData(0, std::string(graphKey));

    PreparedQueryResult result = FuryDatabase.Query(stmt);
    if (!result)
        return std::nullopt;

    Field* fields = result->Fetch();

    DirectorGraphDefinition graph;
    graph.graphKey = std::string(graphKey);
    graph.scopeKey = fields[0].Get<std::string>();
    graph.displayName = fields[1].Get<std::string>();

    if (!fields[2].IsNull())
        graph.campaignNodeKey = fields[2].Get<std::string>();

    graph.enabled = fields[3].Get<uint8>() != 0;
    return graph;
}

std::optional<DirectorRun> DirectorRepository::FindRun(
    DirectorRunId runId) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_SEL_DIRECTOR_RUN_BY_ID);
    stmt->SetData(0, runId);

    PreparedQueryResult result = FuryDatabase.Query(stmt);
    if (!result)
        return std::nullopt;

    return ReadRun(result->Fetch());
}

std::optional<DirectorRun> DirectorRepository::FindActiveScope(
    HouseholdId householdId,
    std::string_view scopeKey) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_SEL_DIRECTOR_ACTIVE_SCOPE);
    stmt->SetData(0, householdId);
    stmt->SetData(1, std::string(scopeKey));

    PreparedQueryResult result = FuryDatabase.Query(stmt);
    if (!result)
        return std::nullopt;

    return ReadRun(result->Fetch());
}

std::vector<DirectorRun> DirectorRepository::LoadActiveRuns() const
{
    std::vector<DirectorRun> runs;

    PreparedQueryResult result =
        FuryDatabase.Query(
            FuryDatabase.GetPreparedStatement(FURY_SEL_DIRECTOR_ACTIVE_RUNS));
    if (!result)
        return runs;

    do
    {
        runs.push_back(ReadRun(result->Fetch()));
    } while (result->NextRow());

    return runs;
}

void DirectorRepository::InsertRun(
    HouseholdId householdId,
    DirectorGraphDefinition const& graph,
    std::string_view initialPhase,
    EventId sourceEventId) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_INS_DIRECTOR_RUN);
    stmt->SetData(0, householdId);
    stmt->SetData(1, graph.graphKey);
    stmt->SetData(2, graph.scopeKey);
    stmt->SetData(3, std::string(initialPhase));
    stmt->SetData(4, sourceEventId);
    stmt->SetData(5, sourceEventId);
    FuryDatabase.Execute(stmt);
}

void DirectorRepository::UpdatePhase(
    DirectorRunId runId,
    HouseholdId householdId,
    uint64 expectedRevision,
    std::string_view phaseKey,
    EventId sourceEventId) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_UPD_DIRECTOR_PHASE);
    stmt->SetData(0, std::string(phaseKey));
    stmt->SetData(1, sourceEventId);
    stmt->SetData(2, runId);
    stmt->SetData(3, householdId);
    stmt->SetData(4, expectedRevision);
    FuryDatabase.Execute(stmt);
}

void DirectorRepository::AttachRuntime(
    DirectorRunId runId,
    HouseholdId householdId,
    uint64 expectedRevision,
    uint64 externalRuntimeId,
    EventId sourceEventId) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_UPD_DIRECTOR_RUNTIME);
    stmt->SetData(0, externalRuntimeId);
    stmt->SetData(1, sourceEventId);
    stmt->SetData(2, runId);
    stmt->SetData(3, householdId);
    stmt->SetData(4, expectedRevision);
    FuryDatabase.Execute(stmt);
}

void DirectorRepository::Resolve(
    DirectorRunId runId,
    HouseholdId householdId,
    uint64 expectedRevision,
    std::string_view outcomeKey,
    EventId sourceEventId) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_UPD_DIRECTOR_RESOLVE);
    stmt->SetData(0, std::string(outcomeKey));
    stmt->SetData(1, sourceEventId);
    stmt->SetData(2, sourceEventId);
    stmt->SetData(3, runId);
    stmt->SetData(4, householdId);
    stmt->SetData(5, expectedRevision);
    FuryDatabase.Execute(stmt);
}

void DirectorRepository::Abort(
    DirectorRunId runId,
    HouseholdId householdId,
    uint64 expectedRevision,
    std::string_view outcomeKey,
    EventId sourceEventId) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_UPD_DIRECTOR_ABORT);
    stmt->SetData(0, std::string(outcomeKey));
    stmt->SetData(1, sourceEventId);
    stmt->SetData(2, sourceEventId);
    stmt->SetData(3, runId);
    stmt->SetData(4, householdId);
    stmt->SetData(5, expectedRevision);
    FuryDatabase.Execute(stmt);
}
}
