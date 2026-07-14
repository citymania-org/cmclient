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

enum class TownGrowthState: uint8 {
    NOT_GROWING = 0,
    GROWING = 1,
    SHRINKING = 2,
};

namespace ext {

struct CBTownInfo {
    uint32_t pax_delivered = 0;
    uint32_t mail_delivered = 0;
    uint32_t pax_delivered_last_month = 0;
    uint32_t mail_delivered_last_month = 0;
    TownGrowthState growth_state = TownGrowthState::NOT_GROWING;
    uint8_t shrink_effeciency = 0;
    uint16_t shrink_rate = 0;
    uint16_t shrink_counter = 0;
    std::array<uint32_t, NUM_CARGO> stored {};
    std::array<uint32_t, NUM_CARGO> delivered = {};
    std::array<uint32_t, NUM_CARGO> required = {};
    std::array<uint32_t, NUM_CARGO> delivered_last_month = {};
    std::array<uint32_t, NUM_CARGO> required_last_month = {};
};

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

    TownsGrowthTilesIndex growth_tiles_last_month{};
    TownsGrowthTilesIndex growth_tiles{};
    CBTownInfo cb{};
    std::optional<std::pair<StationID, CargoType>> ad_ref_goods_entry = std::nullopt;      ///< poiter to goods entry of some station, used to check rating for regular advertisement

    int town_label = 0;                ///< Label dependent on _local_company rating.
    CompanyMask fund_regularly{};          ///< funds buildings regularly when previous fund ends
    CompanyMask do_powerfund{};            ///< funds buildings when grow counter is maximal (results in fastest funding possible)
    uint32 last_funding = 0;             ///< when town was funded the last time
    CompanyMask advertise_regularly{};     ///< advertised regularly to keep stations rating on desired value
    uint32 last_advertisement = 0;
    uint8 ad_rating_goal = 0;                ///< value to keep rating at (for regular advertisement) (0..255)
};

} // namespace ext

} // namespace citymania

#endif
