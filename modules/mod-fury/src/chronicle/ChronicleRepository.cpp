#include "ChronicleRepository.h"

#include "database/FuryDatabase.h"
#include "DatabaseEnv.h"

#include <string>
#include <utility>

namespace Fury
{
bool ChronicleRepository::Insert(
    HouseholdId householdId,
    EventId sourceEventId,
    std::string_view entryKey,
    std::string_view category,
    std::string_view title,
    std::string_view body,
    std::string_view metadataJson) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_INS_CHRONICLE_ENTRY);

    uint8 index = 0;
    stmt->SetData(index++, householdId);
    stmt->SetData(index++, std::string(entryKey));
    stmt->SetData(index++, std::string(category));
    stmt->SetData(index++, std::string(title));

    if (!body.empty())
        stmt->SetData(index++, std::string(body));
    else
        stmt->SetData(index++, nullptr);

    if (!metadataJson.empty())
        stmt->SetData(index++, std::string(metadataJson));
    else
        stmt->SetData(index++, nullptr);

    stmt->SetData(index++, sourceEventId);
    FuryDatabase.Execute(stmt);

    DatabasePreparedStatement* verify =
        FuryDatabase.GetPreparedStatement(FURY_SEL_CHRONICLE_ENTRY_BY_SOURCE);
    verify->SetData(0, householdId);
    verify->SetData(1, std::string(entryKey));
    verify->SetData(2, sourceEventId);

    return static_cast<bool>(FuryDatabase.Query(verify));
}

std::vector<ChronicleEntry> ChronicleRepository::Timeline(
    HouseholdId householdId,
    uint32 limit) const
{
    std::vector<ChronicleEntry> entries;
    if (!limit)
        return entries;

    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_SEL_CHRONICLE_TIMELINE);
    stmt->SetData(0, householdId);
    stmt->SetData(1, limit);

    PreparedQueryResult result = FuryDatabase.Query(stmt);
    if (!result)
        return entries;

    do
    {
        Field* fields = result->Fetch();

        ChronicleEntry entry;
        entry.id = fields[0].Get<uint64>();
        entry.entryKey = fields[1].Get<std::string>();
        entry.category = fields[2].Get<std::string>();
        entry.title = fields[3].Get<std::string>();

        if (!fields[4].IsNull())
            entry.body = fields[4].Get<std::string>();

        entry.sourceEventId = fields[5].Get<EventId>();
        entry.occurredAt = fields[6].Get<std::string>();

        if (!fields[7].IsNull())
            entry.metadataJson = fields[7].Get<std::string>();

        entries.push_back(std::move(entry));
    } while (result->NextRow());

    return entries;
}
}
