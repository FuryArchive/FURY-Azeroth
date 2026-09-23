#ifndef MOD_FURY_EVENT_FACTORY_H
#define MOD_FURY_EVENT_FACTORY_H

#include "FuryEvent.h"

#include <string_view>

class Creature;
class Item;
class Player;
class Quest;

namespace Fury
{
struct LivingWorldEntityMetadata;
class FuryEventFactory final
{
public:
    static FuryEvent PlayerLogin(Player* player);
    static FuryEvent PlayerLogout(Player* player);
    static FuryEvent LevelChanged(Player* player, uint8 oldLevel);
    static FuryEvent ZoneChanged(Player* player, uint32 newZone, uint32 newArea);
    static FuryEvent QuestCompleted(Player* player, Quest const* quest);
    static FuryEvent CreatureKilled(Player* player, Creature* creature, bool viaPet);
    static FuryEvent LivingWorldEntityKilled(
        Player* player,
        Creature* creature,
        bool viaPet,
        LivingWorldEntityMetadata const& metadata);
    static FuryEvent ItemLooted(
        Player* player,
        Item* item,
        uint32 count,
        ObjectGuid lootGuid);
    static FuryEvent ItemCreated(Player* player, Item* item, uint32 count);
    static FuryEvent ProfessionCrafted(
        Player* player,
        uint32 skillId,
        uint32 recipeSpellId,
        uint32 itemId,
        uint32 unitOrdinal,
        uint32 producedCount);

private:
    static FuryEvent Base(Player* player, std::string type);
    static std::string OccurrenceIdentity(
        std::string_view scope,
        Player* player);
};
}

#endif
