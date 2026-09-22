#ifndef MOD_FURY_CONTRACT_TYPES_H
#define MOD_FURY_CONTRACT_TYPES_H

#include "events/FuryEvent.h"

#include <optional>
#include <string>

namespace Fury
{
using ContractInstanceId = uint64;
using DirectorRunId = uint64;

enum class ContractRepeatPolicy : uint8
{
    Once = 1,
    Repeatable = 2,
    DirectorRun = 3
};

enum class ContractStatus : uint8
{
    Available = 1,
    Active = 2,
    Complete = 3,
    Failed = 4,
    Expired = 5
};

enum class ObjectiveType : uint8
{
    Kill = 1,
    Boss = 2,
    Collect = 3,
    Craft = 4,
    Deliver = 5,
    Explore = 6,
    Escort = 7,
    Defend = 8,
    Hunt = 9,
    Dungeon = 10,
    Event = 11
};

struct ContractDefinition
{
    std::string contractKey;
    std::string boardKey;
    std::string title;
    std::optional<std::string> campaignNodeKey;
    std::optional<std::string> directorPhase;
    ContractRepeatPolicy repeatPolicy = ContractRepeatPolicy::Once;
    std::optional<std::string> rewardKey;
    bool enabled = false;
};

struct ContractInstance
{
    ContractInstanceId id = 0;
    HouseholdId householdId = 0;
    std::string contractKey;
    ContractStatus status = ContractStatus::Active;
    EventId acceptedEventId = 0;
    std::optional<EventId> completedEventId;
    uint64 revision = 0;
};

struct ContractObjectiveMatch
{
    ContractInstanceId instanceId = 0;
    std::string contractKey;
    uint16 ordinal = 0;
    uint32 requiredCount = 0;
    uint32 progressCount = 0;
    EventId lastEventId = 0;
};

enum class ContractAcceptOutcome : uint8
{
    Accepted = 1,
    AlreadyActive = 2,
    AlreadyCompleted = 3,
    InvalidSource = 4,
    InvalidKey = 5,
    NotFound = 6,
    Disabled = 7,
    MissingObjectives = 8,
    DirectorRunRequired = 9,
    PersistenceFailed = 10
};

struct ContractAcceptResult
{
    ContractAcceptOutcome outcome = ContractAcceptOutcome::PersistenceFailed;
    std::optional<ContractInstanceId> instanceId;

    [[nodiscard]] bool AcceptedOrExisting() const
    {
        return outcome == ContractAcceptOutcome::Accepted ||
            outcome == ContractAcceptOutcome::AlreadyActive;
    }
};
}

#endif
