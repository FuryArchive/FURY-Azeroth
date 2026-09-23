#ifndef MOD_FURY_DEFIAS_SCORE_POLICY_H
#define MOD_FURY_DEFIAS_SCORE_POLICY_H

#include "Define.h"

#include <string_view>

namespace Fury::Defias
{
struct ScoreThresholds
{
    uint16 partial = 30;
    uint16 success = 70;
};

enum class ScoreOutcome : uint8
{
    Ignored = 1,
    Partial = 2,
    Success = 3
};

[[nodiscard]] constexpr ScoreOutcome ResolveScoreOutcome(
    uint32 score,
    ScoreThresholds const& thresholds = {})
{
    return score >= thresholds.success
        ? ScoreOutcome::Success
        : score >= thresholds.partial
            ? ScoreOutcome::Partial
            : ScoreOutcome::Ignored;
}

[[nodiscard]] constexpr std::string_view ScoreOutcomeKey(
    ScoreOutcome outcome)
{
    switch (outcome)
    {
        case ScoreOutcome::Success:
            return "success";
        case ScoreOutcome::Partial:
            return "partial";
        case ScoreOutcome::Ignored:
            return "ignored";
    }

    return "ignored";
}
}

#endif
