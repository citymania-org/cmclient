#ifndef CMEXT_COMPANY_HPP
#define CMEXT_COMPANY_HPP

#include "../../cargo_type.h"

namespace citymania {

namespace ext {
class CompanyEconomyEntry {
public:
    Money cargo_income[NUM_CARGO]; ///< Cargo income from each cargo type
};

} // namespace ext

} // namespace citymania

#endif
