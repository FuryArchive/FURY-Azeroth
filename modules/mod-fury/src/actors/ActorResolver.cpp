#include "ActorResolver.h"
#include "ActorPolicy.h"

#include "household/HouseholdService.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "WorldSession.h"

namespace Fury
{
ActorResolver::ActorResolver(HouseholdService const* households)
    : _households(households)
{
}

void ActorResolver::SetHouseholdService(HouseholdService const* households)
{
    _households = households;
}

ActorContext ActorResolver::Resolve(Player* player) const
{
    if (!player)
        return {};

    ActorContext result;
    result.characterGuid = player->GetGUID();

    if (WorldSession* session = player->GetSession())
        result.accountId = session->GetAccountId();

    bool const isReal = IsRealPlayer(player);

    if (_households && result.accountId)
        result.householdId = _households->FindByAccount(result.accountId);

    result.kind = ClassifyActor(
        true,
        isReal,
        result.householdId.has_value());
    result.isEligibleForPersistentProgression =
        IsPersistentProgressionAuthority(result.kind);

    return result;
}
}
