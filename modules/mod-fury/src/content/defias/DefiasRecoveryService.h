#ifndef MOD_FURY_DEFIAS_RECOVERY_SERVICE_H
#define MOD_FURY_DEFIAS_RECOVERY_SERVICE_H

#include "director/DirectorReconciliation.h"
#include "events/EventConsumer.h"

#include <optional>
#include <string_view>

namespace Fury
{
class DirectorService;
class EventStore;
class LivingWorldAdapter;
}

namespace Fury::Defias
{
class RecoveryService final : public EventConsumer
{
public:
    RecoveryService(
        DirectorReconciliationService& reconciliation,
        DirectorService& director,
        LivingWorldAdapter& livingWorld,
        EventStore const& events);

    [[nodiscard]] std::string_view Key() const override
    {
        return "defias.recovery.v1";
    }

    // Periodic scan only records durable recovery intent. Mutations happen
    // through Handle(), so a crash before the consumer checkpoint replays the
    // same intent and repairs any partially persisted action.
    void Tick();

    bool Handle(FuryEvent const& event) override;

private:
    [[nodiscard]] bool QueueDecision(
        DirectorReconcileDecision const& decision) const;

    [[nodiscard]] bool QueueRunAction(
        DirectorRun const& run,
        std::string_view eventType,
        uint64 runtimeId,
        DirectorReconcileAction action) const;

    [[nodiscard]] bool QueueOrphanRuntime(
        uint64 runtimeId) const;

    [[nodiscard]] bool HandleRunAction(
        FuryEvent const& event,
        std::string_view outcomeKey,
        bool failExternalRuntime) const;

    [[nodiscard]] static std::optional<uint64> RuntimeId(
        FuryEvent const& event);

    DirectorReconciliationService& _reconciliation;
    DirectorService& _director;
    LivingWorldAdapter& _livingWorld;
    EventStore const& _events;
};
}

#endif
