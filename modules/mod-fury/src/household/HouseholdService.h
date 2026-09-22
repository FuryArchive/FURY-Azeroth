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

    [[nodiscard]] HouseholdMemberResult AddHumanMember(
        HouseholdId householdId,
        ActorContext const& actor,
        uint8 role = 1) const;

    void RemoveMember(HouseholdId householdId, uint32 accountId) const;

    [[nodiscard]] std::optional<HouseholdId> FindByAccount(uint32 accountId) const;
    [[nodiscard]] uint32 CountMembers(HouseholdId householdId) const;

private:
    [[nodiscard]] HouseholdMemberResult AddMemberAccount(
        HouseholdId householdId,
        uint32 accountId,
        uint8 role) const;

    static constexpr uint32 MaxHumanMembers = 2;

    HouseholdRepository const& _repository;

    // Serialize membership mutations so the two-member invariant is not
    // vulnerable to COUNT -> INSERT races inside one worldserver process.
    mutable std::mutex _mutationMutex;
    mutable std::shared_mutex _membershipMutex;
    mutable std::unordered_map<uint32, HouseholdId> _membershipByAccount;
};
}

#endif
