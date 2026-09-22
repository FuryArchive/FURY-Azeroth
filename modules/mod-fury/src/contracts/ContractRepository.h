#ifndef MOD_FURY_CONTRACT_REPOSITORY_H
#define MOD_FURY_CONTRACT_REPOSITORY_H

#include "ContractTypes.h"

#include <optional>
#include <string_view>
#include <vector>

namespace Fury
{
class ContractRepository final
{
public:
    [[nodiscard]] std::optional<ContractDefinition> FindDefinition(
        std::string_view contractKey) const;

    [[nodiscard]] std::optional<ContractInstance> FindActiveInstance(
        HouseholdId householdId,
        std::string_view contractKey) const;

    [[nodiscard]] std::optional<ContractInstance> FindInstance(
        ContractInstanceId instanceId) const;

    [[nodiscard]] bool HasCompletedInstance(
        HouseholdId householdId,
        std::string_view contractKey) const;

    [[nodiscard]] uint32 CountObjectives(
        std::string_view contractKey) const;

    void InsertActiveInstance(
        HouseholdId householdId,
        std::string_view contractKey,
        std::optional<DirectorRunId> directorRunId,
        EventId acceptedEventId) const;

    void InitializeProgress(
        ContractInstanceId instanceId,
        std::string_view contractKey) const;

    [[nodiscard]] std::vector<ContractObjectiveMatch> FindMatchingObjectives(
        FuryEvent const& event) const;

    void AdvanceObjective(
        ContractObjectiveMatch const& objective,
        EventId eventId) const;

    [[nodiscard]] std::optional<ContractObjectiveProgress> FindProgress(
        ContractInstanceId instanceId,
        uint16 objectiveOrdinal) const;

    [[nodiscard]] std::optional<uint32> CountIncompleteObjectives(
        ContractInstanceId instanceId) const;

    void MarkComplete(
        ContractInstanceId instanceId,
        EventId completionEventId) const;
};
}

#endif
