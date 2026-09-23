#include "content/defias/DefiasParticipationPolicy.h"

#include <cstdlib>
#include <iostream>

namespace
{
void Require(bool condition, char const* label)
{
    if (!condition)
    {
        std::cerr << "[FURY][FAIL] " << label << '\n';
        std::exit(1);
    }

    std::cout << "[FURY][PASS] " << label << '\n';
}
}

int main()
{
    using namespace Fury;
    using namespace Fury::Defias;

    ParticipationRules rules;
    rules.windowSeconds = 20;
    rules.radiusYards = 60.0f;
    rules.shareGroup = true;

    ParticipationCandidate direct;
    direct.actorKind = ActorKind::Human;
    direct.secondsSinceAnchorAction = 5;
    direct.distanceYards = 25.0f;
    direct.direct = true;

    Require(
        ResolveParticipationCredit(direct, rules) ==
            ParticipationCreditKind::Direct,
        "direct human damage inside time/radius earns direct credit");

    // A pet/guardian is normalized to its Human owner before policy input.
    ParticipationCandidate petOwner = direct;
    Require(
        ResolveParticipationCredit(petOwner, rules) ==
            ParticipationCreditKind::Direct,
        "human pet owner receives direct participation credit");

    ParticipationCandidate bot = direct;
    bot.actorKind = ActorKind::RandomPlayerBot;
    Require(
        ResolveParticipationCredit(bot, rules) ==
            ParticipationCreditKind::None,
        "bot-only damage never earns persistent participation credit");

    ParticipationCandidate nearbyRandom;
    nearbyRandom.actorKind = ActorKind::Human;
    nearbyRandom.secondsSinceAnchorAction = 5;
    nearbyRandom.distanceYards = 20.0f;
    Require(
        ResolveParticipationCredit(nearbyRandom, rules) ==
            ParticipationCreditKind::None,
        "nearby non-participant is not credited");

    ParticipationCandidate grouped = nearbyRandom;
    grouped.linkedToDirectGroup = true;
    Require(
        ResolveParticipationCredit(grouped, rules) ==
            ParticipationCreditKind::GroupShare,
        "nearby same-group human can share a recent direct contribution");

    ParticipationRules noGroupShare = rules;
    noGroupShare.shareGroup = false;
    Require(
        ResolveParticipationCredit(grouped, noGroupShare) ==
            ParticipationCreditKind::None,
        "group sharing can be disabled");

    ParticipationCandidate stale = direct;
    stale.secondsSinceAnchorAction = 21;
    Require(
        ResolveParticipationCredit(stale, rules) ==
            ParticipationCreditKind::None,
        "stale direct damage falls outside the participation window");

    ParticipationCandidate far = direct;
    far.distanceYards = 61.0f;
    Require(
        ResolveParticipationCredit(far, rules) ==
            ParticipationCreditKind::None,
        "participant outside configured radius is not credited");

    Require(
        BetterCredit(
            ParticipationCreditKind::GroupShare,
            ParticipationCreditKind::Direct) ==
            ParticipationCreditKind::Direct,
        "direct credit outranks inherited group credit");

    std::cout << "[FURY][PASS] T28 participation policy gate passed\n";
    return 0;
}
