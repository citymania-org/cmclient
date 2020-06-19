#ifndef CMEXT_TOWN_HPP
#define CMEXT_TOWN_HPP

namespace citymania {

namespace ext {

class Town {
public:
    bool growing_by_chance = false;    ///< whether town is growing due to 1/12 chance
    uint32 real_population = 0;        ///< population including unfinished houses
    uint32 hs_total = 0;               ///< number of skipped house buildings (HS) in total
    uint32 hs_total_prev = 0;          ///< number of skipped house buildings (HS) in total at the end of last month
    uint32 hs_last_month = 0;          ///< number of skipped house buildings (HS) during last month
    uint32 cs_total = 0;               ///< number of skipped growth cycles (CS) in total
    uint32 cs_total_prev = 0;          ///< number of skipped growth cycles (CS) in total at the end of last month
    uint32 cs_last_month = 0;          ///< number of skipped growth cycles (CS) during last month
    uint32 hr_total = 0;               ///< number of houses removed by the server (HR) in total
    uint32 hr_total_prev = 0;          ///< number of houses removed by the server (HR) in total at the end of last month
    uint32 hr_last_month = 0;          ///< number of houses removed by the server (HR) during last month
    uint32 houses_constructing = 0;    ///< number of houses currently being built
    uint32 houses_reconstructed_this_month = 0;  ///< number of houses rebuilt this month
    uint32 houses_reconstructed_last_month = 0;  ///< number of houses rebuild last month
    uint32 houses_demolished_this_month = 0; ///< number of houses demolished this month
    uint32 houses_demolished_last_month = 0; ///< number of houses demolished last month

};

} // namespace citymania

} // namespace citymania

#endif
