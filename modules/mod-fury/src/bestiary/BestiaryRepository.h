#ifndef MOD_FURY_BESTIARY_REPOSITORY_H
#define MOD_FURY_BESTIARY_REPOSITORY_H

#include "BestiaryTypes.h"

#include <optional>
#include <string_view>
#include <vector>

namespace Fury
{
class BestiaryRepository final
{
public:
    [[nodiscard]] std::vector<BestiaryMapping> FindMappings(
        uint32 creatureEntry) const;

    [[nodiscard]] std::vector<BestiaryMapping> FindEventMappings(
        std::string_view eventType,
        std::string_view subjectType,
        uint64 subjectId) const;

    [[nodiscard]] std::vector<uint32> FindHouseholdAccountsAtLeastLevel(
        HouseholdId householdId,
        std::string_view entryKey,
        BestiaryDiscoveryLevel level) const;

    [[nodiscard]] std::optional<BestiaryState> FindState(
        uint32 accountId,
        std::string_view entryKey) const;

    void ApplyKill(
        uint32 accountId,
        std::string_view entryKey,
        BestiaryDiscoveryLevel level,
        EventId eventId) const;

    void ApplyLevel(
        uint32 accountId,
        std::string_view entryKey,
        BestiaryDiscoveryLevel level,
        EventId eventId) const;
};
}

#endif
