#ifndef MOD_FURY_DEFIAS_FIELD_RELIEF_H
#define MOD_FURY_DEFIAS_FIELD_RELIEF_H

#include "Define.h"

#include <array>
#include <optional>
#include <string_view>

class Player;

namespace Fury::Defias
{
inline constexpr char FieldReliefOrderKey[] =
    "classic.westfall.defias.field_relief.supplies";

struct FieldReliefOption
{
    uint16 ordinal = 0;
    uint32 skillId = 0;
    uint16 minimumSkill = 1;
    uint32 itemId = 0;
    uint32 requiredCount = 0;
    std::string_view label;
};

inline constexpr std::array<FieldReliefOption, 7> FieldReliefOptions{{
    {1, 129, 1, 1251, 8, "Linen Bandages"},
    {2, 171, 1, 118, 5, "Minor Healing Potions"},
    {3, 185, 1, 2679, 8, "Charred Wolf Meat"},
    {4, 165, 1, 2304, 4, "Light Armor Kits"},
    {5, 164, 1, 2862, 6, "Rough Sharpening Stones"},
    {6, 197, 1, 2996, 6, "Bolts of Linen Cloth"},
    {7, 202, 1, 4357, 8, "Rough Blasting Powder"}
}};

[[nodiscard]] constexpr uint16 SelectFieldReliefOrdinal(
    std::array<uint16, FieldReliefOptions.size()> const& skillValues)
{
    for (std::size_t index = 0; index < FieldReliefOptions.size(); ++index)
    {
        if (skillValues[index] >= FieldReliefOptions[index].minimumSkill)
            return FieldReliefOptions[index].ordinal;
    }

    return 0;
}

[[nodiscard]] std::optional<FieldReliefOption>
SelectFieldReliefOption(Player const* player);
}

#endif
