#ifndef MOD_FURY_CHRONICLE_ENTRY_H
#define MOD_FURY_CHRONICLE_ENTRY_H

#include "events/FuryEvent.h"

#include <string>

namespace Fury
{
struct ChronicleEntry
{
    uint64 id = 0;
    std::string entryKey;
    std::string category;
    std::string title;
    std::string body;
    EventId sourceEventId = 0;
    std::string occurredAt;
    std::string metadataJson = "{}";
};
}

#endif
