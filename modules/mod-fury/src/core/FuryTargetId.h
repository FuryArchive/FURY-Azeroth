#ifndef MOD_FURY_TARGET_ID_H
#define MOD_FURY_TARGET_ID_H

#include "Define.h"

namespace Fury
{
[[nodiscard]] constexpr uint64 MakeProfessionCraftTarget(
    uint32 skillId,
    uint32 itemId)
{
    return (static_cast<uint64>(skillId) << 32) |
        static_cast<uint64>(itemId);
}

[[nodiscard]] constexpr uint32 ProfessionCraftSkill(uint64 target)
{
    return static_cast<uint32>(target >> 32);
}

[[nodiscard]] constexpr uint32 ProfessionCraftItem(uint64 target)
{
    return static_cast<uint32>(target & 0xFFFFFFFFULL);
}

static_assert(
    ProfessionCraftSkill(MakeProfessionCraftTarget(164, 2840)) == 164);
static_assert(
    ProfessionCraftItem(MakeProfessionCraftTarget(164, 2840)) == 2840);
}

#endif
