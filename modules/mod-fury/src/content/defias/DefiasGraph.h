#ifndef MOD_FURY_DEFIAS_GRAPH_H
#define MOD_FURY_DEFIAS_GRAPH_H

#include "director/DirectorTypes.h"
#include "director/DirectorPolicy.h"
#include <string_view>

namespace Fury::Defias
{
inline constexpr char GraphKey[] = "classic.westfall.defias_resurgence.v1";
inline constexpr char ScopeKey[] = "classic.westfall";
inline constexpr char CampaignKey[] = "campaign.classic.westfall";

[[nodiscard]] constexpr std::string_view OutcomeForScore(uint32 score)
{
    return score >= 70 ? "success" : score >= 30 ? "partial" : "ignored";
}

// Persistence and execution boundary: the graph owns decisions; Director and
// LivingWorldAdapter own durable mutations and physical execution respectively.
class GraphPort
{
public:
    virtual ~GraphPort() = default;
    virtual bool ContentValid() const = 0;
    virtual bool CampaignComplete(HouseholdId household) = 0;
    virtual std::optional<DirectorRun> Find(HouseholdId household) = 0;
    virtual DirectorResult Start(FuryEvent const& source) = 0;
    virtual bool Advance(FuryEvent const& source, DirectorRun const& run, std::string_view phase) = 0;
    virtual std::optional<uint64> StartExternal(FuryEvent const& source) = 0;
    virtual bool Attach(FuryEvent const& source, DirectorRun const& run, uint64 runtime) = 0;
};

class Graph final
{
public:
    explicit Graph(GraphPort& port) : _port(port) { }

    [[nodiscard]] bool Enter(FuryEvent const& source, uint32 level, uint32 minimumLevel = 10)
    {
        if (!Eligible(source, level, minimumLevel) || !_port.ContentValid())
            return true;
        auto household = *source.actor.householdId;
        auto existing = _port.Find(household);
        if (existing)
        {
            // Retry the original start's event emission after row persistence.
            if (Active(*existing) && existing->startedEventId == source.id)
            {
                auto result = _port.Start(source);
                return result.Accepted() || result.outcome == DirectorOutcome::GraphDisabled;
            }
            return true;
        }
        if (_port.CampaignComplete(household)) return true;
        DirectorResult result = _port.Start(source);
        // Another graph owns the exclusive scope. This trigger is consumed;
        // a later eligible entry can try again, without blocking the stream.
        return result.Accepted() || result.outcome == DirectorOutcome::ScopeBusy ||
            result.outcome == DirectorOutcome::GraphDisabled;
    }

    [[nodiscard]] bool Activate(FuryEvent const& source, DirectorRunId runId)
    {
        if (!HumanSource(source) || !_port.ContentValid())
            return true;
        auto run = _port.Find(*source.actor.householdId);
        if (!run || run->id != runId || !Active(*run))
            return true;
        if (run->phaseKey == "rumours" || (run->phaseKey == "invasion" && !run->externalRuntimeId))
        {
            // Persist the activation decision BEFORE starting the executor.
            // Replay resumes this phase after a crash before runtime binding.
            if (!_port.Advance(source, *run, "invasion"))
                return false;
            run = _port.Find(*source.actor.householdId);
            if (!run) return false;
        }
        if (run->phaseKey != "invasion") return true;
        if (run->externalRuntimeId)
            return _port.Attach(source, *run, *run->externalRuntimeId);
        auto runtime = _port.StartExternal(source);
        return runtime && _port.Attach(source, *run, *runtime);
    }

    [[nodiscard]] bool Observe(FuryEvent const& source, uint64 runtimeId, uint32 stageId, bool terminal)
    {
        if (!source.id || !source.actor.householdId ||
            !CanMutateDirectorRun(source.actor.kind))
            return true;
        auto run = _port.Find(*source.actor.householdId);
        if (!run || !Active(*run) || !run->externalRuntimeId ||
            *run->externalRuntimeId != runtimeId)
            return true;
        if (terminal && (run->phaseKey == "invasion" || run->phaseKey == "final_battle" || run->phaseKey == "resolution"))
            return _port.Advance(source, *run, "resolution");
        if (!terminal && stageId == 1006 && (run->phaseKey == "invasion" || run->phaseKey == "final_battle"))
            return _port.Advance(source, *run, "final_battle");
        return true;
    }

    [[nodiscard]] static bool Eligible(FuryEvent const& source, uint32 level, uint32 minimumLevel)
    {
        return HumanSource(source) && source.mapId == 0 && source.zoneId == 40 &&
            minimumLevel >= 10 && level >= minimumLevel;
    }

private:
    static bool HumanSource(FuryEvent const& source)
    {
        return source.id && source.actor.householdId && *source.actor.householdId &&
            CanStartDirectorRun(source.actor.kind);
    }
    static bool Active(DirectorRun const& run)
    {
        return run.graphKey == GraphKey && run.status == DirectorRunStatus::Active;
    }
    GraphPort& _port;
};
}
#endif
