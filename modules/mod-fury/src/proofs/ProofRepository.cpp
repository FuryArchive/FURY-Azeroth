#include "ProofRepository.h"

#include "database/FuryDatabase.h"
#include "DatabaseEnv.h"

#include <string>

namespace Fury
{
std::optional<ProofRecord> ProofRepository::Find(
    HouseholdId householdId,
    std::string_view proofKey) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_SEL_PROOF);
    stmt->SetData(0, householdId);
    stmt->SetData(1, std::string(proofKey));

    PreparedQueryResult result = FuryDatabase.Query(stmt);
    if (!result)
        return std::nullopt;

    Field* fields = result->Fetch();

    ProofRecord proof;
    proof.householdId = householdId;
    proof.proofKey = std::string(proofKey);
    proof.sourceEventId = fields[0].Get<EventId>();
    return proof;
}

void ProofRepository::Insert(
    HouseholdId householdId,
    std::string_view proofKey,
    EventId sourceEventId,
    std::string_view metadataJson) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_INS_PROOF);

    stmt->SetData(0, householdId);
    stmt->SetData(1, std::string(proofKey));
    stmt->SetData(2, sourceEventId);

    if (!metadataJson.empty())
        stmt->SetData(3, std::string(metadataJson));
    else
        stmt->SetData(3, nullptr);

    FuryDatabase.Execute(stmt);
}
}
