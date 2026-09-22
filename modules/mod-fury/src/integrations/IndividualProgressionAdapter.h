#ifndef MOD_FURY_INDIVIDUAL_PROGRESSION_ADAPTER_H
#define MOD_FURY_INDIVIDUAL_PROGRESSION_ADAPTER_H

#include "Define.h"

class Player;

namespace Fury
{
enum class IndividualProgressionAvailability : uint8
{
    Unavailable = 1,
    Incompatible = 2,
    Disabled = 3,
    PlayerSettingsDisabled = 4,
    Available = 5
};

enum class IndividualProgressionGateOutcome : uint8
{
    Allowed = 1,
    NotRequired = 2,
    ModuleUnavailable = 3,
    ModuleIncompatible = 4,
    ModuleDisabled = 5,
    PlayerSettingsDisabled = 6,
    InvalidRequiredState = 7,
    PlayerUnavailable = 8,
    NotPassed = 9
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
        // Pinned Individual Progression values are 0..10 and 12..18.
        return state <= 18 && state != 11;
    }

    [[nodiscard]] static constexpr bool Passes(
        uint8 currentState,
        uint8 requiredState)
    {
        return IsKnownState(currentState) &&
            IsKnownState(requiredState) &&
            currentState >= requiredState;
    }
};

static_assert(IndividualProgressionAdapter::IsKnownState(0));
static_assert(IndividualProgressionAdapter::IsKnownState(10));
static_assert(!IndividualProgressionAdapter::IsKnownState(11));
static_assert(IndividualProgressionAdapter::IsKnownState(18));
static_assert(!IndividualProgressionAdapter::IsKnownState(19));
static_assert(IndividualProgressionAdapter::Passes(12, 10));
static_assert(!IndividualProgressionAdapter::Passes(3, 4));
}

#endif
