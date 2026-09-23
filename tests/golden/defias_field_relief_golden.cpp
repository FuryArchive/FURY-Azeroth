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

    Require(!SelectFieldReliefChoice(0, 0, 0),
        "no supported skill leaves Field Relief optional");

    auto alchemy = SelectFieldReliefChoice(1, 0, 0);
    Require(alchemy.optionOrdinal == FieldReliefAlchemyOption,
        "Alchemy selects potion option");
    Require(alchemy.skillId == 171 && alchemy.itemId == 118 &&
            alchemy.requiredCount == 3,
        "Alchemy target is Minor Healing Potion");

    auto firstAid = SelectFieldReliefChoice(0, 1, 0);
    Require(firstAid.optionOrdinal == FieldReliefFirstAidOption,
        "First Aid selects bandage option");
    Require(firstAid.skillId == 129 && firstAid.itemId == 1251 &&
            firstAid.requiredCount == 6,
        "First Aid target is Linen Bandage");

    auto cooking = SelectFieldReliefChoice(0, 0, 1);
    Require(cooking.optionOrdinal == FieldReliefCookingOption,
        "Cooking selects ration option");
    Require(cooking.skillId == 185 && cooking.itemId == 2681 &&
            cooking.requiredCount == 6,
        "Cooking target is Roasted Boar Meat");

    Require(SelectFieldReliefChoice(40, 75, 25).optionOrdinal ==
            FieldReliefFirstAidOption,
        "highest supported skill wins");
    Require(SelectFieldReliefChoice(50, 50, 50).optionOrdinal ==
            FieldReliefAlchemyOption,
        "ties resolve deterministically");

    std::cout << "[FURY][PASS] T31 Field Relief policy gate passed\n";
    return 0;
}
