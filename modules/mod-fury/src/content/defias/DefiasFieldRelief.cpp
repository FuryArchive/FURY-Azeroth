#include "DefiasFieldRelief.h"

#include "Player.h"

namespace Fury::Defias
{
std::optional<FieldReliefOption>
SelectFieldReliefOption(Player const* player)
{
    if (!player)
        return std::nullopt;

    std::array<uint16, FieldReliefOptions.size()> values{};

    for (std::size_t index = 0; index < FieldReliefOptions.size(); ++index)
    {
        FieldReliefOption const& option = FieldReliefOptions[index];

        if (player->HasSkill(option.skillId))
            values[index] = player->GetPureSkillValue(option.skillId);
    }

    uint16 const ordinal = SelectFieldReliefOrdinal(values);
    if (!ordinal)
        return std::nullopt;

    for (FieldReliefOption const& option : FieldReliefOptions)
    {
        if (option.ordinal == ordinal)
            return option;
    }

    return std::nullopt;
}
