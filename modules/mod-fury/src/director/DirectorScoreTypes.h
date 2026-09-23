#ifndef MOD_FURY_DIRECTOR_SCORE_TYPES_H
#define MOD_FURY_DIRECTOR_SCORE_TYPES_H

#include "DirectorTypes.h"

#include <optional>
#include <string>

namespace Fury
{
struct DirectorScoreComponent
{
    std::string graphKey;
    std::string componentKey;
    uint16 scoreValue = 0;
    std::string sourceEventType;
    std::optional<std::string> sourceCorrelationKey;
    bool enabled = false;
};

struct DirectorScoreAward
{
    DirectorRunId directorRunId = 0;
    std::string graphKey;
    std::string componentKey;
    uint16 scoreValue = 0;
    EventId sourceEventId = 0;
};
}

#endif
