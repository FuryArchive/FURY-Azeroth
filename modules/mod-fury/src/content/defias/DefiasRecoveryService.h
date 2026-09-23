#ifndef MOD_FURY_DEFIAS_RECOVERY_SERVICE_H
#define MOD_FURY_DEFIAS_RECOVERY_SERVICE_H

#include "director/DirectorTypes.h"

#include <optional>
#include <string_view>

namespace Fury
{
class DirectorRepository;
class DirectorService;
class EventStore;
class LivingWorldAdapter;
}

namespace Fury::Defias
{
class RecoveryService final
{
public:
    RecoveryService(
        DirectorService& director,
        DirectorRepository const& repository,
        LivingWorldAdapter& livingWorld,
        EventStore const& events);

    [[nodiscard]] bool Reconcile() const;

private:
    [[nodiscard]] std::optional<FuryEvent> EmitRecoveryEvent(
        DirectorRun const& run,
        std::string_view action,
        uint64 runtimeId) const;

    [[nodiscard]] bool HandleRun(
        DirectorRun const& run) const;

    [[nodiscard]] bool HandleOrphanRuntime(
        uint64 runtimeId) const;

    DirectorService& _director;
    DirectorRepository const& _repository;
    LivingWorldAdapter& _livingWorld;
    EventStore const& _events;
};
}

#endif
