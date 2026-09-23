#ifndef MOD_FURY_DEFIAS_FIELD_RELIEF_SERVICE_H
#define MOD_FURY_DEFIAS_FIELD_RELIEF_SERVICE_H

#include "events/EventConsumer.h"

namespace Fury
{
class EventStore;
}

namespace Fury::Defias
{
class FieldReliefService final : public EventConsumer
{
public:
    explicit FieldReliefService(EventStore const& events);

    [[nodiscard]] std::string_view Key() const override
    {
        return "defias.field_relief.v1";
    }

    bool Handle(FuryEvent const& event) override;

private:
    EventStore const& _events;
};
}

#endif
