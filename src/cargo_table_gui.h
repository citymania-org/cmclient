/* $Id: cargo_table_gui.h 21700 2011-01-03 11:55:08Z  $ */

/** @file cargo_table_gui.h GUI Functions related to cargos. */

#ifndef CARGO_TABLE_H
#define CARGO_TABLE_H

#include "company_type.h"
#include "gfx_type.h"

void ShowCompanyCargos(CompanyID company);
void InvalidateCargosWindows(CompanyID cid);

#endif /* CARGO_TABLE_H */
