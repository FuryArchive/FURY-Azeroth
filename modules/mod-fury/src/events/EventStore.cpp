#include "EventStore.h"

#include "database/FuryDatabase.h"
#include "DatabaseEnv.h"
#include "Log.h"

#include <openssl/sha.h>

namespace Fury
{
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
