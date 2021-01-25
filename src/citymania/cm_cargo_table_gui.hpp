/** @file cargo_table_gui.h GUI Functions related to cargos. */

#ifndef CM_CARGO_TABLE_HPP
#define CM_CARGO_TABLE_HPP

#include "company_type.h"

namespace citymania {

void ShowCompanyCargos(CompanyID company);
void InvalidateCargosWindows(CompanyID cid);

} // namespace citymania

#endif /* CARGO_TABLE_H */
