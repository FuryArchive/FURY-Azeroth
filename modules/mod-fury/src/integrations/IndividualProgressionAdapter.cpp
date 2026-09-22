#include "IndividualProgressionAdapter.h"

#include "Player.h"
#include "World.h"

#include <type_traits>
#include <utility>

#if __has_include("IndividualProgression.h")
#include "IndividualProgression.h"
#define FURY_HAS_INDIVIDUAL_PROGRESSION 1
#else
#define FURY_HAS_INDIVIDUAL_PROGRESSION 0
#endif

namespace Fury
{
#if FURY_HAS_INDIVIDUAL_PROGRESSION
namespace
{
template <typename T, typename = void>
struct HasExpectedIpReadApi : std::false_type
{
};

template <typename T>
struct HasExpectedIpReadApi<T, std::void_t<
    decltype(T::instance()),
    decltype(std::declval<T&>().enabled),
    decltype(std::declval<T const&>().GetPlayerProgressionFromQuests(
        static_cast<Player*>(nullptr)))>> : std::true_type
{
};

constexpr bool HasCompatibleIpReadApi =
    HasExpectedIpReadApi<IndividualProgression>::value;
}
#endif

IndividualProgressionAvailability
IndividualProgressionAdapter::Availability() const
{
#if !FURY_HAS_INDIVIDUAL_PROGRESSION
    return IndividualProgressionAvailability::Unavailable;
#else
    if constexpr (!HasCompatibleIpReadApi)
    {
        return IndividualProgressionAvailability::Incompatible;
    }
    else
    {
        IndividualProgression* integration =
            IndividualProgression::instance();

        if (!integration)
            return IndividualProgressionAvailability::Unavailable;

        if (!integration->enabled)
            return IndividualProgressionAvailability::Disabled;

        if (!sWorld->getBoolConfig(CONFIG_PLAYER_SETTINGS_ENABLED))
        {
            return IndividualProgressionAvailability::
                PlayerSettingsDisabled;
        }

        return IndividualProgressionAvailability::Available;
    }
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

    IndividualProgressionAvailability const availability =
        Availability();

    switch (availability)
    {
        case IndividualProgressionAvailability::Unavailable:
            return {
                IndividualProgressionGateOutcome::ModuleUnavailable,
                requiredState,
                0
            };
        case IndividualProgressionAvailability::Incompatible:
            return {
                IndividualProgressionGateOutcome::ModuleIncompatible,
                requiredState,
                0
            };
        case IndividualProgressionAvailability::Disabled:
            return {
                IndividualProgressionGateOutcome::ModuleDisabled,
                requiredState,
                0
            };
        case IndividualProgressionAvailability::PlayerSettingsDisabled:
            return {
                IndividualProgressionGateOutcome::
                    PlayerSettingsDisabled,
                requiredState,
                0
            };
        case IndividualProgressionAvailability::Available:
            break;
    }

    if (!player || !player->IsInWorld())
    {
        return {
            IndividualProgressionGateOutcome::PlayerUnavailable,
            requiredState,
            0
        };
    }

#if FURY_HAS_INDIVIDUAL_PROGRESSION
    if constexpr (HasCompatibleIpReadApi)
    {
        IndividualProgression* integration =
            IndividualProgression::instance();
        if (!integration)
        {
            return {
                IndividualProgressionGateOutcome::ModuleUnavailable,
                requiredState,
                0
            };
        }

        uint8 const current =
            integration->GetPlayerProgressionFromQuests(player);

        if (!IsKnownState(current))
        {
            return {
                IndividualProgressionGateOutcome::ModuleIncompatible,
                requiredState,
                current
            };
        }

        return {
            Passes(current, requiredState)
                ? IndividualProgressionGateOutcome::Allowed
                : IndividualProgressionGateOutcome::NotPassed,
            requiredState,
            current
        };
    }
#endif

    return {
        IndividualProgressionGateOutcome::ModuleIncompatible,
        requiredState,
        0
    };
}
}
