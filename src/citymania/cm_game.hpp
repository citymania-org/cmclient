#ifndef CM_GAME_HPP
#define CM_GAME_HPP

#include "../town.h"

#include "cm_event.hpp"

namespace citymania {

class Game {
protected:
    TownsGrowthTilesIndex towns_growth_tiles_last_month;
    TownsGrowthTilesIndex towns_growth_tiles;

public:
    event::Dispatcher events;

    Game();
    void set_town_growth_tile(Town *town, TileIndex tile, TownGrowthTileState state);
};

} // namespace citymania

#endif
