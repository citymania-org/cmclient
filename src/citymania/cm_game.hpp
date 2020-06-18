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
protected:
    typedef std::map<TileIndex, TownGrowthTileState> TownsGrowthTilesIndex;
    TownsGrowthTilesIndex towns_growth_tiles_last_month;
    TownsGrowthTilesIndex towns_growth_tiles;

public:
    event::Dispatcher events;

    Game();
};

} // namespace citymania

#endif
