#include "contracts/ContractBoardPolicy.h"

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
    using namespace Fury;

    ContractDefinition definition;
    definition.contractKey = "classic.westfall.defias.recon_roads";
    definition.boardKey = "classic.westfall.contracts";
    definition.title = "Recon Roads";
    definition.campaignNodeKey = "campaign.classic.westfall";
    definition.directorPhase = "rumours";
    definition.repeatPolicy = ContractRepeatPolicy::DirectorRun;
    definition.enabled = true;

    ContractBoardContext context;
    context.householdId = 7;
    context.campaignNodeKey = "campaign.classic.westfall";

    context.allowNewContracts = false;
    Require(!ContractMatchesBoardContext(definition, context),
        "new contracts stay hidden without an active Defias run");

    context.allowNewContracts = true;
    Require(!ContractMatchesBoardContext(definition, context),
        "DirectorRun contract requires a Director run id");

    context.directorRunId = 42;
    context.directorPhase = "invasion";
    Require(!ContractMatchesBoardContext(definition, context),
        "contract is hidden outside its authored Director phase");

    context.directorPhase = "rumours";
    Require(ContractMatchesBoardContext(definition, context),
        "contract is available in the matching campaign/run/phase");

    ContractBoardContext wrongCampaign = context;
    wrongCampaign.campaignNodeKey = "campaign.classic.redridge";
    Require(!ContractMatchesBoardContext(definition, wrongCampaign),
        "contract is hidden from a different campaign board context");

    ContractDefinition disabled = definition;
    disabled.enabled = false;
    Require(!ContractMatchesBoardContext(disabled, context),
        "disabled contract cannot become newly available");

    Require(ShouldShowContractOnBoard(disabled, wrongCampaign, true, false),
        "active contract remains visible even after context changes");
    Require(ShouldShowContractOnBoard(disabled, wrongCampaign, false, true),
        "completed contract remains visible even after context changes");
    Require(!ShouldShowContractOnBoard(disabled, wrongCampaign, false, false),
        "disabled historical-free contract stays hidden");

    std::cout << "[FURY][PASS] T26 contract board policy gate passed\n";
    return 0;
}
