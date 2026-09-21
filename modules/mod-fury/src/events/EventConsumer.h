#ifndef MOD_FURY_EVENT_CONSUMER_H
#define MOD_FURY_EVENT_CONSUMER_H

#include "FuryEvent.h"

#include <string_view>

namespace Fury
{
class EventConsumer
{
public:
    virtual ~EventConsumer() = default;

    [[nodiscard]] virtual std::string_view Key() const = 0;

    // Return true only after the event's domain mutation is safely complete.
    // Returning false keeps the checkpoint behind this event for a later retry.
    virtual bool Handle(FuryEvent const& event) = 0;
};
}

#endif
