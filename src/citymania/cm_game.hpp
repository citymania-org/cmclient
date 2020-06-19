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
};

} // namespace citymania

#endif
