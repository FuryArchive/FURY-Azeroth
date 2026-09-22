#ifndef MOD_FURY_DIRECTOR_RECONCILIATION_H
#define MOD_FURY_DIRECTOR_RECONCILIATION_H

#include "DirectorRepository.h"

#include <optional>
#include <vector>

namespace Fury
{
enum class ExternalRuntimeState : uint8
{
    NotManaged = 1,
    Missing = 2,
    Active = 3,
    Complete = 4,
    Failed = 5
};

struct ExternalRuntimeSnapshot
{
    ExternalRuntimeState state = ExternalRuntimeState::NotManaged;
    std::optional<uint64> runtimeId;
};

enum class DirectorReconcileAction : uint8
{
    None = 1,
    AttachExternalRuntime = 2,
    ReattachExistingRuntime = 3,
    RestartMissingRuntime = 4,
    ResolveFromExternal = 5,
    FailFromExternal = 6,
    RuntimeConflict = 7
};

struct DirectorReconcileDecision
{
    DirectorRun run;
    ExternalRuntimeSnapshot external;
    DirectorReconcileAction action = DirectorReconcileAction::None;
};

class DirectorRuntimeProbe
{
public:
    virtual ~DirectorRuntimeProbe() = default;

    [[nodiscard]] virtual ExternalRuntimeSnapshot Inspect(
        DirectorRun const& run) const = 0;
};

class DirectorReconciliationService final
{
public:
    explicit DirectorReconciliationService(
        DirectorRepository const& repository)
        : _repository(repository)
    {
    }

    [[nodiscard]] std::vector<DirectorReconcileDecision> BuildPlan(
        DirectorRuntimeProbe const& probe) const;

    [[nodiscard]] static DirectorReconcileAction Evaluate(
        DirectorRun const& run,
        ExternalRuntimeSnapshot const& external);

private:
    DirectorRepository const& _repository;
};
}

#endif
