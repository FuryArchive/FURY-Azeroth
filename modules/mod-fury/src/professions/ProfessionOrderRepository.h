#ifndef MOD_FURY_PROFESSION_ORDER_REPOSITORY_H
#define MOD_FURY_PROFESSION_ORDER_REPOSITORY_H

#include "ProfessionOrderTypes.h"

#include <optional>
#include <string_view>
#include <vector>

namespace Fury
{
class ProfessionOrderRepository final
{
public:
    [[nodiscard]] std::optional<ProfessionOrderDefinition> FindDefinition(
        std::string_view orderKey) const;

    [[nodiscard]] std::optional<ProfessionOrderOption> FindOption(
        std::string_view orderKey,
        uint16 optionOrdinal) const;

    [[nodiscard]] std::optional<ProfessionOrderInstance> FindActive(
        HouseholdId householdId,
        std::string_view orderKey) const;

    [[nodiscard]] std::optional<ProfessionOrderInstance> FindInstance(
        ProfessionOrderInstanceId instanceId) const;

    [[nodiscard]] bool HasCompleted(
        HouseholdId householdId,
        std::string_view orderKey) const;

    void InsertActive(
        HouseholdId householdId,
        std::string_view orderKey,
        uint16 optionOrdinal,
        EventId acceptedEventId) const;

    [[nodiscard]] std::vector<ProfessionOrderInstance> FindMatchingActive(
        HouseholdId householdId,
        uint32 skillId,
        uint32 itemId) const;

    void Advance(
        ProfessionOrderInstanceId instanceId,
        HouseholdId householdId,
        EventId eventId) const;

    void MarkComplete(
        ProfessionOrderInstanceId instanceId,
        HouseholdId householdId,
        EventId completionEventId) const;
};
}

#endif
