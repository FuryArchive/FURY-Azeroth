#ifndef MOD_FURY_ACTOR_RESOLVER_H
#define MOD_FURY_ACTOR_RESOLVER_H

#include "ActorContext.h"

class Player;

namespace Fury
{
class HouseholdService;

class ActorResolver final
{
public:
    explicit ActorResolver(HouseholdService const* households = nullptr);

    void SetHouseholdService(HouseholdService const* households);

    ActorContext Resolve(Player* player) const;

private:
    HouseholdService const* _households = nullptr;
};
}

#endif
