#include "ActorResolver.h"

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

    if (IsRealPlayer(player))
    {
        result.kind = ActorKind::Human;
        result.isEligibleForPersistentProgression = true;

        if (_households && result.accountId)
            result.householdId = _households->FindByAccount(result.accountId);

        return result;
    }

    if (_households && result.accountId)
        result.householdId = _households->FindByAccount(result.accountId);

    if (result.householdId)
    {
        result.kind = ActorKind::HouseholdAltBot;
        return result;
    }

    result.kind = ActorKind::RandomPlayerBot;
    return result;
}
}
