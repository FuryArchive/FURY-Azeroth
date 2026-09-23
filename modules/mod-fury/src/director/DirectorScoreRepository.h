#ifndef MOD_FURY_DIRECTOR_SCORE_REPOSITORY_H
#define MOD_FURY_DIRECTOR_SCORE_REPOSITORY_H

#include "DirectorScoreTypes.h"

#include <optional>
#include <string_view>
#include <vector>

namespace Fury
{
class DirectorScoreRepository final
{
public:
    [[nodiscard]] std::vector<DirectorScoreComponent>
    FindMatchingComponents(
        std::string_view graphKey,
        FuryEvent const& event) const;

    [[nodiscard]] std::optional<uint32> DefinitionTotal(
        std::string_view graphKey) const;

    void Award(
        DirectorRunId directorRunId,
        std::string_view graphKey,
        DirectorScoreComponent const& component,
        EventId sourceEventId) const;

    [[nodiscard]] std::optional<DirectorScoreAward> FindAward(
        DirectorRunId directorRunId,
        std::string_view componentKey) const;

    [[nodiscard]] std::optional<uint32> Score(
        DirectorRunId directorRunId) const;
};
}

#endif
