#ifndef MOD_FURY_PROOF_SERVICE_H
#define MOD_FURY_PROOF_SERVICE_H

#include "ProofRepository.h"

namespace Fury
{
class EventStore;

class ProofService final
{
public:
    ProofService(
        ProofRepository const& repository,
        EventStore const& events);

    [[nodiscard]] bool Has(
        HouseholdId householdId,
        std::string_view proofKey) const;

    [[nodiscard]] ProofGrantResult Grant(
        FuryEvent const& source,
        std::string_view proofKey,
        std::string_view metadataJson = "{}") const;

private:
    void EmitGrantedEvent(
        FuryEvent const& source,
        std::string_view proofKey,
        EventId originalSourceEventId,
        std::string_view metadataJson) const;

    ProofRepository const& _repository;
    EventStore const& _events;
};
}

#endif
