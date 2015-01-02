/* $Id: cargo_table_gui.cpp 21909 2011-01-26 08:14:36Z TheDude $ */

#include "stdafx.h"
#include "window_gui.h"
#include "window_func.h"
#include "strings_func.h"
#include "company_func.h"
#include "company_base.h"
#include "table/strings.h"
#include "textbuf_gui.h"
#include "cargotype.h"
#include "widgets/dropdown_type.h"

#include "widgets/cargo_table_widget.h"

static const uint EXP_TOPPADDING = 5;
static const uint EXP_LINESPACE  = 2;      ///< Amount of vertical space for a horizontal (sub-)total line.
static const uint EXP_BLOCKSPACE = 10;     ///< Amount of vertical space between two blocks of numbers.

enum CargoOption {
	WID_CT_OPTION_CARGO_TOTAL = 0,
	WID_CT_OPTION_CARGO_MONTH,
};

static void DrawPrice(Money amount, int left, int right, int top)
{
	SetDParam(0, amount);
	DrawString(left, right, top, STR_FINANCES_POSITIVE_INCOME, TC_FROMSTRING, SA_RIGHT);
}

void InvalidateCargosWindows(CompanyID cid)
{
	if (cid == _local_company) SetWindowDirty(WC_STATUS_BAR, 0);
	SetWindowDirty(WC_CARGOS, cid);
}

/** Cargos window handler. */
struct CargosWindow : Window {

	CargoOption cargoPeriod;
	CargosWindow(WindowDesc *desc, WindowNumber window_number) : Window(desc)
	{
		this->InitNested(window_number);
		this->owner = (Owner)this->window_number;
		this->cargoPeriod = WID_CT_OPTION_CARGO_TOTAL;
	}

	virtual void SetStringParameters(int widget) const
	{
		if(widget != WID_CT_CAPTION) return;
		SetDParam(0, (CompanyID)this->window_number);
		SetDParam(1, (CompanyID)this->window_number);
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		if(widget != WID_CT_HEADER_CARGO) return;
		this->cargoPeriod = (this->cargoPeriod == WID_CT_OPTION_CARGO_TOTAL) ? WID_CT_OPTION_CARGO_MONTH : WID_CT_OPTION_CARGO_TOTAL;
		this->SetDirty();
	}

	void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		uint extra_width = 0;
		switch(widget){
			case WID_CT_HEADER_AMOUNT:
			case WID_CT_HEADER_INCOME:
				extra_width += 16;
			case WID_CT_HEADER_CARGO:
				size->width = 96 + extra_width;
				size->height = EXP_BLOCKSPACE + EXP_LINESPACE;
				break;

			case WID_CT_AMOUNT:
			case WID_CT_INCOME:
				extra_width += 16;
			case WID_CT_LIST:
				size->width = 96 + extra_width;
				size->height = (_sorted_standard_cargo_specs_size + 3) * (EXP_BLOCKSPACE + EXP_LINESPACE);
				break;
		}
	}

	void DrawWidget(const Rect &r, int widget) const
	{
		int rect_x = (r.left + WD_FRAMERECT_LEFT);
		int y = r.top;
		const Company *c = Company::Get((CompanyID)this->window_number);
		uint32 sum_cargo_amount = 0;
		Money sum_cargo_income = 0;

		switch(widget){
			case WID_CT_HEADER_CARGO:
				//DrawString(r.left, r.right, y, STR_TOOLBAR_CARGOS_HEADER_CARGO, TC_FROMSTRING, SA_LEFT);
				y += EXP_BLOCKSPACE + EXP_LINESPACE;
				break;
			case WID_CT_HEADER_AMOUNT:
				DrawString(r.left, r.right, y, STR_TOOLBAR_CARGOS_HEADER_AMOUNT, TC_FROMSTRING, SA_CENTER);
				y += EXP_BLOCKSPACE + EXP_LINESPACE;
				break;
			case WID_CT_HEADER_INCOME:
				DrawString(r.left, r.right, y, STR_TOOLBAR_CARGOS_HEADER_INCOME, TC_FROMSTRING, SA_RIGHT);
				y += EXP_BLOCKSPACE + EXP_LINESPACE;
				break;

			case WID_CT_LIST:{
				y += EXP_TOPPADDING; //top padding
				for (int i = 0; i < _sorted_standard_cargo_specs_size; i++) {
					const CargoSpec *cs = _sorted_cargo_specs[i];

					GfxFillRect(rect_x, y, rect_x + 8, y + 5, 0);
					GfxFillRect(rect_x + 1, y + 1, rect_x + 7, y + 4, cs->legend_colour); //coloured cargo rectangles

					SetDParam(0, cs->name);
					DrawString(r.left + 14, r.right, y, STR_TOOLBAR_CARGOS_NAME); //cargo name

					y += EXP_BLOCKSPACE + EXP_LINESPACE;	//padding
				}

				//total
				GfxFillRect(rect_x, y, rect_x + 96, y, 0);
				y += EXP_BLOCKSPACE + EXP_LINESPACE;

				StringID string_to_draw = STR_TOOLBAR_CARGOS_HEADER_TOTAL;
				if(this->cargoPeriod != WID_CT_OPTION_CARGO_TOTAL) string_to_draw++;
				DrawString(r.left, r.right, y, string_to_draw);
				break;
			}
			case WID_CT_AMOUNT:
				y += EXP_TOPPADDING;
				for (int i = 0; i < _sorted_standard_cargo_specs_size; i++) {
					const CargoSpec *cs = _sorted_cargo_specs[i];

					if(this->cargoPeriod == WID_CT_OPTION_CARGO_MONTH){
						sum_cargo_amount += c->cargo_units_period[0][cs->Index()];
						SetDParam(0,  c->cargo_units_period[0][cs->Index()]);
					}
					else{
						sum_cargo_amount += c->cargo_units[cs->Index()];
						SetDParam(0,  c->cargo_units[cs->Index()]);
					}

					DrawString(r.left, r.right, y, STR_TOOLBAR_CARGOS_UNITS, TC_FROMSTRING, SA_RIGHT); //cargo amount in pcs
					y += EXP_BLOCKSPACE + EXP_LINESPACE;
				}

				//total
				GfxFillRect(rect_x, y, rect_x + 108, y, 0);
				y += EXP_BLOCKSPACE + EXP_LINESPACE;
				SetDParam(0, sum_cargo_amount);
				DrawString(r.left, r.right, y, STR_TOOLBAR_CARGOS_UNITS_TOTAL, TC_FROMSTRING, SA_RIGHT);
				break;

			case WID_CT_INCOME:
				y += EXP_TOPPADDING;
				for (int i = 0; i < _sorted_standard_cargo_specs_size; i++) {
					const CargoSpec *cs = _sorted_cargo_specs[i];

					if(this->cargoPeriod == WID_CT_OPTION_CARGO_MONTH){
						sum_cargo_income += c->cargo_income_period[0][cs->Index()];
						DrawPrice(c->cargo_income_period[0][cs->Index()], r.left, r.right, y); //cargo income in money
					}
					else{
						sum_cargo_income += c->cargo_income[cs->Index()];
						DrawPrice(c->cargo_income[cs->Index()], r.left, r.right, y); //cargo income in money
					}

					y += EXP_BLOCKSPACE + EXP_LINESPACE;
				}

				//total
				GfxFillRect(rect_x, y, rect_x + 108, y, 0);
				y += EXP_BLOCKSPACE + EXP_LINESPACE;
				DrawPrice(sum_cargo_income, r.left, r.right, y);
				break;
		}
	}
};

