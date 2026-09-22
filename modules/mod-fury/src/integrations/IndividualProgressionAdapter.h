#ifndef MOD_FURY_INDIVIDUAL_PROGRESSION_ADAPTER_H
#define MOD_FURY_INDIVIDUAL_PROGRESSION_ADAPTER_H

#include "Define.h"

class Player;

namespace Fury
{
enum class IndividualProgressionAvailability : uint8
{
    Unavailable = 1,
    Disabled = 2,
    Available = 3
};

enum class IndividualProgressionGateOutcome : uint8
{
    Allowed = 1,
    NotRequired = 2,
    ModuleUnavailable = 3,
    ModuleDisabled = 4,
    InvalidRequiredState = 5,
    PlayerUnavailable = 6,
    NotPassed = 7
};

struct IndividualProgressionGateResult
{
    IndividualProgressionGateOutcome outcome =
        IndividualProgressionGateOutcome::ModuleUnavailable;
    uint8 requiredState = 0;
    uint8 currentState = 0;

    [[nodiscard]] bool Allowed() const
    {
        return outcome == IndividualProgressionGateOutcome::Allowed ||
            outcome == IndividualProgressionGateOutcome::NotRequired;
    }
};

class IndividualProgressionAdapter final
{
public:
    [[nodiscard]] IndividualProgressionAvailability Availability() const;

    [[nodiscard]] IndividualProgressionGateResult Check(
        Player* player,
        uint8 requiredState) const;

    [[nodiscard]] static constexpr bool IsKnownState(uint8 state)
    {
        if (state == 0)
            return true;

        // Pinned IP progression values are 1..10, 12..18.
        return state <= 18 && state != 11;
    }
};

static_assert(IndividualProgressionAdapter::IsKnownState(0));
static_assert(IndividualProgressionAdapter::IsKnownState(1));
static_assert(IndividualProgressionAdapter::IsKnownState(10));
static_assert(!IndividualProgressionAdapter::IsKnownState(11));
static_assert(IndividualProgressionAdapter::IsKnownState(18));
static_assert(!IndividualProgressionAdapter::IsKnownState(19));
}

#endif
