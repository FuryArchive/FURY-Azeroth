#ifndef MOD_FURY_EVENT_H
#define MOD_FURY_EVENT_H

#include "actors/ActorContext.h"

#include <optional>
#include <string>

namespace Fury
{
using EventId = uint64;

struct FuryEvent
{
    EventId id = 0;

    std::string type;
    ActorContext actor;

    uint32 mapId = 0;
    uint32 zoneId = 0;
    uint32 areaId = 0;

    std::string subjectType;
    std::optional<uint64> subjectId;

    std::string sourceSystem;
    std::string correlationKey;

    // Stable authoritative identity for deduplication. EventStore persists
    // SHA-256(identity), never a timestamp-derived key.
    std::string dedupeIdentity;

    std::string payloadJson = "{}";
};
}

#endif
