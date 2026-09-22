#ifndef MOD_FURY_DIRECTOR_SERVICE_H
#define MOD_FURY_DIRECTOR_SERVICE_H

#include "DirectorRepository.h"

namespace Fury
{
class EventStore;

class DirectorService final
{
public:
    DirectorService(
        DirectorRepository const& repository,
        EventStore const& events);

    [[nodiscard]] DirectorResult Start(
        FuryEvent const& source,
        std::string_view graphKey,
        std::string_view initialPhase) const;

    [[nodiscard]] DirectorResult AdvancePhase(
        FuryEvent const& source,
        DirectorRunId runId,
        uint64 expectedRevision,
        std::string_view phaseKey) const;

    [[nodiscard]] DirectorResult AttachRuntime(
        FuryEvent const& source,
        DirectorRunId runId,
        uint64 expectedRevision,
        uint64 externalRuntimeId) const;

    [[nodiscard]] DirectorResult Resolve(
        FuryEvent const& source,
        DirectorRunId runId,
        uint64 expectedRevision,
        std::string_view outcomeKey) const;

    [[nodiscard]] DirectorResult Abort(
        FuryEvent const& source,
        DirectorRunId runId,
        uint64 expectedRevision,
        std::string_view outcomeKey) const;

    [[nodiscard]] std::optional<DirectorRun> FindRun(
        DirectorRunId runId) const
    {
        return _repository.FindRun(runId);
    }

    [[nodiscard]] std::vector<DirectorRun> ActiveRuns() const
    {
        return _repository.LoadActiveRuns();
    }

private:
    [[nodiscard]] std::optional<EventId> EmitStartedEvent(
        FuryEvent const& source,
        DirectorRun const& run) const;

    [[nodiscard]] std::optional<EventId> EmitPhaseEvent(
        FuryEvent const& source,
        DirectorRun const& run) const;

    [[nodiscard]] std::optional<EventId> EmitRuntimeEvent(
        FuryEvent const& source,
        DirectorRun const& run) const;

    [[nodiscard]] std::optional<EventId> EmitTerminalEvent(
        FuryEvent const& source,
        DirectorRun const& run,
        std::string_view eventType) const;

    static bool IsStartSource(FuryEvent const& source);
    static bool IsMutationSource(
        FuryEvent const& source,
        HouseholdId householdId);

    DirectorRepository const& _repository;
    EventStore const& _events;
};
}

#endif
