#ifndef MOD_FURY_CHRONICLE_REPOSITORY_H
#define MOD_FURY_CHRONICLE_REPOSITORY_H

#include "ChronicleEntry.h"

#include <string_view>
#include <vector>

namespace Fury
{
class ChronicleRepository final
{
public:
    [[nodiscard]] bool Insert(
        HouseholdId householdId,
        EventId sourceEventId,
        std::string_view entryKey,
        std::string_view category,
        std::string_view title,
        std::string_view body,
        std::string_view metadataJson) const;

    [[nodiscard]] std::vector<ChronicleEntry> Timeline(
        HouseholdId householdId,
        uint32 limit) const;
};
}

#endif
