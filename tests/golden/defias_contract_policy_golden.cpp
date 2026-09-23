#include "content/defias/DefiasContracts.h"

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

    Require(IsScoutGroup(100) && IsScoutGroup(101),
        "groups 100/101 are the two pinned scout groups");
    Require(!IsScoutGroup(102),
        "control groups are not classified as scouts");

    Require(IsControlGroup(102) &&
            IsControlGroup(103) &&
            IsControlGroup(104),
        "groups 102-104 are the pinned control teams");
    Require(!IsControlGroup(105),
        "leadership is not classified as a control team");

    Require(IsCommanderGroup(105),
        "group 105 is the pinned Defias leadership group");

    for (uint32 group : HostileSpawnGroups)
        Require(IsHostileSpawnGroup(group),
            "all authored hostile Defias groups are accepted");

    Require(!IsHostileSpawnGroup(106) &&
            !IsHostileSpawnGroup(107),
        "Stormwind groups 106/107 can never author Defias kill progress");

    Require(std::string_view(ReconRoadsContract) ==
            "classic.westfall.defias.scout_report",
        "Recon Roads keeps the T25 activation contract key");

    std::cout << "[FURY][PASS] T27 Defias contract policy gate passed\n";
    return 0;
}
