#include "HouseholdService.h"

#include "HouseholdRepository.h"

#include <string>

namespace Fury
{
HouseholdService::HouseholdService(HouseholdRepository const& repository)
    : _repository(repository)
{
}

bool HouseholdService::Initialize()
{
    auto memberships = _repository.LoadMemberships();

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

    {
        std::unique_lock lock(_membershipMutex);
        _membershipByAccount[accountId] = householdId;
    }

    return HouseholdMemberResult::Added;
}

void HouseholdService::RemoveMember(HouseholdId householdId, uint32 accountId) const
{
    _repository.RemoveMember(householdId, accountId);

    std::unique_lock lock(_membershipMutex);
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
}
