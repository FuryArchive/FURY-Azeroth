#ifndef MOD_FURY_DEFIAS_SCORE_SERVICE_H
#define MOD_FURY_DEFIAS_SCORE_SERVICE_H

#include "DefiasScorePolicy.h"
#include "director/DirectorScoreRepository.h"
#include "events/EventConsumer.h"

namespace Fury
{
class DirectorRepository;
}

namespace Fury::Defias
{
class ScoreService final : public EventConsumer
{
public:
    ScoreService(
        DirectorScoreRepository const& scores,
        DirectorRepository const& director);

    void Initialize();
    void Reset();

    [[nodiscard]] std::string_view Key() const override
    {
        return "defias.score.v1";
    }

    bool Handle(FuryEvent const& event) override;

    [[nodiscard]] bool Enabled() const { return _enabled; }

    [[nodiscard]] std::optional<uint32> Score(
        DirectorRunId runId) const
    {
        return _scores.Score(runId);
    }

    [[nodiscard]] ScoreOutcome Outcome(
        DirectorRunId runId) const;

    [[nodiscard]] ScoreThresholds const& Thresholds() const
    {
        return _thresholds;
    }

private:
    DirectorScoreRepository const& _scores;
    DirectorRepository const& _director;
    ScoreThresholds _thresholds;
    bool _enabled = false;
};
}

#endif
