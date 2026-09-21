#ifndef MOD_FURY_EVENT_BUS_H
#define MOD_FURY_EVENT_BUS_H

#include "EventConsumer.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace Fury
{
class ConsumerCheckpointRepository;
class EventStore;

class EventBus final
{
public:
    EventBus(
        EventStore const& store,
        ConsumerCheckpointRepository const& checkpoints);

    void SetReplayBatchSize(uint32 replayBatchSize);

    bool RegisterConsumer(EventConsumer& consumer);
    void UnregisterConsumer(std::string_view consumerKey);

    void ReplayPending();

private:
    bool ReplayConsumer(EventConsumer& consumer);

    EventStore const& _store;
    ConsumerCheckpointRepository const& _checkpoints;

    uint32 _replayBatchSize = 500;
    std::unordered_map<std::string, EventConsumer*> _consumers;
};
}

#endif
