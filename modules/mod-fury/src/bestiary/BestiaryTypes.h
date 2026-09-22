#ifndef MOD_FURY_BESTIARY_TYPES_H
#define MOD_FURY_BESTIARY_TYPES_H

#include "events/FuryEvent.h"

#include <string>

namespace Fury
{
enum class BestiaryDiscoveryLevel : uint8
{
    Unknown = 0,
    Encountered = 1,
    Studied = 2,
    Mastered = 3
};

[[nodiscard]] constexpr bool IsValidBestiaryLevel(
    BestiaryDiscoveryLevel level)
{
    return level >= BestiaryDiscoveryLevel::Encountered &&
        level <= BestiaryDiscoveryLevel::Mastered;
}

struct BestiaryMapping
{
    std::string entryKey;
    BestiaryDiscoveryLevel discoveryLevel =
        BestiaryDiscoveryLevel::Encountered;
};

struct BestiaryState
{
    uint32 accountId = 0;
    std::string entryKey;
    BestiaryDiscoveryLevel discoveryLevel =
        BestiaryDiscoveryLevel::Unknown;
    uint32 killCount = 0;
    EventId firstEventId = 0;
    EventId lastEventId = 0;
    uint64 revision = 0;
};

enum class BestiaryAdvanceOutcome : uint8
{
    Updated = 1,
    AlreadyApplied = 2,
    InvalidSource = 3,
    InvalidKey = 4,
    InvalidLevel = 5,
    PersistenceFailed = 6
};

struct BestiaryAdvanceResult
{
    BestiaryAdvanceOutcome outcome =
        BestiaryAdvanceOutcome::PersistenceFailed;
    BestiaryState state;

    [[nodiscard]] bool Accepted() const
    {
        return outcome == BestiaryAdvanceOutcome::Updated ||
            outcome == BestiaryAdvanceOutcome::AlreadyApplied;
    }
};

static_assert(IsValidBestiaryLevel(BestiaryDiscoveryLevel::Encountered));
static_assert(IsValidBestiaryLevel(BestiaryDiscoveryLevel::Studied));
static_assert(IsValidBestiaryLevel(BestiaryDiscoveryLevel::Mastered));
static_assert(!IsValidBestiaryLevel(BestiaryDiscoveryLevel::Unknown));
}

#endif
