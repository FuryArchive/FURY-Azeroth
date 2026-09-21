#include "ConsumerCheckpointRepository.h"

#include "database/FuryDatabase.h"
#include "DatabaseEnv.h"

#include <string>

namespace Fury
{
EventId ConsumerCheckpointRepository::Load(std::string_view consumerKey) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_SEL_CONSUMER_CHECKPOINT);
    stmt->SetData(0, std::string(consumerKey));

    PreparedQueryResult result = FuryDatabase.Query(stmt);
    if (!result)
        return 0;

    Field* fields = result->Fetch();
    return fields[0].Get<EventId>();
}

void ConsumerCheckpointRepository::Advance(
    std::string_view consumerKey,
    EventId eventId) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_UPSERT_CONSUMER_CHECKPOINT);
    stmt->SetData(0, std::string(consumerKey));
    stmt->SetData(1, eventId);
    FuryDatabase.Execute(stmt);
}
}
