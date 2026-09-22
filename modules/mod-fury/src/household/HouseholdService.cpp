#include "HouseholdService.h"

#include "HouseholdRepository.h"

#include "Log.h"

#include <string>
#include <unordered_map>

namespace Fury
{
HouseholdService::HouseholdService(HouseholdRepository const& repository)
    : _repository(repository)
{
}

bool HouseholdService::Initialize()
{
    auto memberships = _repository.LoadMemberships();

    std::unordered_map<HouseholdId, uint32> counts;
    for (auto const& [accountId, householdId] : memberships)
    {
        (void)accountId;
        uint32& count = counts[householdId];
        ++count;

        if (count > MaxHumanMembers)
        {
            LOG_ERROR(
                "server.loading",
                "[FURY] household {} has {} member accounts; M1 maximum is {}. "
                "Refusing to initialize the household cache.",
                householdId,
                count,
                MaxHumanMembers);
            return false;
        }
    }

    std::unique_lock lock(_membershipMutex);
    _membershipByAccount.clear();

    for (auto const& [accountId, householdId] : memberships)
        _membershipByAccount.emplace(accountId, householdId);

    return true;
}

void HouseholdService::Shutdown()
{
    std::unique_lock lock(_membershipMutex);
    _membershipByAccount.clear();
}

HouseholdCreateResult HouseholdService::CreateOrGet(
    std::string_view slug,
    std::string_view displayName) const
{
    if (slug.empty() || displayName.empty() || slug.size() > 64 || displayName.size() > 96)
        return {};

    return {_repository.CreateOrGet(slug, displayName)};
}

HouseholdMemberResult HouseholdService::AddMember(
    HouseholdId householdId,
    uint32 accountId,
    uint8 role) const
{
    std::lock_guard<std::mutex> mutationLock(_mutationMutex);

    if (!_repository.Exists(householdId))
        return HouseholdMemberResult::HouseholdNotFound;

    if (std::optional<HouseholdId> current = _repository.FindByAccount(accountId))
    {
        if (*current == householdId)
            return HouseholdMemberResult::AlreadyMember;

        return HouseholdMemberResult::AccountInOtherHousehold;
    }

    if (_repository.CountMembers(householdId) >= MaxHumanMembers)
        return HouseholdMemberResult::HouseholdFull;

    _repository.AddMember(householdId, accountId, role);

    // INSERT IGNORE protects database uniqueness, but it also means callers
    // must verify what actually persisted before mutating the in-memory cache.
    // This prevents cache/database divergence if another writer won the
    // account uniqueness race or the insert failed.
    std::optional<HouseholdId> persisted =
        _repository.FindByAccount(accountId);
    if (!persisted)
        return HouseholdMemberResult::PersistenceFailed;

    if (*persisted != householdId)
        return HouseholdMemberResult::AccountInOtherHousehold;

    {
        std::unique_lock lock(_membershipMutex);
        _membershipByAccount[accountId] = householdId;
    }

    return HouseholdMemberResult::Added;
}

void HouseholdService::RemoveMember(HouseholdId householdId, uint32 accountId) const
{
    std::lock_guard<std::mutex> mutationLock(_mutationMutex);
    _repository.RemoveMember(householdId, accountId);

    // Re-read the authoritative row before touching the cache. A failed DELETE
    // must not make ActorResolver believe the account left its household.
    std::optional<HouseholdId> persisted =
        _repository.FindByAccount(accountId);

    std::unique_lock lock(_membershipMutex);
    if (persisted)
    {
        _membershipByAccount[accountId] = *persisted;
        return;
    }

    auto itr = _membershipByAccount.find(accountId);
    if (itr != _membershipByAccount.end() && itr->second == householdId)
        _membershipByAccount.erase(itr);
}

std::optional<HouseholdId> HouseholdService::FindByAccount(uint32 accountId) const
{
    std::shared_lock lock(_membershipMutex);
    auto itr = _membershipByAccount.find(accountId);
    if (itr == _membershipByAccount.end())
        return std::nullopt;

    return itr->second;
}

uint32 HouseholdService::CountMembers(HouseholdId householdId) const
{
    return _repository.CountMembers(householdId);
}
}
