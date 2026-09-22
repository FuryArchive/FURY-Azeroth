#ifndef MOD_FURY_PROFESSION_ORDER_POLICY_H
#define MOD_FURY_PROFESSION_ORDER_POLICY_H

#include "actors/ActorPolicy.h"

namespace Fury
{
[[nodiscard]] constexpr bool CanAuthorProfessionOrderProgress(ActorKind kind)
{
    return IsPersistentProgressionAuthority(kind);
}

static_assert(CanAuthorProfessionOrderProgress(ActorKind::Human));
static_assert(!CanAuthorProfessionOrderProgress(ActorKind::System));
static_assert(!CanAuthorProfessionOrderProgress(ActorKind::HouseholdAltBot));
static_assert(!CanAuthorProfessionOrderProgress(ActorKind::RandomPlayerBot));
static_assert(!CanAuthorProfessionOrderProgress(ActorKind::NpcAssistant));
}

#endif
