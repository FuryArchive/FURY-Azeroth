#ifndef MOD_FURY_HOUSEHOLD_SERVICE_H
#define MOD_FURY_HOUSEHOLD_SERVICE_H

#include "Household.h"

#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string_view>
#include <unordered_map>

namespace Fury
{
class HouseholdRepository;

class HouseholdService final
{
public:
    explicit HouseholdService(HouseholdRepository const& repository);

    bool Initialize();
    void Shutdown();

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

    mutable std::shared_mutex _membershipMutex;
    mutable std::unordered_map<uint32, HouseholdId> _membershipByAccount;
};
}

#endif
