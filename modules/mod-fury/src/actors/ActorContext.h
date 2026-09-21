#ifndef MOD_FURY_ACTOR_CONTEXT_H
#define MOD_FURY_ACTOR_CONTEXT_H

#include "Define.h"
#include "ObjectGuid.h"

#include <optional>

namespace Fury
{
using HouseholdId = uint64;

enum class ActorKind : uint8
{
    Human = 1,
    HouseholdAltBot = 2,
    RandomPlayerBot = 3,
    NpcAssistant = 4,
    System = 5
};

struct ActorContext
{
    ActorKind kind = ActorKind::System;
    ObjectGuid characterGuid = ObjectGuid::Empty;
    uint32 accountId = 0;
    std::optional<HouseholdId> householdId;

    bool isEligibleForPersistentProgression = false;

    [[nodiscard]] bool IsHuman() const
    {
        return kind == ActorKind::Human;
    }
};
}

#endif
