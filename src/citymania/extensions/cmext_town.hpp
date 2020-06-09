#ifndef CMEXT_TOWN_HPP
#define CMEXT_TOWN_HPP

namespace citymania {

namespace ext {

class Town {
public:
    uint32 hs_total = 0;              ///< number of skipped house buildings (HS) in total
    uint32 hs_total_prev = 0;         ///< number of skipped house buildings (HS) in total at the end of last month
    uint32 hs_last_month = 0;         ///< number of skipped house buildings (HS) during last month
    uint32 cs_total = 0;              ///< number of skipped growth cycles (CS) in total
    uint32 cs_total_prev = 0;         ///< number of skipped growth cycles (CS) in total at the end of last month
    uint32 cs_last_month = 0;         ///< number of skipped growth cycles (CS) during last month
    uint32 hr_total = 0;              ///< number of houses removed by the server (HR) in total
    uint32 hr_total_prev = 0;         ///< number of houses removed by the server (HR) in total at the end of last month
    uint32 hr_last_month = 0;         ///< number of houses removed by the server (HR) during last month
};

} // namespace citymania

} // namespace citymania

#endif
