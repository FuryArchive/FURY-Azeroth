#ifndef MOD_FURY_HOUSEHOLD_SERVICE_H
#define MOD_FURY_HOUSEHOLD_SERVICE_H

#include "Household.h"

#include <optional>
#include <string_view>

namespace Fury
{
class HouseholdRepository;

class HouseholdService final
{
public:
    explicit HouseholdService(HouseholdRepository const& repository);

    [[nodiscard]] HouseholdCreateResult CreateOrGet(
        std::string_view slug,
        std::string_view displayName) const;

    [[nodiscard]] HouseholdMemberResult AddMember(
        HouseholdId householdId,
        uint32 accountId,
        uint8 role = 1) const;

    void RemoveMember(HouseholdId householdId, uint32 accountId) const;

    [[nodiscard]] std::optional<HouseholdId> FindByAccount(uint32 accountId) const;

private:
    static constexpr uint32 MaxHumanMembers = 2;

    HouseholdRepository const& _repository;
};
}

#endif
