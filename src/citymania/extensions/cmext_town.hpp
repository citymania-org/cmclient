#ifndef CMEXT_TOWN_HPP
#define CMEXT_TOWN_HPP

#include <map>

namespace citymania {

enum class TownGrowthTileState : uint8_t {
    NONE = 0,
    RH_REMOVED,
    NEW_HOUSE,
    RH_REBUILT,               // rebuilt and removed houses are also
    CS,
    HS,
    HR
};

typedef std::map<TileIndex, TownGrowthTileState> TownsGrowthTilesIndex;

namespace ext {

class Town {
public:
    bool growing_by_chance = false;    ///< whether town is growing due to 1/12 chance
    uint32_t real_population = 0;        ///< population including unfinished houses
    uint32_t hs_total = 0;               ///< number of skipped house buildings (HS) in total
    uint16_t hs_this_month = 0;          ///< number of skipped house buildings (HS) during the current month
    uint16_t hs_last_month = 0;          ///< number of skipped house buildings (HS) during last month
    uint32_t cs_total = 0;               ///< number of skipped growth cycles (CS) in total
    uint16_t cs_this_month = 0;          ///< number of skipped growth cycles (CS) during the current month
    uint16_t cs_last_month = 0;          ///< number of skipped growth cycles (CS) during last month
    uint32_t hr_total = 0;               ///< number of houses removed by the server (HR) in total
    uint16_t hr_this_month = 0;          ///< number of houses removed by the server (HR) during the current month
    uint16_t hr_last_month = 0;          ///< number of houses removed by the server (HR) during last month
    uint16_t houses_constructing = 0;    ///< number of houses currently being built
    uint16_t houses_reconstructed_this_month = 0;  ///< number of houses rebuilt this month
    uint16_t houses_reconstructed_last_month = 0;  ///< number of houses rebuild last month
    uint16_t houses_demolished_this_month = 0; ///< number of houses demolished this month
    uint16_t houses_demolished_last_month = 0; ///< number of houses demolished last month

    TownsGrowthTilesIndex growth_tiles_last_month;
    TownsGrowthTilesIndex growth_tiles;
};

} // namespace ext

} // namespace citymania

#endif
