#ifndef MOD_FURY_DEFIAS_CONTRACTS_H
#define MOD_FURY_DEFIAS_CONTRACTS_H

#include "Define.h"

#include <array>
#include <string_view>

namespace Fury::Defias
{
inline constexpr char ContractBoardKey[] = "classic.westfall.contracts";

inline constexpr char ReconRoadsContract[] =
    "classic.westfall.defias.scout_report";
inline constexpr char BreakScoutsContract[] =
    "classic.westfall.defias.break_scouts";
inline constexpr char BreakControlContract[] =
    "classic.westfall.defias.break_control";
inline constexpr char HoldSentinelContract[] =
    "classic.westfall.defias.hold_sentinel";
inline constexpr char FieldReliefContract[] =
    "classic.westfall.defias.field_relief";
inline constexpr char DefeatCommanderContract[] =
    "classic.westfall.defias.defeat_commander";

inline constexpr std::array<uint32, 6> HostileSpawnGroups{{
    100, 101, 102, 103, 104, 105
}};

[[nodiscard]] constexpr bool IsHostileSpawnGroup(uint32 spawnGroupId)
{
    for (uint32 id : HostileSpawnGroups)
    {
        if (id == spawnGroupId)
            return true;
    }

    return false;
}

[[nodiscard]] constexpr bool IsScoutGroup(uint32 spawnGroupId)
{
    return spawnGroupId == 100 || spawnGroupId == 101;
}

[[nodiscard]] constexpr bool IsControlGroup(uint32 spawnGroupId)
{
    return spawnGroupId >= 102 && spawnGroupId <= 104;
}

[[nodiscard]] constexpr bool IsCommanderGroup(uint32 spawnGroupId)
{
    return spawnGroupId == 105;
}
}

#endif
