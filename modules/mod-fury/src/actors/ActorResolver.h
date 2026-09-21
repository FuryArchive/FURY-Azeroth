#ifndef MOD_FURY_ACTOR_RESOLVER_H
#define MOD_FURY_ACTOR_RESOLVER_H

#include "ActorContext.h"

class Player;

namespace Fury
{
class ActorResolver final
{
public:
    ActorContext Resolve(
        Player* player,
        std::optional<HouseholdId> householdId = std::nullopt) const;
};
}

#endif
