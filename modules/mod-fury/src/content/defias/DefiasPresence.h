#ifndef MOD_FURY_DEFIAS_PRESENCE_H
#define MOD_FURY_DEFIAS_PRESENCE_H
#include "events/FuryEvent.h"
#include <chrono>
#include <unordered_map>

namespace Fury::Defias
{
class PresenceTracker
{
public:
    using Clock = std::chrono::steady_clock;
    struct Entry { FuryEvent source; Clock::time_point since; };
    void Observe(uint64 guid, FuryEvent const& event, bool eligible, Clock::time_point now)
    {
        if (!guid) return;
        if (event.type == "player.login" || event.type == "player.logout" || !eligible)
            _entries.erase(guid);
        if (eligible && event.type != "player.logout")
            _entries.try_emplace(guid, Entry{event, now});
    }
    auto& Entries() { return _entries; }
    void Reset() { _entries.clear(); }
private:
    std::unordered_map<uint64, Entry> _entries;
};
}
#endif
