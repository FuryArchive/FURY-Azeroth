#ifndef MOD_FURY_PROOF_TYPES_H
#define MOD_FURY_PROOF_TYPES_H

#include "events/FuryEvent.h"

#include <string>

namespace Fury
{
struct ProofRecord
{
    HouseholdId householdId = 0;
    std::string proofKey;
    EventId sourceEventId = 0;
    std::string metadataJson = "{}";
};

enum class ProofGrantOutcome : uint8
{
    Granted = 1,
    AlreadyGranted = 2,
    InvalidSource = 3,
    InvalidProofKey = 4,
    PersistenceFailed = 5
};

struct ProofGrantResult
{
    ProofGrantOutcome outcome = ProofGrantOutcome::PersistenceFailed;
    EventId sourceEventId = 0;

    [[nodiscard]] bool Accepted() const
    {
        return outcome == ProofGrantOutcome::Granted ||
            outcome == ProofGrantOutcome::AlreadyGranted;
    }
};
}

#endif
