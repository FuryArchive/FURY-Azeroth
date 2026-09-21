#ifndef MOD_FURY_HOUSEHOLD_REPOSITORY_H
#define MOD_FURY_HOUSEHOLD_REPOSITORY_H

#include "Household.h"

#include <optional>
#include <string_view>

namespace Fury
{
class HouseholdRepository final
{
public:
    [[nodiscard]] bool Exists(HouseholdId householdId) const;
    [[nodiscard]] std::optional<HouseholdId> FindBySlug(std::string_view slug) const;
    [[nodiscard]] std::optional<HouseholdId> FindByAccount(uint32 accountId) const;
    [[nodiscard]] uint32 CountMembers(HouseholdId householdId) const;

    [[nodiscard]] std::optional<HouseholdId> CreateOrGet(
        std::string_view slug,
        std::string_view displayName) const;

    void AddMember(HouseholdId householdId, uint32 accountId, uint8 role) const;
    void RemoveMember(HouseholdId householdId, uint32 accountId) const;
};
}

#endif
