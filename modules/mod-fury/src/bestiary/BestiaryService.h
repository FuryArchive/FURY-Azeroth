#ifndef MOD_FURY_BESTIARY_SERVICE_H
#define MOD_FURY_BESTIARY_SERVICE_H

#include "BestiaryRepository.h"
#include "events/EventConsumer.h"

namespace Fury
{
class DirectorRepository;
class EventStore;

class BestiaryService final : public EventConsumer
{
public:
    BestiaryService(
        BestiaryRepository const& repository,
        EventStore const& events,
        DirectorRepository const& director);

    [[nodiscard]] std::string_view Key() const override
    {
        return "bestiary.projection.v1";
    }

    bool Handle(FuryEvent const& event) override;

    [[nodiscard]] BestiaryAdvanceResult Promote(
        FuryEvent const& source,
        std::string_view entryKey,
        BestiaryDiscoveryLevel level) const;

    [[nodiscard]] std::optional<BestiaryState> FindState(
        uint32 accountId,
        std::string_view entryKey) const
    {
        return _repository.FindState(accountId, entryKey);
    }

private:
    [[nodiscard]] bool ApplyMappedKill(
        FuryEvent const& source,
        BestiaryMapping const& mapping) const;

    [[nodiscard]] bool ApplyDefiasSuccessMastery(
        FuryEvent const& source) const;

    [[nodiscard]] bool EmitAdvancedEvent(
        FuryEvent const& source,
        BestiaryState const& state) const;

    static bool IsAuthorizedSource(FuryEvent const& source);

    BestiaryRepository const& _repository;
    EventStore const& _events;
    DirectorRepository const& _director;
};
}

#endif