static const NWidgetPart _nested_cargos_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_CT_CAPTION), SetDataTip(STR_TOOLBAR_CARGOS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY), SetResize(1, 1),
		NWidget(NWID_HORIZONTAL), SetPadding(WD_FRAMERECT_TOP, WD_FRAMERECT_RIGHT, WD_FRAMERECT_BOTTOM, WD_FRAMERECT_LEFT), SetPIP(0, 9, 0),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_CT_HEADER_CARGO), SetMinimalSize(96, 16),SetFill(1, 0), SetPadding(2,2,2,2), SetDataTip(STR_TOOLBAR_CARGOS_HEADER_CARGO, STR_TOOLBAR_CARGOS_HEADER_CARGO),
			NWidget(WWT_EMPTY, COLOUR_GREY, WID_CT_HEADER_AMOUNT), SetMinimalSize(108, 16), SetFill(1, 0), SetPadding(2,2,2,2), SetDataTip(STR_TOOLBAR_CARGOS_HEADER_AMOUNT, STR_TOOLBAR_CARGOS_HEADER_AMOUNT),
			NWidget(WWT_EMPTY, COLOUR_GREY, WID_CT_HEADER_INCOME), SetMinimalSize(108, 16), SetFill(1, 0), SetPadding(2,2,2,2), SetDataTip(STR_TOOLBAR_CARGOS_HEADER_INCOME, STR_TOOLBAR_CARGOS_HEADER_INCOME),
		EndContainer(),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY), SetResize(1, 1),
		NWidget(NWID_HORIZONTAL), SetPadding(WD_FRAMERECT_TOP, WD_FRAMERECT_RIGHT, WD_FRAMERECT_BOTTOM, WD_FRAMERECT_LEFT), SetPIP(0, 9, 0),
			NWidget(WWT_EMPTY, COLOUR_GREY, WID_CT_LIST),SetMinimalSize(96, 0),SetFill(1, 0), SetPadding(2,2,2,2), SetResize(1, 1),
			NWidget(WWT_EMPTY, COLOUR_GREY, WID_CT_AMOUNT),SetMinimalSize(108, 0),SetFill(1, 0), SetPadding(2,2,2,2), SetResize(1, 1),
			NWidget(WWT_EMPTY, COLOUR_GREY, WID_CT_INCOME),SetMinimalSize(108, 0),SetFill(1, 0), SetPadding(2,2,2,2), SetResize(1, 1),
			NWidget(NWID_VERTICAL),
				NWidget(NWID_SPACER), SetFill(0, 1), SetResize(0, 1),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _cargos_desc(
	WDP_AUTO, NULL, 0, 0,
	WC_CARGOS, WC_NONE,
	WDF_CONSTRUCTION,
	_nested_cargos_widgets, lengthof(_nested_cargos_widgets)
);

void ShowCompanyCargos(CompanyID company)
{
	if (!Company::IsValidID(company)) return;
	AllocateWindowDescFront<CargosWindow>(&_cargos_desc, company);
}

