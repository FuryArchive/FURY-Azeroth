#ifndef MOD_FURY_DIRECTOR_POLICY_H
#define MOD_FURY_DIRECTOR_POLICY_H

#include "actors/ActorPolicy.h"

namespace Fury
{
[[nodiscard]] constexpr bool CanStartDirectorRun(ActorKind kind)
{
    return IsPersistentProgressionAuthority(kind);
}

[[nodiscard]] constexpr bool CanMutateDirectorRun(ActorKind kind)
{
    return kind == ActorKind::Human ||
        kind == ActorKind::System;
}

static_assert(CanStartDirectorRun(ActorKind::Human));
static_assert(!CanStartDirectorRun(ActorKind::System));
static_assert(!CanStartDirectorRun(ActorKind::HouseholdAltBot));
static_assert(!CanStartDirectorRun(ActorKind::RandomPlayerBot));
static_assert(!CanStartDirectorRun(ActorKind::NpcAssistant));

static_assert(CanMutateDirectorRun(ActorKind::Human));
static_assert(CanMutateDirectorRun(ActorKind::System));
static_assert(!CanMutateDirectorRun(ActorKind::HouseholdAltBot));
static_assert(!CanMutateDirectorRun(ActorKind::RandomPlayerBot));
static_assert(!CanMutateDirectorRun(ActorKind::NpcAssistant));
}

#endif
