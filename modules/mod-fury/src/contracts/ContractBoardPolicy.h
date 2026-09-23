#ifndef MOD_FURY_CONTRACT_BOARD_POLICY_H
#define MOD_FURY_CONTRACT_BOARD_POLICY_H

#include "ContractTypes.h"

namespace Fury
{
[[nodiscard]] inline bool ContractMatchesBoardContext(
    ContractDefinition const& definition,
    ContractBoardContext const& context)
{
    if (!definition.enabled || !context.householdId ||
        !context.allowNewContracts)
    {
        return false;
    }

    if (definition.campaignNodeKey &&
        (!context.campaignNodeKey ||
         *context.campaignNodeKey != *definition.campaignNodeKey))
    {
        return false;
    }

    if (definition.directorPhase &&
        (!context.directorPhase ||
         *context.directorPhase != *definition.directorPhase))
    {
        return false;
    }

    if (definition.repeatPolicy == ContractRepeatPolicy::DirectorRun &&
        !context.directorRunId)
    {
        return false;
    }

    return true;
}

[[nodiscard]] inline bool ShouldShowContractOnBoard(
    ContractDefinition const& definition,
    ContractBoardContext const& context,
    bool hasActiveInstance,
    bool hasCompletedInstance)
{
    return hasActiveInstance || hasCompletedInstance ||
        ContractMatchesBoardContext(definition, context);
}
}

#endif
