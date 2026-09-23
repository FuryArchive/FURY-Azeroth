#ifndef MOD_FURY_DEFIAS_PARTICIPATION_POLICY_H
#define MOD_FURY_DEFIAS_PARTICIPATION_POLICY_H

#include "actors/ActorContext.h"

namespace Fury::Defias
{
struct ParticipationRules
{
    uint32 windowSeconds = 20;
    float radiusYards = 60.0f;
    bool shareGroup = true;
};

enum class ParticipationCreditKind : uint8
{
    None = 0,
    GroupShare = 1,
    Direct = 2
};

struct ParticipationCandidate
{
    ActorKind actorKind = ActorKind::System;
    uint32 secondsSinceAnchorAction = 0;
    float distanceYards = 0.0f;
    bool direct = false;
    bool linkedToDirectGroup = false;
};

[[nodiscard]] inline ParticipationCreditKind ResolveParticipationCredit(
    ParticipationCandidate const& candidate,
    ParticipationRules const& rules)
{
    if (candidate.actorKind != ActorKind::Human ||
        candidate.secondsSinceAnchorAction > rules.windowSeconds ||
        candidate.distanceYards > rules.radiusYards)
    {
        return ParticipationCreditKind::None;
    }

    if (candidate.direct)
        return ParticipationCreditKind::Direct;

    if (rules.shareGroup && candidate.linkedToDirectGroup)
        return ParticipationCreditKind::GroupShare;

    return ParticipationCreditKind::None;
}

[[nodiscard]] constexpr ParticipationCreditKind BetterCredit(
    ParticipationCreditKind current,
    ParticipationCreditKind candidate)
{
    return static_cast<uint8>(candidate) > static_cast<uint8>(current)
        ? candidate
        : current;
}
}

#endif
