/** @file cm_cargo_table_gui.hpp CityMania cargo table GUI. */

#ifndef CM_CARGO_TABLE_GUI_HPP
#define CM_CARGO_TABLE_GUI_HPP

#include "../company_type.h"

namespace citymania {

void ShowCompanyCargos(CompanyID company);
void InvalidateCargosWindows(CompanyID cid);

} // namespace citymania

#endif /* CM_CARGO_TABLE_GUI_HPP */
