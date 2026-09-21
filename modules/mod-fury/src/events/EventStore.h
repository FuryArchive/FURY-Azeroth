#ifndef MOD_FURY_EVENT_STORE_H
#define MOD_FURY_EVENT_STORE_H

#include "FuryEvent.h"

#include <array>
#include <optional>
#include <string_view>
#include <vector>

namespace Fury
{
class EventStore final
{
public:
    [[nodiscard]] std::optional<EventId> Append(FuryEvent const& event) const;
    [[nodiscard]] std::vector<FuryEvent> ReadAfter(
        EventId checkpoint,
        uint32 limit) const;

private:
    static std::array<uint8, 32> HashIdentity(std::string_view identity);
    [[nodiscard]] std::optional<EventId> FindByDedupe(
        std::array<uint8, 32> const& dedupeKey) const;
};
}

#endif
