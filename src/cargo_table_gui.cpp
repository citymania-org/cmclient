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
#include "zoom_func.h"
#include "widgets/dropdown_type.h"

#include "widgets/cargo_table_widget.h"

static const uint CT_LINESPACE  = 3;      ///< Amount of vertical space for a horizontal (sub-)total line.
static const uint CT_ICON_MARGIN = 2;     ///< Amount of space between cargo icon and text

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
		Dimension icon_size = this->GetMaxIconSize();
		int line_height = max(FONT_HEIGHT_NORMAL, (int)icon_size.height);
		int icon_space = icon_size.width + ScaleGUITrad(CT_ICON_MARGIN);
		switch(widget) {
			case WID_CT_HEADER_AMOUNT:
			case WID_CT_HEADER_INCOME:
			case WID_CT_AMOUNT:
			case WID_CT_INCOME: {
				break;
			}
			case WID_CT_HEADER_CARGO:
			case WID_CT_LIST: {
				for (int i = 0; i < _sorted_standard_cargo_specs_size; i++) {
					const CargoSpec *cs = _sorted_cargo_specs[i];
					size->width = max(GetStringBoundingBox(cs->name).width + icon_space, size->width);
				}
				size->width = max(GetStringBoundingBox(STR_TOOLBAR_CARGOS_HEADER_TOTAL_MONTH).width, size->width);
				break;
			}
		}
		switch(widget) {
			case WID_CT_HEADER_AMOUNT:
			case WID_CT_HEADER_INCOME:
				size->height = FONT_HEIGHT_NORMAL;
				break;
			case WID_CT_HEADER_CARGO:
				break;
			case WID_CT_AMOUNT:
			case WID_CT_INCOME:
			case WID_CT_LIST: {
				size->height = _sorted_standard_cargo_specs_size * line_height + CT_LINESPACE + FONT_HEIGHT_NORMAL;
				break;
			}
		}
	}

	Dimension GetMaxIconSize() const {
		const CargoSpec *cs;
		Dimension size = {0, 0};
		FOR_ALL_CARGOSPECS(cs) {
			Dimension icon_size = GetSpriteSize(cs->GetCargoIcon());
			size.width = max(size.width, icon_size.width);
			size.height = max(size.height, icon_size.height);
		}
		return size;
	}

	void DrawWidget(const Rect &r, int widget) const
	{
		const Company *c = Company::Get((CompanyID)this->window_number);
		uint32 sum_cargo_amount = 0;
		Money sum_cargo_income = 0;
		int y = r.top;
		Dimension max_icon_size = this->GetMaxIconSize();
		int line_height = max(FONT_HEIGHT_NORMAL, (int)(max_icon_size.height));
		int icon_space = max_icon_size.width + ScaleGUITrad(CT_ICON_MARGIN);
		int text_y_ofs = (line_height - FONT_HEIGHT_NORMAL);

		switch(widget){
			case WID_CT_HEADER_CARGO:
				break;
			case WID_CT_HEADER_AMOUNT:
				DrawString(r.left, r.right, y, STR_TOOLBAR_CARGOS_HEADER_AMOUNT, TC_FROMSTRING, SA_RIGHT);
				break;
			case WID_CT_HEADER_INCOME:
				DrawString(r.left, r.right, y, STR_TOOLBAR_CARGOS_HEADER_INCOME, TC_FROMSTRING, SA_RIGHT);
				break;

			case WID_CT_LIST: {
				int rect_x = r.left + WD_FRAMERECT_LEFT;
				for (int i = 0; i < _sorted_standard_cargo_specs_size; i++) {
					const CargoSpec *cs = _sorted_cargo_specs[i];
					Dimension icon_size = GetSpriteSize(cs->GetCargoIcon());
					DrawSprite(cs->GetCargoIcon(), PAL_NONE,
					           r.left + max_icon_size.width - icon_size.width,
					           y + (line_height - (int)icon_size.height) / 2);

					SetDParam(0, cs->name);
					DrawString(r.left + icon_space, r.right, y + text_y_ofs, STR_TOOLBAR_CARGOS_NAME);

					y += line_height;
				}

				GfxFillRect(r.left, y + 1, r.right, y + 1, PC_BLACK);
				y += CT_LINESPACE;

				StringID string_to_draw = STR_TOOLBAR_CARGOS_HEADER_TOTAL;
				if (this->cargoPeriod != WID_CT_OPTION_CARGO_TOTAL) string_to_draw++;
				DrawString(r.left, r.right, y, string_to_draw);
				break;
			}
			case WID_CT_AMOUNT:
				for (int i = 0; i < _sorted_standard_cargo_specs_size; i++) {
					const CargoSpec *cs = _sorted_cargo_specs[i];

					if (this->cargoPeriod == WID_CT_OPTION_CARGO_MONTH) {
						sum_cargo_amount += c->cargo_units_period[0][cs->Index()];
						SetDParam(0,  c->cargo_units_period[0][cs->Index()]);
					} else {
						sum_cargo_amount += c->cargo_units[cs->Index()];
						SetDParam(0,  c->cargo_units[cs->Index()]);
					}

					DrawString(r.left, r.right, y + text_y_ofs, STR_TOOLBAR_CARGOS_UNITS, TC_FROMSTRING, SA_RIGHT); //cargo amount in pcs
					y += line_height;
				}

				GfxFillRect(r.left, y + 1, r.right, y + 1, PC_BLACK);
				y += CT_LINESPACE;
				SetDParam(0, sum_cargo_amount);
				DrawString(r.left, r.right, y, STR_TOOLBAR_CARGOS_UNITS_TOTAL, TC_FROMSTRING, SA_RIGHT);
				break;

			case WID_CT_INCOME:
				for (int i = 0; i < _sorted_standard_cargo_specs_size; i++) {
					const CargoSpec *cs = _sorted_cargo_specs[i];

					if (this->cargoPeriod == WID_CT_OPTION_CARGO_MONTH) {
						sum_cargo_income += c->cargo_income_period[0][cs->Index()];
						DrawPrice(c->cargo_income_period[0][cs->Index()], r.left, r.right, y + text_y_ofs);
					} else {
						sum_cargo_income += c->cargo_income[cs->Index()];
						DrawPrice(c->cargo_income[cs->Index()], r.left, r.right, y + text_y_ofs);
					}

					y += line_height;
				}

				GfxFillRect(r.left, y + 1, r.right, y + 1, PC_BLACK);
				y += CT_LINESPACE;
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
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_CT_HEADER_CARGO), SetFill(1, 0), SetPadding(2,2,2,2), SetDataTip(STR_TOOLBAR_CARGOS_HEADER_CARGO, STR_TOOLBAR_CARGOS_HEADER_CARGO),
			NWidget(WWT_TEXT, COLOUR_GREY, WID_CT_HEADER_AMOUNT), SetMinimalSize(108, 16), SetFill(1, 0), SetPadding(2,2,2,2), SetDataTip(STR_NULL, STR_NULL),
			NWidget(WWT_TEXT, COLOUR_GREY, WID_CT_HEADER_INCOME), SetMinimalSize(108, 16), SetFill(1, 0), SetPadding(2,2,2,2), SetDataTip(STR_NULL, STR_NULL),
			// NWidget(NWID_VERTICAL),
			// 	NWidget(NWID_SPACER), SetFill(0, 1), SetResize(0, 1),
			// EndContainer(),
		EndContainer(),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY), SetResize(1, 1),
		NWidget(NWID_HORIZONTAL), SetPadding(WD_FRAMERECT_TOP, WD_FRAMERECT_RIGHT, WD_FRAMERECT_BOTTOM, WD_FRAMERECT_LEFT), SetPIP(0, 9, 0),
			NWidget(WWT_EMPTY, COLOUR_GREY, WID_CT_LIST),SetFill(1, 0), SetPadding(2,2,2,2), SetResize(1, 1),
			NWidget(WWT_EMPTY, COLOUR_GREY, WID_CT_AMOUNT),SetMinimalSize(108, 0),SetFill(1, 0), SetPadding(2,2,2,2), SetResize(1, 1),
			NWidget(WWT_EMPTY, COLOUR_GREY, WID_CT_INCOME),SetMinimalSize(108, 0),SetFill(1, 0), SetPadding(2,2,2,2), SetResize(1, 1),
			// NWidget(NWID_VERTICAL),
			// 	NWidget(NWID_SPACER), SetFill(0, 1), SetResize(0, 1),
			// EndContainer(),
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

