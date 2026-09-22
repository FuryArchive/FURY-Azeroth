#include "EventStore.h"

#include "database/FuryDatabase.h"
#include "DatabaseEnv.h"
#include "Log.h"

#include <openssl/sha.h>

#include <utility>

namespace Fury
{
namespace
{
FuryEvent ReadEventRow(Field* fields)
{
    FuryEvent event;
    event.id = fields[0].Get<EventId>();
    event.type = fields[1].Get<std::string>();
    event.actor.kind = static_cast<ActorKind>(fields[2].Get<uint8>());

    if (!fields[3].IsNull())
    {
        ObjectGuid guid;
        guid.Set(fields[3].Get<uint64>());
        event.actor.characterGuid = guid;
    }

    if (!fields[4].IsNull())
        event.actor.accountId = fields[4].Get<uint32>();

    if (!fields[5].IsNull())
        event.actor.householdId = fields[5].Get<HouseholdId>();

    event.actor.isEligibleForPersistentProgression =
        event.actor.kind == ActorKind::Human;

    event.mapId = fields[6].Get<uint32>();
    event.zoneId = fields[7].Get<uint32>();
    event.areaId = fields[8].Get<uint32>();

    if (!fields[9].IsNull())
        event.subjectType = fields[9].Get<std::string>();

    if (!fields[10].IsNull())
        event.subjectId = fields[10].Get<uint64>();

    event.sourceSystem = fields[11].Get<std::string>();

    if (!fields[12].IsNull())
        event.correlationKey = fields[12].Get<std::string>();

    event.payloadJson = fields[13].IsNull()
        ? "{}"
        : fields[13].Get<std::string>();

    return event;
}

std::vector<FuryEvent> ReadEventRows(PreparedQueryResult const& result)
{
    std::vector<FuryEvent> events;
    if (!result)
        return events;

    do
    {
        events.push_back(ReadEventRow(result->Fetch()));
    } while (result->NextRow());

    return events;
}
}

std::optional<EventId> EventStore::Append(FuryEvent const& event) const
{
    if (event.type.empty() || event.sourceSystem.empty() || event.dedupeIdentity.empty())
    {
        LOG_ERROR(
            "server.loading",
            "[FURY] refused event append with missing type/source/dedupe identity.");
        return std::nullopt;
    }

    std::array<uint8, 32> const dedupeKey = HashIdentity(event.dedupeIdentity);

    if (std::optional<EventId> existing = FindByDedupe(dedupeKey))
        return existing;

    DatabasePreparedStatement* stmt = FuryDatabase.GetPreparedStatement(FURY_INS_EVENT);
    uint8 index = 0;

    stmt->SetData(index++, event.type);
    stmt->SetData(index++, event.actor.kind);

    if (!event.actor.characterGuid.IsEmpty())
        stmt->SetData(index++, event.actor.characterGuid.GetRawValue());
    else
        stmt->SetData(index++, nullptr);

    if (event.actor.accountId)
        stmt->SetData(index++, event.actor.accountId);
    else
        stmt->SetData(index++, nullptr);

    if (event.actor.householdId)
        stmt->SetData(index++, *event.actor.householdId);
    else
        stmt->SetData(index++, nullptr);

    stmt->SetData(index++, event.mapId);
    stmt->SetData(index++, event.zoneId);
    stmt->SetData(index++, event.areaId);

    if (!event.subjectType.empty())
        stmt->SetData(index++, event.subjectType);
    else
        stmt->SetData(index++, nullptr);

    if (event.subjectId)
        stmt->SetData(index++, *event.subjectId);
    else
        stmt->SetData(index++, nullptr);

    stmt->SetData(index++, event.sourceSystem);

    if (!event.correlationKey.empty())
        stmt->SetData(index++, event.correlationKey);
    else
        stmt->SetData(index++, nullptr);

    stmt->SetData(index++, dedupeKey);

    if (!event.payloadJson.empty())
        stmt->SetData(index++, event.payloadJson);
    else
        stmt->SetData(index++, nullptr);

    FuryDatabase.Execute(stmt);

    std::optional<EventId> persisted = FindByDedupe(dedupeKey);
    if (!persisted)
    {
        LOG_ERROR(
            "server.loading",
            "[FURY] event append did not produce a durable row (type={}, source={}).",
            event.type,
            event.sourceSystem);
    }

    return persisted;
}

std::vector<FuryEvent> EventStore::ReadAfter(EventId checkpoint, uint32 limit) const
{
    if (!limit)
        return {};

    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_SEL_EVENTS_AFTER_ID);
    stmt->SetData(0, checkpoint);
    stmt->SetData(1, limit);

    return ReadEventRows(FuryDatabase.Query(stmt));
}

std::vector<FuryEvent> EventStore::Tail(uint32 limit) const
{
    if (!limit)
        return {};

    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_SEL_EVENT_TAIL);
    stmt->SetData(0, limit);

    return ReadEventRows(FuryDatabase.Query(stmt));
}

std::array<uint8, 32> EventStore::HashIdentity(std::string_view identity)
{
    std::array<uint8, 32> digest{};
    SHA256(
        reinterpret_cast<unsigned char const*>(identity.data()),
        identity.size(),
        digest.data());
    return digest;
}

std::optional<EventId> EventStore::FindByDedupe(
    std::array<uint8, 32> const& dedupeKey) const
{
    DatabasePreparedStatement* stmt =
        FuryDatabase.GetPreparedStatement(FURY_SEL_EVENT_ID_BY_DEDUPE);
    stmt->SetData(0, dedupeKey);

    PreparedQueryResult result = FuryDatabase.Query(stmt);
    if (!result)
        return std::nullopt;

    Field* fields = result->Fetch();
    return fields[0].Get<EventId>();
}
}
