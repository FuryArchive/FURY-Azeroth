#ifndef MOD_FURY_PROOF_POLICY_H
#define MOD_FURY_PROOF_POLICY_H

#include "actors/ActorPolicy.h"

namespace Fury
{
[[nodiscard]] constexpr bool CanGrantHouseholdProof(ActorKind kind)
{
    return IsPersistentProgressionAuthority(kind);
}

static_assert(CanGrantHouseholdProof(ActorKind::Human));
static_assert(!CanGrantHouseholdProof(ActorKind::System));
static_assert(!CanGrantHouseholdProof(ActorKind::HouseholdAltBot));
static_assert(!CanGrantHouseholdProof(ActorKind::RandomPlayerBot));
}

#endif
