#ifndef MOD_FURY_DIRECTOR_REPOSITORY_H
#define MOD_FURY_DIRECTOR_REPOSITORY_H

#include "DirectorTypes.h"

#include <optional>
#include <string_view>
#include <vector>

namespace Fury
{
class DirectorRepository final
{
public:
    [[nodiscard]] std::optional<DirectorGraphDefinition> FindGraph(
        std::string_view graphKey) const;

    [[nodiscard]] std::optional<DirectorRun> FindRun(
        DirectorRunId runId) const;

    [[nodiscard]] std::optional<DirectorRun> FindActiveScope(
        HouseholdId householdId,
        std::string_view scopeKey) const;

    [[nodiscard]] std::vector<DirectorRun> LoadActiveRuns() const;

    void InsertRun(
        HouseholdId householdId,
        DirectorGraphDefinition const& graph,
        std::string_view initialPhase,
        EventId sourceEventId) const;

    void UpdatePhase(
        DirectorRunId runId,
        HouseholdId householdId,
        uint64 expectedRevision,
        std::string_view phaseKey,
        EventId sourceEventId) const;

    void AttachRuntime(
        DirectorRunId runId,
        HouseholdId householdId,
        uint64 expectedRevision,
        uint64 externalRuntimeId,
        EventId sourceEventId) const;

    [[nodiscard]] std::optional<DirectorParticipation> FindParticipation(
        DirectorRunId runId,
        std::string_view contributionKey) const;

    [[nodiscard]] std::optional<uint32> ParticipationTotal(
        DirectorRunId runId) const;

    void InsertParticipation(
        DirectorRunId runId,
        HouseholdId householdId,
        std::string_view contributionKey,
        uint32 points,
        EventId sourceEventId,
        uint64 expectedRevision) const;

    void RecalculateParticipation(
        DirectorRunId runId,
        HouseholdId householdId,
        uint64 expectedRevision,
        EventId sourceEventId) const;

    void Resolve(
        DirectorRunId runId,
        HouseholdId householdId,
        uint64 expectedRevision,
        std::string_view outcomeKey,
        EventId sourceEventId) const;

    void Abort(
        DirectorRunId runId,
        HouseholdId householdId,
        uint64 expectedRevision,
        std::string_view outcomeKey,
        EventId sourceEventId) const;
};
}

#endif
