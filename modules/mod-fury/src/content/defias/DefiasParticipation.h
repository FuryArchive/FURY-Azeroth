#ifndef MOD_FURY_DEFIAS_PARTICIPATION_H
#define MOD_FURY_DEFIAS_PARTICIPATION_H

#include "DefiasParticipationPolicy.h"
#include "core/FuryIds.h"

#include "ObjectGuid.h"

#include <chrono>
#include <unordered_map>

class Creature;
class Player;
class Unit;

namespace Fury
{
class ActorResolver;
class EventStore;
class LivingWorldAdapter;
struct LivingWorldEntityMetadata;
}

namespace Fury::Defias
{
class ParticipationService final
{
public:
    ParticipationService(
        LivingWorldAdapter& livingWorld,
        ActorResolver& actors,
        EventStore const& events);

    void Initialize();
    void Reset();

    void ObserveDamage(Unit* attacker, Unit* victim, uint32 damage);
    void ObserveDeath(Unit* victim, Unit* killer);

    [[nodiscard]] ParticipationRules const& Rules() const { return _rules; }
    [[nodiscard]] std::size_t EncounterCount() const { return _encounters.size(); }

private:
    struct Observation
    {
        ObjectGuid playerGuid;
        HouseholdId householdId = 0;
        ObjectGuid groupGuid;
        std::chrono::steady_clock::time_point lastAction;
    };

    struct Encounter
    {
        uint64 runtimeId = 0;
        uint64 runtimeGroupId = 0;
        uint32 spawnGroupId = 0;
        std::unordered_map<uint64, Observation> directParticipants;
    };

    struct HouseholdCredit
    {
        Player* player = nullptr;
        ParticipationCreditKind kind = ParticipationCreditKind::None;
        uint32 secondsSinceAnchorAction = 0;
    };

    [[nodiscard]] bool ResolveRuntimeTarget(
        Creature* creature,
        LivingWorldEntityMetadata& metadata) const;

    void AddCredit(
        std::unordered_map<HouseholdId, HouseholdCredit>& credits,
        Player* player,
        ParticipationCreditKind kind,
        uint32 secondsSinceAnchorAction) const;

    void Prune(std::chrono::steady_clock::time_point now);

    LivingWorldAdapter& _livingWorld;
    ActorResolver& _actors;
    EventStore const& _events;
    ParticipationRules _rules;
    std::unordered_map<uint64, Encounter> _encounters;
};
}

#endif
