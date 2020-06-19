#ifndef CMEXT_GAME_HPP
#define CMEXT_GAME_HPP

#include "../town.h"

#include "cm_event.hpp"

namespace citymania {

/* BEGIN CMClient growth tiles */
enum class TownGrowthTileState : uint8 {
    NONE = 0,
    RH_REMOVED,
    NEW_HOUSE,
    RH_REBUILT,               // rebuilt and removed houses are also
    CS,
    HS,
    HR
};

class Game {
public:
    event::Dispatcher events;

    typedef std::map<TileIndex, TownGrowthTileState> TownsGrowthTilesIndex;
    TownsGrowthTilesIndex towns_growth_tiles_last_month;
    TownsGrowthTilesIndex towns_growth_tiles;

    Game();

    TownGrowthTileState get_town_growth_tile(TileIndex tile) {
        auto a = this->towns_growth_tiles.find(tile);
        auto b = this->towns_growth_tiles_last_month.find(tile);
        auto as = (a == this->towns_growth_tiles.end() ? TownGrowthTileState::NONE : (*a).second);
        auto bs = (b == this->towns_growth_tiles.end() ? TownGrowthTileState::NONE : (*b).second);
        return max(as, bs);
    }

    void rebuild_town_caches() {
        for (Town *town : Town::Iterate()) {
            town->cm.houses_constructing = 0;
            town->cache.potential_pop = 0;
        }
        for (TileIndex t = 0; t < MapSize(); t++) {
            if (!IsTileType(t, MP_HOUSE)) continue;
            Town *town = Town::GetByTile(t);
            if (!IsHouseCompleted(t))
                town->cm.houses_constructing++;
            HouseID house_id = GetHouseType(t);
            town->cache.potential_pop += HouseSpec::Get(house_id)->population;
        }
    }
};

} // namespace citymania

#endif
