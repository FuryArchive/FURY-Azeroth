#ifndef MOD_FURY_ACTOR_POLICY_H
#define MOD_FURY_ACTOR_POLICY_H

#include "ActorContext.h"

namespace Fury
{
[[nodiscard]] constexpr ActorKind ClassifyActor(
    bool hasPlayer,
    bool isRealPlayer,
    bool belongsToHousehold)
{
    if (!hasPlayer)
        return ActorKind::System;

    if (isRealPlayer)
        return ActorKind::Human;

    if (belongsToHousehold)
        return ActorKind::HouseholdAltBot;

    return ActorKind::RandomPlayerBot;
}

[[nodiscard]] constexpr bool IsPersistentProgressionAuthority(ActorKind kind)
{
    return kind == ActorKind::Human;
}

[[nodiscard]] constexpr bool ShouldPersistGeneralEvent(ActorKind kind)
{
    return kind == ActorKind::Human ||
        kind == ActorKind::HouseholdAltBot;
}

static_assert(ClassifyActor(false, false, false) == ActorKind::System);
static_assert(ClassifyActor(true, true, false) == ActorKind::Human);
static_assert(ClassifyActor(true, false, true) == ActorKind::HouseholdAltBot);
static_assert(ClassifyActor(true, false, false) == ActorKind::RandomPlayerBot);

static_assert(IsPersistentProgressionAuthority(ActorKind::Human));
static_assert(!IsPersistentProgressionAuthority(ActorKind::HouseholdAltBot));
static_assert(!IsPersistentProgressionAuthority(ActorKind::RandomPlayerBot));

static_assert(ShouldPersistGeneralEvent(ActorKind::Human));
static_assert(ShouldPersistGeneralEvent(ActorKind::HouseholdAltBot));
static_assert(!ShouldPersistGeneralEvent(ActorKind::RandomPlayerBot));
}

#endif
