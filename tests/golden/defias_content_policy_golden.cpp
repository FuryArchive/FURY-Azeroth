#include "content/defias/DefiasContent.h"

#include <cstdlib>
#include <iostream>
#include <unordered_map>
#include <unordered_set>

namespace
{
void Require(bool condition, char const* label)
{
    if (!condition)
    {
        std::cerr << "[FURY][FAIL] " << label << '\n';
        std::exit(1);
    }

    std::cout << "[FURY][PASS] " << label << '\n';
}

bool HasIssue(
    Fury::Defias::ContentValidationResult const& result,
    Fury::Defias::ContentIssueCode code)
{
    for (Fury::Defias::ContentIssue const& issue : result.issues)
        if (issue.code == code)
            return true;

    return false;
}

class FakeContent final : public Fury::LivingWorldContentReader
{
public:
    bool available = true;
    std::optional<Fury::LivingWorldInvasionMetadata> invasion;
    std::vector<Fury::LivingWorldStageMetadata> stages;
    std::unordered_set<uint32> signals;
    std::unordered_set<uint32> groups;

    [[nodiscard]] bool ContentAvailable() const override
    {
        return available;
    }

    [[nodiscard]] std::optional<Fury::LivingWorldInvasionMetadata>
    Invasion(uint32 invasionId) const override
    {
        if (!invasion || invasion->id != invasionId)
            return std::nullopt;

        return invasion;
    }

    [[nodiscard]] std::vector<Fury::LivingWorldStageMetadata>
    Stages(uint32 invasionId) const override
    {
        if (invasionId != Fury::Defias::InvasionId)
            return {};

        return stages;
    }

    [[nodiscard]] bool HasRuntimeSignal(uint32 signalId) const override
    {
        return signals.count(signalId) != 0;
    }

    [[nodiscard]] bool HasSpawnGroup(uint32 spawnGroupId) const override
    {
        return groups.count(spawnGroupId) != 0;
    }
};

FakeContent Canonical()
{
    FakeContent content;
    content.invasion = Fury::LivingWorldInvasionMetadata{
        Fury::Defias::InvasionId,
        Fury::Defias::MapId,
        Fury::Defias::ZoneId,
        false,
        true
    };

    for (Fury::Defias::ExpectedStage const& expected :
         Fury::Defias::RequiredStages)
    {
        content.stages.push_back({
            expected.id,
            expected.order,
            expected.completionType,
            expected.completionTargetId,
            true
        });
    }

    for (uint32 signal : Fury::Defias::RequiredSignals)
        content.signals.insert(signal);

    for (uint32 group : Fury::Defias::RequiredSpawnGroups)
        content.groups.insert(group);

    return content;
}
}

int main()
{
    using namespace Fury::Defias;

    FakeContent content = Canonical();
    ContentValidationResult result = ContentService::Validate(content);
    Require(result.Valid(), "canonical pinned Defias content validates");

    content = Canonical();
    content.invasion->allowRandomStart = true;
    result = ContentService::Validate(content);
    Require(
        HasIssue(result, ContentIssueCode::RandomStartEnabled),
        "random-start drift is rejected");

    content = Canonical();
    content.stages.erase(content.stages.begin() + 4);
    result = ContentService::Validate(content);
    Require(
        HasIssue(result, ContentIssueCode::StageMissing),
        "missing pinned stage is rejected");

    content = Canonical();
    content.stages[4].completionTargetId = 999;
    result = ContentService::Validate(content);
    Require(
        HasIssue(result, ContentIssueCode::StageContractMismatch),
        "stage completion-contract drift is rejected");

    content = Canonical();
    content.signals.erase(104);
    result = ContentService::Validate(content);
    Require(
        HasIssue(result, ContentIssueCode::SignalMissingOrDisabled),
        "missing pinned runtime signal is rejected");

    content = Canonical();
    content.groups.erase(105);
    result = ContentService::Validate(content);
    Require(
        HasIssue(result, ContentIssueCode::SpawnGroupMissingOrDisabled),
        "missing pinned spawn group is rejected");

    content = Canonical();
    content.available = false;
    result = ContentService::Validate(content);
    Require(
        HasIssue(result, ContentIssueCode::LivingWorldUnavailable),
        "missing Living World content is rejected cleanly");

    std::cout
        << "[FURY][PASS] T24 Defias content policy golden gate passed\n";
    return 0;
}
