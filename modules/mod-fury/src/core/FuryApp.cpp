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
      _livingWorld(_events),
      _campaign(_campaignRepository, _events, _individualProgression),
      _proofs(_proofRepository, _events),
      _contracts(_contractRepository, _events),
      _director(_directorRepository, _events),
      _directorReconciliation(_directorRepository),
      _professionOrders(_professionOrderRepository, _events),
      _bestiary(_bestiaryRepository, _events, _directorRepository),
      _defiasParticipation(_livingWorld, _actors, _events),
      _defiasScore(_directorScoreRepository),
      _defiasGraph(_director, _directorRepository, _campaign, _livingWorld, _defiasContent, _events, _actors)
{
    _eventBus.RegisterConsumer(_chronicle);
    _eventBus.RegisterConsumer(_contracts);
    _eventBus.RegisterConsumer(_professionOrders);
    _eventBus.RegisterConsumer(_bestiary);
    _eventBus.RegisterConsumer(_defiasGraph);
    _eventBus.RegisterConsumer(_defiasScore);
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

    if (_enabled && !_defiasContent.Initialize(_livingWorld))
    {
        LOG_ERROR(
            "server.loading",
            "[FURY] Defias content validation failed; M3 Defias content is disabled.");

        for (Defias::ContentIssue const& issue :
             _defiasContent.Validation().issues)
        {
            LOG_ERROR(
                "server.loading",
                "[FURY] Defias content issue [{}]: {}",
                issue.key,
                issue.message);
        }
    }
    else if (_enabled)
    {
        LOG_INFO(
            "server.loading",
            "[FURY] Defias authored-content contract validated.");
    }

    if (_enabled)
    {
        _defiasParticipation.Initialize();
        _defiasScore.Initialize();
        _defiasGraph.Initialize();
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

    _defiasGraph.Reset();
    _defiasScore.Reset();
    _defiasParticipation.Reset();
    _defiasContent.Reset();
    _livingWorld.Reset();
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
    _defiasGraph.Tick();
}

void App::RunReconcileTick()
{
    // Living World exposes stable runtime queries rather than a generic
    // lifecycle callback surface. Normalize currently managed runtimes into
    // replay-safe FURY events, then let the existing Director reconciler
    // inspect the same adapter boundary. T33 executes non-trivial recovery
    // decisions; T23 establishes the probe/integration seam only.
    _livingWorld.PollManagedRuntimes();

    auto const plan =
        _directorReconciliation.BuildPlan(_livingWorld);
    (void)plan;
}
}
