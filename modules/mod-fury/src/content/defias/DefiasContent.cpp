#include "DefiasContent.h"

#include <algorithm>

namespace Fury::Defias
{
namespace
{
void AddIssue(
    ContentValidationResult& result,
    ContentIssueCode code,
    std::string key,
    std::string message)
{
    result.issues.push_back({
        code,
        std::move(key),
        std::move(message)
    });
}
}

bool ContentService::Initialize(LivingWorldContentReader const& content)
{
    _validation = Validate(content);
    _initialized = true;
    return _validation.Valid();
}

void ContentService::Reset()
{
    _initialized = false;
    _validation.issues.clear();
}

ContentValidationResult ContentService::Validate(
    LivingWorldContentReader const& content)
{
    ContentValidationResult result;

    if (!content.ContentAvailable())
    {
        AddIssue(
            result,
            ContentIssueCode::LivingWorldUnavailable,
            "living_world",
            "Living World authored content is unavailable.");
        return result;
    }

    std::optional<LivingWorldInvasionMetadata> invasion =
        content.Invasion(InvasionId);

    if (!invasion)
    {
        AddIssue(
            result,
            ContentIssueCode::InvasionMissing,
            "invasion:1",
            "Required Defias invasion id 1 is missing.");
    }
    else
    {
        if (!invasion->enabled)
        {
            AddIssue(
                result,
                ContentIssueCode::InvasionDisabled,
                "invasion:1",
                "Defias invasion id 1 exists but is disabled.");
        }

        if (invasion->mapId != MapId || invasion->zoneId != ZoneId)
        {
            AddIssue(
                result,
                ContentIssueCode::InvasionLocationMismatch,
                "invasion:1",
                "Defias invasion id 1 no longer targets map 0 / zone 40.");
        }

        if (invasion->allowRandomStart)
        {
            AddIssue(
                result,
                ContentIssueCode::RandomStartEnabled,
                "invasion:1",
                "Defias invasion id 1 still allows random start; "
                "the FURY world-DB overlay must set allow_random_start=0.");
        }
    }

    std::vector<LivingWorldStageMetadata> const stages =
        content.Stages(InvasionId);

    for (ExpectedStage const& expected : RequiredStages)
    {
        auto itr = std::find_if(
            stages.begin(),
            stages.end(),
            [&expected](LivingWorldStageMetadata const& stage)
            {
                return stage.id == expected.id;
            });

        std::string const key =
            "stage:" + std::to_string(expected.id);

        if (itr == stages.end())
        {
            AddIssue(
                result,
                ContentIssueCode::StageMissing,
                key,
                "Required Defias stage " +
                    std::to_string(expected.id) +
                    " is missing.");
            continue;
        }

        if (!itr->enabled)
        {
            AddIssue(
                result,
                ContentIssueCode::StageDisabled,
                key,
                "Required Defias stage " +
                    std::to_string(expected.id) +
                    " is disabled.");
        }

        if (itr->order != expected.order ||
            itr->completionType != expected.completionType ||
            itr->completionTargetId != expected.completionTargetId)
        {
            AddIssue(
                result,
                ContentIssueCode::StageContractMismatch,
                key,
                "Defias stage " +
                    std::to_string(expected.id) +
                    " completion contract differs from the pinned T21 audit.");
        }
    }

    for (uint32 signalId : RequiredSignals)
    {
        if (content.HasRuntimeSignal(signalId))
            continue;

        AddIssue(
            result,
            ContentIssueCode::SignalMissingOrDisabled,
            "signal:" + std::to_string(signalId),
            "Required Defias runtime signal " +
                std::to_string(signalId) +
                " is missing or disabled.");
    }

    for (uint32 spawnGroupId : RequiredSpawnGroups)
    {
        if (content.HasSpawnGroup(spawnGroupId))
            continue;

        AddIssue(
            result,
            ContentIssueCode::SpawnGroupMissingOrDisabled,
            "spawn_group:" + std::to_string(spawnGroupId),
            "Required Defias spawn group " +
                std::to_string(spawnGroupId) +
                " is missing or disabled.");
    }

    return result;
}
}
