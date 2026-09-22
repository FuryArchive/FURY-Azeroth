#include "IndividualProgressionAdapter.h"

#include "Player.h"

#if __has_include("IndividualProgression.h")
#include "IndividualProgression.h"
#define FURY_HAS_INDIVIDUAL_PROGRESSION 1
#else
#define FURY_HAS_INDIVIDUAL_PROGRESSION 0
#endif

namespace Fury
{
IndividualProgressionAvailability
IndividualProgressionAdapter::Availability() const
{
#if FURY_HAS_INDIVIDUAL_PROGRESSION
    if (!sIndividualProgression)
        return IndividualProgressionAvailability::Unavailable;

    return sIndividualProgression->enabled
        ? IndividualProgressionAvailability::Available
        : IndividualProgressionAvailability::Disabled;
#else
    return IndividualProgressionAvailability::Unavailable;
#endif
}

IndividualProgressionGateResult
IndividualProgressionAdapter::Check(
    Player* player,
    uint8 requiredState) const
{
    if (requiredState == 0)
    {
        return {
            IndividualProgressionGateOutcome::NotRequired,
            0,
            0
        };
    }

    if (!IsKnownState(requiredState))
    {
        return {
            IndividualProgressionGateOutcome::InvalidRequiredState,
            requiredState,
            0
        };
    }

#if FURY_HAS_INDIVIDUAL_PROGRESSION
    if (!sIndividualProgression)
    {
        return {
            IndividualProgressionGateOutcome::ModuleUnavailable,
            requiredState,
            0
        };
    }

    if (!sIndividualProgression->enabled)
    {
        return {
            IndividualProgressionGateOutcome::ModuleDisabled,
            requiredState,
            0
        };
    }

    if (!player || !player->IsInWorld())
    {
        return {
            IndividualProgressionGateOutcome::PlayerUnavailable,
            requiredState,
            0
        };
    }

    uint8 const current =
        sIndividualProgression->GetPlayerProgressionFromQuests(player);

    bool const passed =
        sIndividualProgression->hasPassedProgression(
            player,
            static_cast<ProgressionState>(requiredState));

    return {
        passed
            ? IndividualProgressionGateOutcome::Allowed
            : IndividualProgressionGateOutcome::NotPassed,
        requiredState,
        current
    };
#else
    (void)player;
    return {
        IndividualProgressionGateOutcome::ModuleUnavailable,
        requiredState,
        0
    };
#endif
}
}
