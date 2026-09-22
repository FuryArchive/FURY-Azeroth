#ifndef MOD_FURY_PROOF_REPOSITORY_H
#define MOD_FURY_PROOF_REPOSITORY_H

#include "ProofTypes.h"

#include <optional>
#include <string_view>

namespace Fury
{
class ProofRepository final
{
public:
    [[nodiscard]] std::optional<ProofRecord> Find(
        HouseholdId householdId,
        std::string_view proofKey) const;

    void InsertIgnore(
        HouseholdId householdId,
        std::string_view proofKey,
        EventId sourceEventId,
        std::string_view metadataJson) const;
};
}

#endif
