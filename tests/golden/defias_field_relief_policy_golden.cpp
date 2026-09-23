#include "content/defias/DefiasFieldRelief.h"

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

    std::array<uint16, FieldReliefOptions.size()> skills{};

    Require(SelectFieldReliefOrdinal(skills) == 0,
        "no supported profession yields no Field Relief path");

    skills[6] = 1;
    Require(SelectFieldReliefOrdinal(skills) == 7,
        "Engineering alone selects Rough Blasting Powder");

    skills[4] = 1;
    Require(SelectFieldReliefOrdinal(skills) == 5,
        "Blacksmithing outranks Engineering deterministically");

    skills[0] = 1;
    Require(SelectFieldReliefOrdinal(skills) == 1,
        "First Aid is the preferred relief path when available");

    Require(FieldReliefOptions[0].skillId == 129 &&
            FieldReliefOptions[0].itemId == 1251,
        "First Aid path uses Linen Bandage");
    Require(FieldReliefOptions[1].skillId == 171 &&
            FieldReliefOptions[1].itemId == 118,
        "Alchemy path uses Minor Healing Potion");
    Require(FieldReliefOptions[2].skillId == 185 &&
            FieldReliefOptions[2].itemId == 2679,
        "Cooking path uses Charred Wolf Meat");
    Require(FieldReliefOptions[3].skillId == 165 &&
            FieldReliefOptions[3].itemId == 2304,
        "Leatherworking path uses Light Armor Kit");
    Require(FieldReliefOptions[4].skillId == 164 &&
            FieldReliefOptions[4].itemId == 2862,
        "Blacksmithing path uses Rough Sharpening Stone");
    Require(FieldReliefOptions[5].skillId == 197 &&
            FieldReliefOptions[5].itemId == 2996,
        "Tailoring path uses Bolt of Linen Cloth");
    Require(FieldReliefOptions[6].skillId == 202 &&
            FieldReliefOptions[6].itemId == 4357,
        "Engineering path uses Rough Blasting Powder");

    std::cout << "[FURY][PASS] T31 Field Relief policy gate passed\n";
    return 0;
}
