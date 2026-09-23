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

    [[nodiscard]] std::optional<DirectorRun> FindLatestGraph(
        HouseholdId householdId, std::string_view graphKey) const;

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

    void RecoverRuntime(
        DirectorRunId runId,
        HouseholdId householdId,
        uint64 expectedRevision,
        uint64 expectedRuntimeId,
        uint64 replacementRuntimeId,
        EventId sourceEventId) const;

    void Resolve(
        DirectorRunId runId,
        HouseholdId householdId,
        uint64 expectedRevision,
        std::string_view outcomeKey,
        EventId sourceEventId) const;

    void BindTerminalEvent(
        DirectorRunId runId,
        HouseholdId householdId,
        EventId terminalEventId) const;

    void Abort(
        DirectorRunId runId,
        HouseholdId householdId,
        uint64 expectedRevision,
        std::string_view outcomeKey,
        EventId sourceEventId) const;
};
}

#endif
