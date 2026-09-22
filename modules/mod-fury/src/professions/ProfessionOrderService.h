#ifndef MOD_FURY_PROFESSION_ORDER_SERVICE_H
#define MOD_FURY_PROFESSION_ORDER_SERVICE_H

#include "ProfessionOrderRepository.h"
#include "events/EventConsumer.h"

namespace Fury
{
class EventStore;

class ProfessionOrderService final : public EventConsumer
{
public:
    ProfessionOrderService(
        ProfessionOrderRepository const& repository,
        EventStore const& events);

    [[nodiscard]] std::string_view Key() const override
    {
        return "professions.orders.v1";
    }

    [[nodiscard]] ProfessionOrderStartResult Start(
        FuryEvent const& source,
        std::string_view orderKey,
        uint16 optionOrdinal) const;

    bool Handle(FuryEvent const& event) override;

private:
    [[nodiscard]] std::optional<EventId> EmitAcceptedEvent(
        FuryEvent const& source,
        ProfessionOrderInstance const& instance) const;

    [[nodiscard]] std::optional<EventId> EmitCompletedEvent(
        FuryEvent const& source,
        ProfessionOrderInstance const& instance) const;

    static bool IsAuthorizedSource(FuryEvent const& source);

    ProfessionOrderRepository const& _repository;
    EventStore const& _events;
};
}

#endif
