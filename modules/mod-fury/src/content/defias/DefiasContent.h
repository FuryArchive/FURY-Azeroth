#ifndef MOD_FURY_DEFIAS_CONTENT_H
#define MOD_FURY_DEFIAS_CONTENT_H

#include "Define.h"
#include "integrations/LivingWorldAdapter.h"

#include <array>
#include <string>
#include <vector>

namespace Fury::Defias
{
inline constexpr uint32 InvasionId = 1;
inline constexpr uint16 MapId = 0;
inline constexpr uint32 ZoneId = 40;

struct ExpectedStage
{
    uint32 id;
    uint16 order;
    uint8 completionType;
    uint32 completionTargetId;
};

inline constexpr std::array<ExpectedStage, 6> RequiredStages{{
    {1001, 10, 1, 100},
    {1002, 20, 0, 0},
    {1003, 30, 0, 0},
    {1004, 40, 0, 0},
    {1005, 50, 1, 103},
    {1006, 60, 1, 104}
}};

inline constexpr std::array<uint32, 5> RequiredSignals{{
    100, 101, 102, 103, 104
}};

inline constexpr std::array<uint32, 8> RequiredSpawnGroups{{
    100, 101, 102, 103, 104, 105, 106, 107
}};

enum class ContentIssueCode : uint8
{
    LivingWorldUnavailable = 1,
    InvasionMissing = 2,
    InvasionDisabled = 3,
    InvasionLocationMismatch = 4,
    RandomStartEnabled = 5,
    StageMissing = 6,
    StageDisabled = 7,
    StageContractMismatch = 8,
    SignalMissingOrDisabled = 9,
    SpawnGroupMissingOrDisabled = 10
};

struct ContentIssue
{
    ContentIssueCode code = ContentIssueCode::LivingWorldUnavailable;
    std::string key;
    std::string message;
};

struct ContentValidationResult
{
    std::vector<ContentIssue> issues;

    [[nodiscard]] bool Valid() const
    {
        return issues.empty();
    }
};

class ContentService final
{
public:
    [[nodiscard]] bool Initialize(LivingWorldContentReader const& content);
    void Reset();

    [[nodiscard]] bool IsValid() const
    {
        return _initialized && _validation.Valid();
    }

    [[nodiscard]] bool IsInitialized() const
    {
        return _initialized;
    }

    [[nodiscard]] ContentValidationResult const& Validation() const
    {
        return _validation;
    }

    [[nodiscard]] static ContentValidationResult Validate(
        LivingWorldContentReader const& content);

private:
    bool _initialized = false;
    ContentValidationResult _validation;
};
}

#endif
