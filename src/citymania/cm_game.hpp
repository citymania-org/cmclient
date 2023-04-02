#ifndef CM_GAME_HPP
#define CM_GAME_HPP

#include "../town.h"

#include "cm_event.hpp"

namespace citymania {

class Game {
protected:
    TownsGrowthTilesIndex towns_growth_tiles_last_month;
    TownsGrowthTilesIndex towns_growth_tiles;
    uint64 start_countdown = 0;

public:
    event::Dispatcher events;

    Game();
    void set_town_growth_tile(Town *town, TileIndex tile, TownGrowthTileState state);
    
    TownGrowthTileState get_town_growth_tile(TileIndex tile) {
        auto a = this->towns_growth_tiles.find(tile);
        auto b = this->towns_growth_tiles_last_month.find(tile);
        auto as = (a == this->towns_growth_tiles.end() ? TownGrowthTileState::NONE : (*a).second);
        auto bs = (b == this->towns_growth_tiles_last_month.end() ? TownGrowthTileState::NONE : (*b).second);
        return std::max(as, bs);
    }
};

} // namespace citymania

#endif
