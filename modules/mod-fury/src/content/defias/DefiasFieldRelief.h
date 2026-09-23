#ifndef MOD_FURY_DEFIAS_FIELD_RELIEF_H
#define MOD_FURY_DEFIAS_FIELD_RELIEF_H

#include "Define.h"

namespace Fury::Defias
{
inline constexpr char FieldReliefOrderKey[] =
    "classic.westfall.defias.field_relief";

inline constexpr uint32 FieldReliefSignalId = 1;

inline constexpr uint32 FieldReliefAlchemySkill = 171;
inline constexpr uint32 FieldReliefFirstAidSkill = 129;
inline constexpr uint32 FieldReliefCookingSkill = 185;

inline constexpr uint32 FieldReliefMinorHealingPotion = 118;
inline constexpr uint32 FieldReliefLinenBandage = 1251;
inline constexpr uint32 FieldReliefRoastedBoarMeat = 2681;

inline constexpr uint16 FieldReliefAlchemyOption = 1;
inline constexpr uint16 FieldReliefFirstAidOption = 2;
inline constexpr uint16 FieldReliefCookingOption = 3;

struct FieldReliefChoice
{
    uint16 optionOrdinal = 0;
    uint32 skillId = 0;
    uint32 itemId = 0;
    uint32 requiredCount = 0;

    [[nodiscard]] constexpr explicit operator bool() const
    {
        return optionOrdinal != 0;
    }
};

[[nodiscard]] constexpr FieldReliefChoice SelectFieldReliefChoice(
    uint32 alchemy,
    uint32 firstAid,
    uint32 cooking)
{
    FieldReliefChoice choice;
    uint32 bestSkill = 0;

    if (alchemy > bestSkill)
    {
        bestSkill = alchemy;
        choice = {
            FieldReliefAlchemyOption,
            FieldReliefAlchemySkill,
            FieldReliefMinorHealingPotion,
            3
        };
    }

    if (firstAid > bestSkill)
    {
        bestSkill = firstAid;
        choice = {
            FieldReliefFirstAidOption,
            FieldReliefFirstAidSkill,
            FieldReliefLinenBandage,
            6
        };
    }

    if (cooking > bestSkill)
    {
        choice = {
            FieldReliefCookingOption,
            FieldReliefCookingSkill,
            FieldReliefRoastedBoarMeat,
            6
        };
    }

    return choice;
}
}

#endif
