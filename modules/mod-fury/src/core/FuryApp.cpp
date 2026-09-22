#include "FuryApp.h"

#include "Config.h"
#include "Log.h"

namespace
{
uint32 ReadTickInterval(char const* key, uint32 defaultValue)
{
    uint32 value = sConfigMgr->GetOption<uint32>(key, defaultValue);
    if (value != 0)
        return value;

    LOG_WARN("server.loading", "[FURY] {} cannot be 0; using {} ms.", key, defaultValue);
    return defaultValue;
}
}

namespace Fury
{
App::App()
    : _households(_householdRepository),
      _actors(&_households),
      _eventBus(_events, _consumerCheckpoints),
      _rewardPolicy(_rewardRepository),
      _rewards(_rewardRepository, _rewardPolicy),
      _chronicle(_chronicleRepository),
      _diagnostics(*this, _diagnosticsRepository),
      _campaign(_campaignRepository, _events, _individualProgression),
      _proofs(_proofRepository, _events),
      _contracts(_contractRepository, _events),
      _director(_directorRepository, _events),
      _directorReconciliation(_directorRepository),
      _professionOrders(_professionOrderRepository, _events),
      _bestiary(_bestiaryRepository, _events)
{
    _eventBus.RegisterConsumer(_chronicle);
    _eventBus.RegisterConsumer(_contracts);
    _eventBus.RegisterConsumer(_professionOrders);
    _eventBus.RegisterConsumer(_bestiary);
}

App& App::Instance()
{
    static App instance;
    return instance;
}

void App::Initialize()
{
    if (_initialized)
        return;

    _enabled = sConfigMgr->GetOption<bool>("Fury.Enable", true);

    _ticks.fastMs = ReadTickInterval("Fury.Tick.FastMs", 250);
    _ticks.serviceMs = ReadTickInterval("Fury.Tick.ServiceMs", 1000);
    _ticks.directorMs = ReadTickInterval("Fury.Tick.DirectorMs", 5000);
    _ticks.reconcileMs = ReadTickInterval("Fury.Tick.ReconcileMs", 30000);
    _eventBus.SetReplayBatchSize(
        sConfigMgr->GetOption<uint32>("Fury.Event.ReplayBatchSize", 500));

    ResetTimers();

    if (_enabled && !_households.Initialize())
    {
        LOG_ERROR("server.loading", "[FURY] household service initialization failed.");
        _enabled = false;
    }

    _initialized = true;

    LOG_INFO(
        "server.loading",
        "[FURY] mod-fury initialized (enabled={}, ticks={}ms/{}ms/{}ms/{}ms).",
        _enabled ? "true" : "false",
        _ticks.fastMs,
        _ticks.serviceMs,
        _ticks.directorMs,
        _ticks.reconcileMs);
}

void App::Update(uint32 diff)
{
    if (!_initialized || !_enabled)
        return;

    if (AdvanceTimer(_fastAccumulator, diff, _ticks.fastMs))
        RunFastTick();

    if (AdvanceTimer(_serviceAccumulator, diff, _ticks.serviceMs))
        RunServiceTick();

    if (AdvanceTimer(_directorAccumulator, diff, _ticks.directorMs))
        RunDirectorTick();

    if (AdvanceTimer(_reconcileAccumulator, diff, _ticks.reconcileMs))
        RunReconcileTick();
}

void App::Shutdown()
{
    if (!_initialized)
        return;

    LOG_INFO("server.loading", "[FURY] mod-fury shutdown.");

    _households.Shutdown();
    ResetTimers();
    _enabled = false;
    _initialized = false;
}

bool App::AdvanceTimer(uint64& accumulator, uint32 diff, uint32 interval)
{
    accumulator += diff;
    if (accumulator < interval)
        return false;

    // Run each cadence at most once per world update. If the server stalls,
    // discard missed repetitions instead of causing a catch-up burst.
    accumulator %= interval;
    return true;
}

void App::ResetTimers()
{
    _fastAccumulator = 0;
    _serviceAccumulator = 0;
    _directorAccumulator = 0;
    _reconcileAccumulator = 0;
}

void App::RunFastTick()
{
    // Reserved for cheap queue flushing. No database scans belong here.
}

void App::RunServiceTick()
{
    _eventBus.ReplayPending();
}

void App::RunDirectorTick()
{
    // T16 Director mutations are service/event driven. Do not poll the
    // database every Director cadence.
}

void App::RunReconcileTick()
{
    // T16 exposes a deterministic reconciliation planner. M3 supplies the
    // external runtime probe/bridge that can execute those decisions.
}
}
