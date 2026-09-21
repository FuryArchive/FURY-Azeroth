#ifndef MOD_FURY_APP_H
#define MOD_FURY_APP_H

#include "Define.h"
#include "actors/ActorResolver.h"
#include "household/HouseholdRepository.h"
#include "household/HouseholdService.h"

namespace Fury
{
class App final
{
public:
    static App& Instance();

    void Initialize();
    void Update(uint32 diff);
    void Shutdown();

    [[nodiscard]] bool IsEnabled() const { return _enabled; }
    [[nodiscard]] bool IsInitialized() const { return _initialized; }

    ActorResolver& Actors() { return _actors; }
    HouseholdService& Households() { return _households; }

private:
    struct TickConfig
    {
        uint32 fastMs = 250;
        uint32 serviceMs = 1000;
        uint32 directorMs = 5000;
        uint32 reconcileMs = 30000;
    };

    App();

    static bool AdvanceTimer(uint64& accumulator, uint32 diff, uint32 interval);

    void ResetTimers();
    void RunFastTick();
    void RunServiceTick();
    void RunDirectorTick();
    void RunReconcileTick();

    HouseholdRepository _householdRepository;
    HouseholdService _households;
    ActorResolver _actors;

    TickConfig _ticks;

    uint64 _fastAccumulator = 0;
    uint64 _serviceAccumulator = 0;
    uint64 _directorAccumulator = 0;
    uint64 _reconcileAccumulator = 0;

    bool _enabled = false;
    bool _initialized = false;
};
}

#endif
