#include "LivingWorldAdapter.h"

#include "core/FuryKey.h"
#include "events/EventStore.h"

#include "StringFormat.h"

#include <unordered_set>

#if __has_include("InvasionRuntime.h") && \
    __has_include("InvasionRuntimeManager.h") && \
    __has_include("InvasionScheduler.h") && \
    __has_include("LivingWorld.h") && \
    __has_include("RuntimeEntityGroup.h") && \
    __has_include("RuntimeSignalManager.h")
#include "InvasionRuntime.h"
#include "InvasionRuntimeManager.h"
#include "InvasionScheduler.h"
#include "LivingWorld.h"
#include "RuntimeEntityGroup.h"
#include "RuntimeSignalManager.h"
#define FURY_HAS_LIVING_WORLD 1
#else
#define FURY_HAS_LIVING_WORLD 0
#endif

namespace Fury
{
LivingWorldAvailability LivingWorldAdapter::Availability() const
{
#if FURY_HAS_LIVING_WORLD
    return LivingWorldAvailability::Available;
#else
    return LivingWorldAvailability::Unavailable;
#endif
}

std::optional<LivingWorldInvasionMetadata>
LivingWorldAdapter::Invasion(uint32 invasionId) const
{
#if !FURY_HAS_LIVING_WORLD
    (void)invasionId;
    return std::nullopt;
#else
    lw::InvasionDefinition const* definition =
        sLivingWorldDataMgr.GetDefinition(invasionId);
    if (!definition)
        return std::nullopt;

    return LivingWorldInvasionMetadata{
        definition->Id,
        definition->MapId,
        definition->ZoneId,
        definition->AllowRandomStart,
        definition->Enabled
    };
#endif
}

std::vector<LivingWorldStageMetadata>
LivingWorldAdapter::Stages(uint32 invasionId) const
{
#if !FURY_HAS_LIVING_WORLD
    (void)invasionId;
    return {};
#else
    std::vector<LivingWorldStageMetadata> result;

    std::vector<lw::InvasionStageDefinition> const* stages =
        sLivingWorldDataMgr.GetStages(invasionId);
    if (!stages)
        return result;

    result.reserve(stages->size());
    for (lw::InvasionStageDefinition const& stage : *stages)
    {
        result.push_back({
            stage.Id,
            stage.StageOrder,
            stage.CompletionType,
            stage.CompletionTargetId,
            stage.Enabled
        });
    }

    return result;
#endif
}

bool LivingWorldAdapter::HasRuntimeSignal(uint32 signalId) const
{
#if !FURY_HAS_LIVING_WORLD
    (void)signalId;
    return false;
#else
    lw::RuntimeSignalDefinition const* signal =
        sLivingWorldDataMgr.GetRuntimeSignal(signalId);
    return signal && signal->Enabled;
#endif
}

bool LivingWorldAdapter::HasSpawnGroup(uint32 spawnGroupId) const
{
#if !FURY_HAS_LIVING_WORLD
    (void)spawnGroupId;
    return false;
#else
    lw::SpawnGroupDefinition const* group =
        sLivingWorldDataMgr.GetSpawnGroup(spawnGroupId);
    return group && group->Enabled;
#endif
}

bool LivingWorldAdapter::ManageGraph(
    std::string_view graphKey,
    uint32 invasionId)
{
    if (!IsCanonicalKey(graphKey) || invasionId == 0)
        return false;

    auto const [itr, inserted] =
        _managedGraphs.emplace(std::string(graphKey), invasionId);

#if FURY_HAS_LIVING_WORLD
    // The pinned executor deletes terminal runtimes in the same update. Capture
    // their result durably before deletion instead of hoping polling sees it.
    sInvasionRuntimeMgr.SetCompletionObserver([this](lw::RuntimeCompletion const& completion)
    {
        bool managed = false;
        for (auto const& entry : _managedGraphs)
            managed = managed || entry.second == completion.invasionId;
        if (!managed) return true;
        LivingWorldRuntimeSnapshot snapshot;
        snapshot.runtimeId = completion.runtimeId;
        snapshot.invasionId = completion.invasionId;
        snapshot.stageId = completion.stageId;
        snapshot.state = completion.success ? LivingWorldRuntimeState::Complete : LivingWorldRuntimeState::Failed;
        return ObserveRuntime(nullptr, snapshot, "living_world.runtime.observed");
    });
#endif
    return inserted || itr->second == invasionId;
}

void LivingWorldAdapter::Reset()
{
#if FURY_HAS_LIVING_WORLD
    sInvasionRuntimeMgr.SetCompletionObserver({});
#endif
    _managedGraphs.clear();
}

std::optional<uint32> LivingWorldAdapter::ManagedInvasion(
    std::string_view graphKey) const
{
    auto itr = _managedGraphs.find(std::string(graphKey));
    if (itr == _managedGraphs.end())
        return std::nullopt;

    return itr->second;
}

LivingWorldStartResult LivingWorldAdapter::StartInvasion(
    FuryEvent const& source,
    uint32 invasionId) const
{
#if !FURY_HAS_LIVING_WORLD
    (void)source;
    (void)invasionId;
    return {LivingWorldStartOutcome::Unavailable, std::nullopt};
#else
    if (source.id == 0)
        return {LivingWorldStartOutcome::InvalidSource, std::nullopt};

    if (!sLivingWorldDataMgr.GetDefinition(invasionId))
        return {LivingWorldStartOutcome::DefinitionMissing, std::nullopt};

    if (sInvasionScheduler.IsInvasionActive(invasionId))
    {
        std::optional<LivingWorldRuntimeSnapshot> runtime =
            RuntimeForInvasion(invasionId);

        if (!runtime)
            return {LivingWorldStartOutcome::RuntimeMissing, std::nullopt};

        if (!ObserveRuntime(
                &source,
                *runtime,
                "living_world.runtime.started"))
        {
            return {
                LivingWorldStartOutcome::PersistenceFailed,
                runtime
            };
        }

        return {LivingWorldStartOutcome::AlreadyActive, runtime};
    }

    if (!sInvasionScheduler.TriggerInvasion(invasionId))
        return {LivingWorldStartOutcome::Rejected, std::nullopt};

    std::optional<LivingWorldRuntimeSnapshot> runtime =
        RuntimeForInvasion(invasionId);

    if (!runtime)
        return {LivingWorldStartOutcome::RuntimeMissing, std::nullopt};

    if (!ObserveRuntime(
            &source,
            *runtime,
            "living_world.runtime.started"))
    {
        return {
            LivingWorldStartOutcome::PersistenceFailed,
            runtime
        };
    }

    return {LivingWorldStartOutcome::Started, runtime};
#endif
}

LivingWorldSignalOutcome LivingWorldAdapter::EmitSignal(
    FuryEvent const& source,
    uint64 runtimeId,
    uint32 signalId) const
{
#if !FURY_HAS_LIVING_WORLD
    (void)source;
    (void)runtimeId;
    (void)signalId;
    return LivingWorldSignalOutcome::Unavailable;
#else
    if (source.id == 0)
        return LivingWorldSignalOutcome::InvalidSource;

    if (!sInvasionRuntimeMgr.GetRuntime(runtimeId))
        return LivingWorldSignalOutcome::RuntimeMissing;

    if (!sLivingWorldDataMgr.GetRuntimeSignal(signalId))
        return LivingWorldSignalOutcome::SignalMissing;

    bool const alreadyEmitted =
        sRuntimeSignalMgr.HasSignal(runtimeId, signalId);

    if (!sRuntimeSignalMgr.EmitSignal(runtimeId, signalId))
        return LivingWorldSignalOutcome::Rejected;

    FuryEvent event;
    event.type = "living_world.signal.emitted";
    event.actor = source.actor;
    event.mapId = source.mapId;
    event.zoneId = source.zoneId;
    event.areaId = source.areaId;
    event.subjectType = "living_world_runtime";
    event.subjectId = runtimeId;
    event.sourceSystem = "living_world";
    event.correlationKey = Acore::StringFormat("signal:{}", signalId);
    event.dedupeIdentity = Acore::StringFormat(
        "living-world:signal:v1:{}:{}",
        runtimeId,
        signalId);
    event.payloadJson = Acore::StringFormat(
        "{{\"runtime_id\":{},\"signal_id\":{},"
        "\"source_event_id\":{}}}",
        runtimeId,
        signalId,
        source.id);

    if (!_events.Append(event))
        return LivingWorldSignalOutcome::PersistenceFailed;

    return alreadyEmitted
        ? LivingWorldSignalOutcome::AlreadyEmitted
        : LivingWorldSignalOutcome::Emitted;
#endif
}

std::optional<LivingWorldRuntimeSnapshot>
LivingWorldAdapter::RuntimeForInvasion(uint32 invasionId) const
{
#if !FURY_HAS_LIVING_WORLD
    (void)invasionId;
    return std::nullopt;
#else
    lw::InvasionRuntime const* runtime =
        sInvasionRuntimeMgr.GetRuntimeForInvasion(invasionId);
    if (!runtime)
        return std::nullopt;

    LivingWorldRuntimeSnapshot snapshot;
    snapshot.invasionId = runtime->GetInvasionId();
    snapshot.runtimeId = runtime->GetRuntimeId();
    snapshot.stageStartedAt = runtime->GetStageStartedAt();
    snapshot.stageEndsAt = runtime->GetStageEndsAt();

    switch (runtime->GetState())
    {
        case lw::ActiveRuntimeState::Initializing:
            snapshot.state = LivingWorldRuntimeState::Initializing;
            break;
        case lw::ActiveRuntimeState::Running:
            snapshot.state = LivingWorldRuntimeState::Running;
            break;
        case lw::ActiveRuntimeState::Completed:
            snapshot.state = LivingWorldRuntimeState::Complete;
            break;
        case lw::ActiveRuntimeState::Failed:
            snapshot.state = LivingWorldRuntimeState::Failed;
            break;
    }

    if (lw::InvasionStageDefinition const* stage =
            runtime->GetCurrentStage())
    {
        snapshot.stageId = stage->Id;
    }

    return snapshot;
#endif
}

std::optional<LivingWorldEntityMetadata>
LivingWorldAdapter::FindEntity(ObjectGuid guid) const
{
#if !FURY_HAS_LIVING_WORLD
    (void)guid;
    return std::nullopt;
#else
    if (guid.IsEmpty())
        return std::nullopt;

    lw::RuntimeEntityMetadata metadata;
    if (!sRuntimeEntityGroupMgr.FindEntityMetadata(guid, metadata))
        return std::nullopt;

    LivingWorldEntityMetadata result;
    result.runtimeId = metadata.RuntimeId;
    result.runtimeGroupId = metadata.RuntimeGroupId;
    result.spawnGroupId = metadata.SpawnGroupId;
    result.memberId = metadata.Entity.MemberId;
    result.entry = metadata.Entity.Entry;
    result.livingWorldTemplateId = metadata.Entity.LwTemplateId;
    result.tacticalRole = metadata.Entity.TacticalRole;
    return result;
#endif
}

void LivingWorldAdapter::PollManagedRuntimes() const
{
    std::unordered_set<uint32> observedInvasions;

    for (auto const& [graphKey, invasionId] : _managedGraphs)
    {
        (void)graphKey;

        if (!observedInvasions.insert(invasionId).second)
            continue;

        std::optional<LivingWorldRuntimeSnapshot> runtime =
            RuntimeForInvasion(invasionId);
        if (!runtime)
            continue;

        (void)ObserveRuntime(
            nullptr,
            *runtime,
            "living_world.runtime.observed");

        if (runtime->stageId != 0)
        {
            (void)ObserveRuntime(
                nullptr,
                *runtime,
                "living_world.stage.observed");
        }
    }
}

ExternalRuntimeSnapshot LivingWorldAdapter::Inspect(
    DirectorRun const& run) const
{
    std::optional<uint32> invasionId = ManagedInvasion(run.graphKey);
    if (!invasionId)
        return {ExternalRuntimeState::NotManaged, std::nullopt};

    std::optional<LivingWorldRuntimeSnapshot> runtime =
        RuntimeForInvasion(*invasionId);

    if (!runtime)
        return {ExternalRuntimeState::Missing, std::nullopt};

    return {
        ToExternalState(runtime->state),
        runtime->runtimeId
    };
}

bool LivingWorldAdapter::ObserveRuntime(
    FuryEvent const* source,
    LivingWorldRuntimeSnapshot const& runtime,
    std::string_view eventType) const
{
#if !FURY_HAS_LIVING_WORLD
    (void)source;
    (void)runtime;
    (void)eventType;
    return false;
#else
    if (runtime.runtimeId == 0 || runtime.invasionId == 0)
        return false;

    FuryEvent event;
    event.type = std::string(eventType);
    if (source)
    {
        event.actor = source->actor;
        event.mapId = source->mapId;
        event.zoneId = source->zoneId;
        event.areaId = source->areaId;
    }
    else
    {
        event.actor.kind = ActorKind::System;
    }

    if (lw::InvasionDefinition const* definition =
            sLivingWorldDataMgr.GetDefinition(runtime.invasionId))
    {
        event.mapId = definition->MapId;
        event.zoneId = definition->ZoneId;
    }

    event.subjectType = "living_world_runtime";
    event.subjectId = runtime.runtimeId;
    event.sourceSystem = "living_world";
    event.correlationKey = Acore::StringFormat(
        "invasion:{}",
        runtime.invasionId);
    event.dedupeIdentity = ObservationIdentity(eventType, runtime);
    event.payloadJson = Acore::StringFormat(
        "{{\"runtime_id\":{},\"invasion_id\":{},"
        "\"stage_id\":{},\"state\":{},"
        "\"stage_started_at\":{},\"stage_ends_at\":{}}}",
        runtime.runtimeId,
        runtime.invasionId,
        runtime.stageId,
        static_cast<uint8>(runtime.state),
        runtime.stageStartedAt,
        runtime.stageEndsAt);

    return _events.Append(event).has_value();
#endif
}
}
