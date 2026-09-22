#ifndef MOD_FURY_DIRECTOR_TYPES_H
#define MOD_FURY_DIRECTOR_TYPES_H

#include "core/FuryIds.h"
#include "events/FuryEvent.h"

#include <optional>
#include <string>
#include <vector>

namespace Fury
{
enum class DirectorRunStatus : uint8
{
    Pending = 1,
    Active = 2,
    Resolving = 3,
    Complete = 4,
    Failed = 5,
    Aborted = 6
};

struct DirectorGraphDefinition
{
    std::string graphKey;
    std::string scopeKey;
    std::string displayName;
    std::optional<std::string> campaignNodeKey;
    bool enabled = false;
};

struct DirectorRun
{
    DirectorRunId id = 0;
    HouseholdId householdId = 0;
    std::string graphKey;
    std::string scopeKey;
    DirectorRunStatus status = DirectorRunStatus::Pending;
    std::string phaseKey;
    std::optional<uint64> externalRuntimeId;
    uint32 participationScore = 0;
    EventId startedEventId = 0;
    EventId lastEventId = 0;
    std::optional<EventId> resolvedEventId;
    std::optional<std::string> outcomeKey;
    uint64 revision = 0;
};

struct DirectorParticipation
{
    DirectorRunId runId = 0;
    std::string contributionKey;
    uint32 points = 0;
    EventId sourceEventId = 0;
};

enum class DirectorOutcome : uint8
{
    Started = 1,
    AlreadyActive = 2,
    ScopeBusy = 3,
    Updated = 4,
    AlreadyApplied = 5,
    InvalidSource = 6,
    InvalidKey = 7,
    GraphNotFound = 8,
    GraphDisabled = 9,
    RunNotFound = 10,
    InvalidState = 11,
    RevisionConflict = 12,
    RuntimeConflict = 13,
    PersistenceFailed = 14,
    ParticipationConflict = 15
};

struct DirectorResult
{
    DirectorOutcome outcome = DirectorOutcome::PersistenceFailed;
    std::optional<DirectorRun> run;

    [[nodiscard]] bool Accepted() const
    {
        return outcome == DirectorOutcome::Started ||
            outcome == DirectorOutcome::AlreadyActive ||
            outcome == DirectorOutcome::Updated ||
            outcome == DirectorOutcome::AlreadyApplied;
    }
};
}

#endif
