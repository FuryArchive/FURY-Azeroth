#include "DiagnosticsRepository.h"

#include "database/FuryDatabase.h"
#include "DatabaseEnv.h"

namespace Fury
{
std::optional<KernelSnapshot> DiagnosticsRepository::Snapshot() const
{
    PreparedQueryResult result =
        FuryDatabase.Query(FuryDatabase.GetPreparedStatement(FURY_SEL_DIAGNOSTIC_COUNTS));
    if (!result)
        return std::nullopt;

    Field* fields = result->Fetch();

    KernelSnapshot snapshot;
    snapshot.households = fields[0].Get<uint64>();
    snapshot.householdMembers = fields[1].Get<uint64>();
    snapshot.events = fields[2].Get<uint64>();
    snapshot.consumers = fields[3].Get<uint64>();
    snapshot.rewardClaims = fields[4].Get<uint64>();
    snapshot.chronicleEntries = fields[5].Get<uint64>();
    snapshot.overfullHouseholds = fields[6].Get<uint64>();
    return snapshot;
}
}
