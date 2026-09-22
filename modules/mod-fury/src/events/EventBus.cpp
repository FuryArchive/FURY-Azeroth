#include "EventBus.h"

#include "ConsumerCheckpointRepository.h"
#include "EventStore.h"
#include "Log.h"

#include <algorithm>

namespace Fury
{
EventBus::EventBus(
    EventStore const& store,
    ConsumerCheckpointRepository const& checkpoints)
    : _store(store),
      _checkpoints(checkpoints)
{
}

void EventBus::SetReplayBatchSize(uint32 replayBatchSize)
{
    _replayBatchSize = std::max<uint32>(1, replayBatchSize);
}

bool EventBus::RegisterConsumer(EventConsumer& consumer)
{
    std::string key(consumer.Key());
    if (key.empty())
        return false;

    return _consumers.emplace(std::move(key), &consumer).second;
}

void EventBus::UnregisterConsumer(std::string_view consumerKey)
{
    _consumers.erase(std::string(consumerKey));
}

void EventBus::ReplayPending()
{
    for (auto const& [key, consumer] : _consumers)
    {
        if (!consumer)
            continue;

        if (!ReplayConsumer(*consumer))
        {
            LOG_WARN(
                "server.loading",
                "[FURY] event consumer '{}' stopped before advancing its checkpoint.",
                key);
        }
    }
}

bool EventBus::ReplayConsumer(EventConsumer& consumer)
{
    EventId const checkpoint = _checkpoints.Load(consumer.Key());
    std::vector<FuryEvent> events = _store.ReadAfter(checkpoint, _replayBatchSize);

    EventId lastHandled = checkpoint;

    for (FuryEvent const& event : events)
    {
        if (!consumer.Handle(event))
            return false;

        lastHandled = event.id;
    }

    // Checkpoint once per successfully processed batch. Consumers are required
    // to be idempotent, so a crash before this write may replay part of the
    // batch but can never skip durable events. This keeps write amplification
    // bounded for long histories.
    if (lastHandled != checkpoint)
        _checkpoints.Advance(consumer.Key(), lastHandled);

    return true;
}
}
