#ifndef MOD_FURY_LIVING_WORLD_ADAPTER_H
#define MOD_FURY_LIVING_WORLD_ADAPTER_H

#include "Define.h"
#include "ObjectGuid.h"
#include "director/DirectorReconciliation.h"
#include "events/FuryEvent.h"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Fury
{
class EventStore;

enum class LivingWorldAvailability : uint8
{
    Unavailable = 1,
    Available = 2
};

enum class LivingWorldRuntimeState : uint8
{
    Missing = 1,
    Initializing = 2,
    Running = 3,
    Complete = 4,
    Failed = 5
};

struct LivingWorldRuntimeSnapshot
{
    LivingWorldRuntimeState state = LivingWorldRuntimeState::Missing;
    uint32 invasionId = 0;
    uint64 runtimeId = 0;
    uint32 stageId = 0;
    uint64 stageStartedAt = 0;
    uint64 stageEndsAt = 0;

    [[nodiscard]] bool IsActive() const
    {
        return state == LivingWorldRuntimeState::Initializing ||
            state == LivingWorldRuntimeState::Running;
    }
};

struct LivingWorldEntityMetadata
{
    uint64 runtimeId = 0;
    uint64 runtimeGroupId = 0;
    uint32 spawnGroupId = 0;
    uint32 memberId = 0;
    uint32 entry = 0;
    uint32 livingWorldTemplateId = 0;
    uint8 tacticalRole = 0;
};

struct LivingWorldInvasionMetadata
{
    uint32 id = 0;
    uint16 mapId = 0;
    uint32 zoneId = 0;
    bool allowRandomStart = false;
    bool enabled = false;
};

struct LivingWorldStageMetadata
{
    uint32 id = 0;
    uint16 order = 0;
    uint8 completionType = 0;
    uint32 completionTargetId = 0;
    bool enabled = false;
};

class LivingWorldContentReader
{
public:
    virtual ~LivingWorldContentReader() = default;

    [[nodiscard]] virtual bool ContentAvailable() const = 0;
    [[nodiscard]] virtual std::optional<LivingWorldInvasionMetadata>
    Invasion(uint32 invasionId) const = 0;
    [[nodiscard]] virtual std::vector<LivingWorldStageMetadata>
    Stages(uint32 invasionId) const = 0;
    [[nodiscard]] virtual bool HasRuntimeSignal(uint32 signalId) const = 0;
    [[nodiscard]] virtual bool HasSpawnGroup(uint32 spawnGroupId) const = 0;
};

enum class LivingWorldStartOutcome : uint8
{
    Started = 1,
    AlreadyActive = 2,
    Unavailable = 3,
    DefinitionMissing = 4,
    InvalidSource = 5,
    Rejected = 6,
    RuntimeMissing = 7,
    PersistenceFailed = 8
};

struct LivingWorldStartResult
{
    LivingWorldStartOutcome outcome = LivingWorldStartOutcome::Unavailable;
    std::optional<LivingWorldRuntimeSnapshot> runtime;

    [[nodiscard]] bool Accepted() const
    {
        return outcome == LivingWorldStartOutcome::Started ||
            outcome == LivingWorldStartOutcome::AlreadyActive;
    }
};

enum class LivingWorldSignalOutcome : uint8
{
    Emitted = 1,
    AlreadyEmitted = 2,
    Unavailable = 3,
    InvalidSource = 4,
    RuntimeMissing = 5,
    SignalMissing = 6,
    Rejected = 7,
    PersistenceFailed = 8
};

class LivingWorldAdapter final
    : public DirectorRuntimeProbe,
      public LivingWorldContentReader
{
public:
    explicit LivingWorldAdapter(EventStore const& events)
        : _events(events)
    {
    }

    [[nodiscard]] LivingWorldAvailability Availability() const;
    [[nodiscard]] bool ContentAvailable() const override
    {
        return Availability() == LivingWorldAvailability::Available;
    }

    [[nodiscard]] std::optional<LivingWorldInvasionMetadata>
    Invasion(uint32 invasionId) const override;

    [[nodiscard]] std::vector<LivingWorldStageMetadata>
    Stages(uint32 invasionId) const override;

    [[nodiscard]] bool HasRuntimeSignal(uint32 signalId) const override;
    [[nodiscard]] bool HasSpawnGroup(uint32 spawnGroupId) const override;

    // Content layers register the stable relationship. Re-registering the
    // same mapping is idempotent; changing an existing graph mapping is
    // rejected so a live Director run cannot silently switch executors.
    [[nodiscard]] bool ManageGraph(
        std::string_view graphKey,
        uint32 invasionId);

    void Reset();

    [[nodiscard]] std::optional<uint32> ManagedInvasion(
        std::string_view graphKey) const;

    [[nodiscard]] LivingWorldStartResult StartInvasion(
        FuryEvent const& source,
        uint32 invasionId) const;

    [[nodiscard]] LivingWorldSignalOutcome EmitSignal(
        FuryEvent const& source,
        uint64 runtimeId,
        uint32 signalId) const;

    [[nodiscard]] std::optional<LivingWorldRuntimeSnapshot>
    RuntimeForInvasion(uint32 invasionId) const;

    [[nodiscard]] std::optional<LivingWorldEntityMetadata>
    FindEntity(ObjectGuid guid) const;

    // Polling is intentional: pinned Living World exposes stable runtime
    // queries but no generic lifecycle callback surface. Durable event
    // dedupe makes repeated observation replay-safe.
    void PollManagedRuntimes() const;

    [[nodiscard]] static std::string ObservationIdentity(
        std::string_view eventType, LivingWorldRuntimeSnapshot const& runtime)
    {
        return "living-world:observation:v2:" + std::string(eventType) + ":" +
            std::to_string(runtime.runtimeId) + ":" + std::to_string(runtime.stageId) +
            ":" + std::to_string(static_cast<uint8>(runtime.state));
    }

    [[nodiscard]] ExternalRuntimeSnapshot Inspect(
        DirectorRun const& run) const override;

    [[nodiscard]] static constexpr ExternalRuntimeState ToExternalState(
        LivingWorldRuntimeState state)
    {
        switch (state)
        {
            case LivingWorldRuntimeState::Initializing:
            case LivingWorldRuntimeState::Running:
                return ExternalRuntimeState::Active;
            case LivingWorldRuntimeState::Complete:
                return ExternalRuntimeState::Complete;
            case LivingWorldRuntimeState::Failed:
                return ExternalRuntimeState::Failed;
            case LivingWorldRuntimeState::Missing:
                return ExternalRuntimeState::Missing;
        }

        return ExternalRuntimeState::Missing;
    }

private:
    [[nodiscard]] bool ObserveRuntime(
        FuryEvent const* source,
        LivingWorldRuntimeSnapshot const& runtime,
        std::string_view eventType) const;

    EventStore const& _events;
    std::unordered_map<std::string, uint32> _managedGraphs;
};

static_assert(
    LivingWorldAdapter::ToExternalState(LivingWorldRuntimeState::Running) ==
    ExternalRuntimeState::Active);
static_assert(
    LivingWorldAdapter::ToExternalState(LivingWorldRuntimeState::Complete) ==
    ExternalRuntimeState::Complete);
static_assert(
    LivingWorldAdapter::ToExternalState(LivingWorldRuntimeState::Failed) ==
    ExternalRuntimeState::Failed);
static_assert(
    LivingWorldAdapter::ToExternalState(LivingWorldRuntimeState::Missing) ==
    ExternalRuntimeState::Missing);
}

#endif
