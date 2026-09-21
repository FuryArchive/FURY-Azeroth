#ifndef MOD_FURY_CONSUMER_CHECKPOINT_REPOSITORY_H
#define MOD_FURY_CONSUMER_CHECKPOINT_REPOSITORY_H

#include "FuryEvent.h"

#include <string_view>

namespace Fury
{
class ConsumerCheckpointRepository final
{
public:
    [[nodiscard]] EventId Load(std::string_view consumerKey) const;
    void Advance(std::string_view consumerKey, EventId eventId) const;
};
}

#endif
