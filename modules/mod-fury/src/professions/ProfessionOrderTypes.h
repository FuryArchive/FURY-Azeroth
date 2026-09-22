#ifndef MOD_FURY_PROFESSION_ORDER_TYPES_H
#define MOD_FURY_PROFESSION_ORDER_TYPES_H

#include "events/FuryEvent.h"

#include <optional>
#include <string>

namespace Fury
{
using ProfessionOrderInstanceId = uint64;

enum class ProfessionOrderRepeatPolicy : uint8
{
    Once = 1,
    Repeatable = 2
};

enum class ProfessionOrderStatus : uint8
{
    Available = 1,
    Active = 2,
    Complete = 3,
    Failed = 4,
    Expired = 5
};

struct ProfessionOrderDefinition
{
    std::string orderKey;
    std::string title;
    ProfessionOrderRepeatPolicy repeatPolicy =
        ProfessionOrderRepeatPolicy::Once;
    bool enabled = false;
};

struct ProfessionOrderOption
{
    std::string orderKey;
    uint16 ordinal = 0;
    uint32 skillId = 0;
    uint32 itemId = 0;
    uint32 requiredCount = 0;
};

struct ProfessionOrderInstance
{
    ProfessionOrderInstanceId id = 0;
    HouseholdId householdId = 0;
    std::string orderKey;
    uint16 optionOrdinal = 0;
    ProfessionOrderStatus status = ProfessionOrderStatus::Active;
    uint32 progressCount = 0;
    EventId acceptedEventId = 0;
    EventId lastEventId = 0;
    std::optional<EventId> completedEventId;
    uint64 revision = 0;

    uint32 skillId = 0;
    uint32 itemId = 0;
    uint32 requiredCount = 0;
};

enum class ProfessionOrderStartOutcome : uint8
{
    Started = 1,
    AlreadyActive = 2,
    AlreadyCompleted = 3,
    InvalidSource = 4,
    InvalidKey = 5,
    OrderNotFound = 6,
    OptionNotFound = 7,
    Disabled = 8,
    PersistenceFailed = 9
};

struct ProfessionOrderStartResult
{
    ProfessionOrderStartOutcome outcome =
        ProfessionOrderStartOutcome::PersistenceFailed;
    std::optional<ProfessionOrderInstance> instance;

    [[nodiscard]] bool Accepted() const
    {
        return outcome == ProfessionOrderStartOutcome::Started ||
            outcome == ProfessionOrderStartOutcome::AlreadyActive;
    }
};
}

#endif
