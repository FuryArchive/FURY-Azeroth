#include "ActorResolver.h"

#include "Player.h"
#include "PlayerbotAI.h"
#include "WorldSession.h"

namespace Fury
{
ActorContext ActorResolver::Resolve(
    Player* player,
    std::optional<HouseholdId> householdId) const
{
    if (!player)
        return {};

    ActorContext result;
    result.characterGuid = player->GetGUID();
    result.householdId = householdId;

    if (WorldSession* session = player->GetSession())
        result.accountId = session->GetAccountId();

    if (IsRealPlayer(player))
    {
        result.kind = ActorKind::Human;
        result.isEligibleForPersistentProgression = true;
        return result;
    }

    if (householdId)
    {
        result.kind = ActorKind::HouseholdAltBot;
        return result;
    }

    result.kind = ActorKind::RandomPlayerBot;
    return result;
}
}
