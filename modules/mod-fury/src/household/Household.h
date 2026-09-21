#ifndef MOD_FURY_HOUSEHOLD_H
#define MOD_FURY_HOUSEHOLD_H

#include "actors/ActorContext.h"

#include <optional>
#include <string>

namespace Fury
{
enum class HouseholdMemberResult : uint8
{
    Added = 1,
    AlreadyMember = 2,
    AccountInOtherHousehold = 3,
    HouseholdFull = 4,
    HouseholdNotFound = 5
};

struct HouseholdCreateResult
{
    std::optional<HouseholdId> householdId;

    [[nodiscard]] bool Succeeded() const
    {
        return householdId.has_value();
    }
};
}

#endif
