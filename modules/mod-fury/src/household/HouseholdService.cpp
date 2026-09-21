#include "HouseholdService.h"

#include "HouseholdRepository.h"

#include <string>

namespace Fury
{
HouseholdService::HouseholdService(HouseholdRepository const& repository)
    : _repository(repository)
{
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
    return HouseholdMemberResult::Added;
}

void HouseholdService::RemoveMember(HouseholdId householdId, uint32 accountId) const
{
    _repository.RemoveMember(householdId, accountId);
}

std::optional<HouseholdId> HouseholdService::FindByAccount(uint32 accountId) const
{
    return _repository.FindByAccount(accountId);
}
}
