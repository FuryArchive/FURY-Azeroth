#include "DefiasScoreService.h"

#include "DefiasGraph.h"
#include "Config.h"
#include "Log.h"
#include <algorithm>

namespace Fury::Defias
{
ScoreService::ScoreService(
    DirectorScoreRepository const& scores)
    : _scores(scores)
{
}

void ScoreService::Initialize()
{
    Reset();

    _thresholds.partial = std::clamp<uint16>(
        sConfigMgr->GetOption<uint16>(
            "Fury.Defias.Score.PartialThreshold",
            30),
        0,
        100);

    _thresholds.success = std::clamp<uint16>(
        sConfigMgr->GetOption<uint16>(
            "Fury.Defias.Score.SuccessThreshold",
            70),
        0,
        100);

    if (_thresholds.partial >= _thresholds.success)
    {
        LOG_ERROR(
            "server.loading",
            "[FURY] Defias score thresholds invalid "
            "(partial={}, success={}); scoring disabled.",
            _thresholds.partial,
            _thresholds.success);
        return;
    }

    std::optional<uint32> definitionTotal =
        _scores.DefinitionTotal(GraphKey);

    if (!definitionTotal || *definitionTotal != 100)
    {
        LOG_ERROR(
            "server.loading",
            "[FURY] Defias score definition must total 100 "
            "(actual={}); scoring disabled.",
            definitionTotal
                ? std::to_string(*definitionTotal)
                : std::string("unavailable"));
        return;
    }

    _enabled = true;

    LOG_INFO(
        "server.loading",
        "[FURY] Defias scoring enabled: total=100 "
        "partial>={} success>={}.",
        _thresholds.partial,
        _thresholds.success);
}

void ScoreService::Reset()
{
    _thresholds = {};
    _enabled = false;
}

bool ScoreService::Handle(FuryEvent const& event)
{
    if (!_enabled ||
        !event.id ||
        !event.actor.householdId ||
        !*event.actor.householdId)
    {
        return true;
    }

    std::optional<DirectorRunId> runId =
        _scores.ResolveRunForEvent(
            GraphKey,
            event);

    if (!runId)
        return true;

    std::vector<DirectorScoreComponent> components =
        _scores.FindMatchingComponents(
            GraphKey,
            event);

    for (DirectorScoreComponent const& component :
         components)
    {
        _scores.Award(
            *runId,
            GraphKey,
            component,
            event.id);

        std::optional<DirectorScoreAward> persisted =
            _scores.FindAward(
                *runId,
                component.componentKey);

        if (!persisted)
        {
            LOG_ERROR(
                "server.loading",
                "[FURY] Defias score award failed "
                "(run={}, component={}, event={}).",
                *runId,
                component.componentKey,
                event.id);
            return false;
        }
    }

    return true;
}

ScoreOutcome ScoreService::Outcome(
    DirectorRunId runId) const
{
    return ResolveScoreOutcome(
        _scores.Score(runId).value_or(0),
        _thresholds);
}
}
