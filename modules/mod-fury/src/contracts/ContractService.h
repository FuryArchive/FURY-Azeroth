#ifndef MOD_FURY_CONTRACT_SERVICE_H
#define MOD_FURY_CONTRACT_SERVICE_H

#include "ContractRepository.h"
#include "events/EventConsumer.h"

namespace Fury
{
class EventStore;

class ContractService final : public EventConsumer
{
public:
    ContractService(
        ContractRepository const& repository,
        EventStore const& events);

    [[nodiscard]] std::string_view Key() const override
    {
        return "contracts.progress.v1";
    }

    [[nodiscard]] ContractAcceptResult Accept(
        FuryEvent const& source,
        std::string_view contractKey,
        std::optional<DirectorRunId> directorRunId = std::nullopt) const;

    bool Handle(FuryEvent const& event) override;

private:
    [[nodiscard]] std::optional<EventId> EmitAcceptedEvent(
        FuryEvent const& source,
        ContractInstance const& instance) const;

    [[nodiscard]] std::optional<EventId> EmitCompletedEvent(
        FuryEvent const& source,
        ContractInstanceId instanceId,
        std::string_view contractKey) const;

    [[nodiscard]] bool FinalizeIfComplete(
        FuryEvent const& source,
        ContractInstanceId instanceId,
        std::string_view contractKey) const;

    static bool IsAuthorizedSource(FuryEvent const& source);

    ContractRepository const& _repository;
    EventStore const& _events;
};
}

#endif
