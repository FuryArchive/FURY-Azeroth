#ifndef MOD_FURY_CONTRACT_POLICY_H
#define MOD_FURY_CONTRACT_POLICY_H

#include "actors/ActorPolicy.h"

namespace Fury
{
[[nodiscard]] constexpr bool CanAuthorContractProgress(ActorKind kind)
{
    return IsPersistentProgressionAuthority(kind);
}

static_assert(CanAuthorContractProgress(ActorKind::Human));
static_assert(!CanAuthorContractProgress(ActorKind::System));
static_assert(!CanAuthorContractProgress(ActorKind::HouseholdAltBot));
static_assert(!CanAuthorContractProgress(ActorKind::RandomPlayerBot));
static_assert(!CanAuthorContractProgress(ActorKind::NpcAssistant));
}

#endif
