#ifndef MOD_FURY_BESTIARY_POLICY_H
#define MOD_FURY_BESTIARY_POLICY_H

#include "actors/ActorPolicy.h"

namespace Fury
{
[[nodiscard]] constexpr bool CanAuthorBestiaryProgress(ActorKind kind)
{
    return IsPersistentProgressionAuthority(kind);
}

static_assert(CanAuthorBestiaryProgress(ActorKind::Human));
static_assert(!CanAuthorBestiaryProgress(ActorKind::System));
static_assert(!CanAuthorBestiaryProgress(ActorKind::HouseholdAltBot));
static_assert(!CanAuthorBestiaryProgress(ActorKind::RandomPlayerBot));
static_assert(!CanAuthorBestiaryProgress(ActorKind::NpcAssistant));
}

#endif
