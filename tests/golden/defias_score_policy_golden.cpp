#include "content/defias/DefiasScorePolicy.h"

#include <cstdlib>
#include <iostream>

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
}

int main()
{
    using namespace Fury::Defias;

    ScoreThresholds defaults;

    Require(
        ResolveScoreOutcome(0, defaults) ==
            ScoreOutcome::Ignored,
        "zero score is Ignored");
    Require(
        ResolveScoreOutcome(29, defaults) ==
            ScoreOutcome::Ignored,
        "29 remains Ignored");
    Require(
        ResolveScoreOutcome(30, defaults) ==
            ScoreOutcome::Partial,
        "30 begins Partial");
    Require(
        ResolveScoreOutcome(69, defaults) ==
            ScoreOutcome::Partial,
        "69 remains Partial");
    Require(
        ResolveScoreOutcome(70, defaults) ==
            ScoreOutcome::Success,
        "70 begins Success");
    Require(
        ResolveScoreOutcome(100, defaults) ==
            ScoreOutcome::Success,
        "100 is Success");

    ScoreThresholds custom;
    custom.partial = 40;
    custom.success = 80;

    Require(
        ResolveScoreOutcome(39, custom) ==
            ScoreOutcome::Ignored &&
        ResolveScoreOutcome(40, custom) ==
            ScoreOutcome::Partial &&
        ResolveScoreOutcome(80, custom) ==
            ScoreOutcome::Success,
        "custom thresholds are honored deterministically");

    Require(
        ScoreOutcomeKey(ScoreOutcome::Success) == "success" &&
        ScoreOutcomeKey(ScoreOutcome::Partial) == "partial" &&
        ScoreOutcomeKey(ScoreOutcome::Ignored) == "ignored",
        "outcome keys stay stable for T32 persistence");

    std::cout << "[FURY][PASS] T29 score policy gate passed\n";
    return 0;
}
