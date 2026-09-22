#include "BestiaryRepository.h"

#include "database/FuryDatabase.h"
#include "DatabaseEnv.h"

#include <string>
#include <utility>

namespace Fury
{
std::vector<BestiaryMapping> BestiaryRepository::FindMappings(
    uint32 creatureEntry) const
{
    std::vector<BestiaryMapping> mappings;

    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_SEL_BESTIARY_MAPPINGS);
    stmt->SetData(0, creatureEntry);

    PreparedQueryResult result = FuryDatabase.Query(stmt);
    if (!result)
        return mappings;

    do
    {
        Field* fields = result->Fetch();

        BestiaryMapping mapping;
        mapping.entryKey = fields[0].Get<std::string>();
        mapping.discoveryLevel =
            static_cast<BestiaryDiscoveryLevel>(fields[1].Get<uint8>());
        mappings.push_back(std::move(mapping));
    } while (result->NextRow());

    return mappings;
}

std::optional<BestiaryState> BestiaryRepository::FindState(
    uint32 accountId,
    std::string_view entryKey) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_SEL_BESTIARY_STATE);
    stmt->SetData(0, accountId);
    stmt->SetData(1, std::string(entryKey));

    PreparedQueryResult result = FuryDatabase.Query(stmt);
    if (!result)
        return std::nullopt;

    Field* fields = result->Fetch();

    BestiaryState state;
    state.accountId = accountId;
    state.entryKey = std::string(entryKey);
    state.discoveryLevel =
        static_cast<BestiaryDiscoveryLevel>(fields[0].Get<uint8>());
    state.killCount = fields[1].Get<uint32>();
    state.firstEventId = fields[2].Get<EventId>();
    state.lastEventId = fields[3].Get<EventId>();
    state.revision = fields[4].Get<uint64>();
    return state;
}

void BestiaryRepository::ApplyKill(
    uint32 accountId,
    std::string_view entryKey,
    BestiaryDiscoveryLevel level,
    EventId eventId) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_UPSERT_BESTIARY_KILL);
    stmt->SetData(0, accountId);
    stmt->SetData(1, std::string(entryKey));
    stmt->SetData(2, level);
    stmt->SetData(3, eventId);
    stmt->SetData(4, eventId);
    FuryDatabase.Execute(stmt);
}

void BestiaryRepository::ApplyLevel(
    uint32 accountId,
    std::string_view entryKey,
    BestiaryDiscoveryLevel level,
    EventId eventId) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_UPSERT_BESTIARY_LEVEL);
    stmt->SetData(0, accountId);
    stmt->SetData(1, std::string(entryKey));
    stmt->SetData(2, level);
    stmt->SetData(3, eventId);
    stmt->SetData(4, eventId);
    FuryDatabase.Execute(stmt);
}
}
