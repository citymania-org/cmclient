#include "../stdafx.h"

#include "cm_cargo_table_gui.hpp"

#include "../window_gui.h"
#include "../window_func.h"
#include "../strings_func.h"
#include "../company_func.h"
#include "../company_base.h"
#include "../table/strings.h"
#include "../table/sprites.h"
#include "../textbuf_gui.h"
#include "../cargotype.h"
#include "../zoom_func.h"
#include "../dropdown_type.h"

#include "../widgets/cargo_table_widget.h"

namespace citymania {

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
	CargosWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc) {
		this->InitNested(window_number);
		this->owner = (Owner)this->window_number;
		this->cargoPeriod = WID_CT_OPTION_CARGO_TOTAL;
	}

	void SetStringParameters(int widget) const override {
		if(widget != WID_CT_CAPTION) return;
		SetDParam(0, (CompanyID)this->window_number);
		SetDParam(1, (CompanyID)this->window_number);
	}

	void OnClick([[maybe_unused]] Point pt, int widget, [[maybe_unused]] int click_count) override {
		if(widget != WID_CT_HEADER_CARGO) return;
		this->cargoPeriod = (this->cargoPeriod == WID_CT_OPTION_CARGO_TOTAL) ? WID_CT_OPTION_CARGO_MONTH : WID_CT_OPTION_CARGO_TOTAL;
		this->SetDirty();
	}

	virtual void UpdateWidgetSize([[maybe_unused]] WidgetID widget, [[maybe_unused]] Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override {
		Dimension icon_size = this->GetMaxIconSize();
		int line_height = std::max(GetCharacterHeight(FS_NORMAL), (int)icon_size.height);
		int icon_space = icon_size.width + ScaleGUITrad(CT_ICON_MARGIN);
		switch(widget) {
			case WID_CT_HEADER_AMOUNT:
			case WID_CT_HEADER_INCOME:
			case WID_CT_AMOUNT:
			case WID_CT_INCOME:
				break;
			case WID_CT_HEADER_CARGO:
			case WID_CT_LIST:
				for (const CargoSpec *cs : _sorted_standard_cargo_specs) {
					size.width = std::max(GetStringBoundingBox(cs->name).width + icon_space, size.width);
				}
				size.width = std::max(GetStringBoundingBox(CM_STR_TOOLBAR_CARGOS_HEADER_TOTAL_MONTH).width, size.width);
				break;
		}
		switch(widget) {
			case WID_CT_HEADER_AMOUNT:
			case WID_CT_HEADER_INCOME:
				size.height = GetCharacterHeight(FS_NORMAL);
				break;
			case WID_CT_HEADER_CARGO:
				break;
			case WID_CT_AMOUNT:
			case WID_CT_INCOME:
			case WID_CT_LIST:
				size.height = _sorted_standard_cargo_specs.size() * line_height + CT_LINESPACE + GetCharacterHeight(FS_NORMAL);
				break;
		}
		size.width += padding.width;
		size.height += padding.height;
	}

	Dimension GetMaxIconSize() const {
		Dimension size = {0, 0};
		for (const CargoSpec *cs : _sorted_standard_cargo_specs) {
			Dimension icon_size = GetSpriteSize(cs->GetCargoIcon());
			size.width = std::max(size.width, icon_size.width);
			size.height = std::max(size.height, icon_size.height);
		}
		return size;
	}

	void DrawWidget(const Rect &r, int widget) const override {
		const Company *c = Company::Get((CompanyID)this->window_number);
		uint32 sum_cargo_amount = 0;
		Money sum_cargo_income = 0;
		int y = r.top;
		Dimension max_icon_size = this->GetMaxIconSize();
		int line_height = std::max(GetCharacterHeight(FS_NORMAL), (int)(max_icon_size.height));
		int icon_space = max_icon_size.width + ScaleGUITrad(CT_ICON_MARGIN);
		int text_y_ofs = (line_height - GetCharacterHeight(FS_NORMAL));

		switch(widget){
			case WID_CT_HEADER_CARGO:
				break;
			case WID_CT_HEADER_AMOUNT:
				DrawString(r.left, r.right, y, CM_STR_TOOLBAR_CARGOS_HEADER_AMOUNT, TC_FROMSTRING, SA_RIGHT);
				break;
			case WID_CT_HEADER_INCOME:
				DrawString(r.left, r.right, y, CM_STR_TOOLBAR_CARGOS_HEADER_INCOME, TC_FROMSTRING, SA_RIGHT);
				break;

			case WID_CT_LIST: {
				int rect_x = r.left + WidgetDimensions::scaled.framerect.left;
				for (const CargoSpec *cs : _sorted_standard_cargo_specs) {
					Dimension icon_size = GetSpriteSize(cs->GetCargoIcon());
					DrawSprite(cs->GetCargoIcon(), PAL_NONE,
					           r.left + max_icon_size.width - icon_size.width,
					           y + (line_height - (int)icon_size.height) / 2);

					SetDParam(0, cs->name);
					DrawString(rect_x + icon_space, r.right, y + text_y_ofs, CM_STR_TOOLBAR_CARGOS_NAME);

					y += line_height;
				}

				GfxFillRect(r.left, y + 1, r.right, y + 1, PC_BLACK);
				y += CT_LINESPACE;

				StringID string_to_draw = CM_STR_TOOLBAR_CARGOS_HEADER_TOTAL;
				if (this->cargoPeriod != WID_CT_OPTION_CARGO_TOTAL) string_to_draw++;
				DrawString(r.left, r.right, y, string_to_draw);
				break;
			}
			case WID_CT_AMOUNT:
				for (const CargoSpec *cs : _sorted_standard_cargo_specs) {
					auto &economy = (this->cargoPeriod == WID_CT_OPTION_CARGO_MONTH ? c->old_economy[0] : c->cur_economy);
					sum_cargo_amount += economy.delivered_cargo[cs->Index()];
					SetDParam(0,  economy.delivered_cargo[cs->Index()]);

					DrawString(r.left, r.right, y + text_y_ofs, CM_STR_TOOLBAR_CARGOS_UNITS, TC_FROMSTRING, SA_RIGHT); //cargo amount in pcs
					y += line_height;
				}

				GfxFillRect(r.left, y + 1, r.right, y + 1, PC_BLACK);
				y += CT_LINESPACE;
				SetDParam(0, sum_cargo_amount);
				DrawString(r.left, r.right, y, CM_STR_TOOLBAR_CARGOS_UNITS_TOTAL, TC_FROMSTRING, SA_RIGHT);
				break;

			case WID_CT_INCOME:
				for (const CargoSpec *cs : _sorted_standard_cargo_specs) {
					auto &economy = (this->cargoPeriod == WID_CT_OPTION_CARGO_MONTH ? c->old_economy[0] : c->cur_economy);

					sum_cargo_income += economy.cm.cargo_income[cs->Index()];
					DrawPrice(economy.cm.cargo_income[cs->Index()], r.left, r.right, y + text_y_ofs);

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
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_CT_CAPTION), SetDataTip(CM_STR_TOOLBAR_CARGOS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY), SetResize(1, 1),
		NWidget(NWID_HORIZONTAL), SetPadding(WidgetDimensions::scaled.framerect.top, WidgetDimensions::scaled.framerect.right, WidgetDimensions::scaled.framerect.bottom, WidgetDimensions::scaled.framerect.left), SetPIP(0, 9, 0),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_CT_HEADER_CARGO), SetFill(1, 0), SetPadding(2,2,2,2), SetDataTip(CM_STR_TOOLBAR_CARGOS_HEADER_CARGO, CM_STR_TOOLBAR_CARGOS_HEADER_CARGO),
			NWidget(WWT_TEXT, COLOUR_GREY, WID_CT_HEADER_AMOUNT), SetMinimalSize(108, 16), SetFill(1, 0), SetPadding(2,2,2,2), SetDataTip(STR_NULL, STR_NULL),
			NWidget(WWT_TEXT, COLOUR_GREY, WID_CT_HEADER_INCOME), SetMinimalSize(108, 16), SetFill(1, 0), SetPadding(2,2,2,2), SetDataTip(STR_NULL, STR_NULL),
		EndContainer(),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY), SetResize(1, 1),
		NWidget(NWID_HORIZONTAL), SetPadding(WidgetDimensions::scaled.framerect.top, WidgetDimensions::scaled.framerect.right, WidgetDimensions::scaled.framerect.bottom, WidgetDimensions::scaled.framerect.left), SetPIP(0, 9, 0),
			NWidget(WWT_EMPTY, COLOUR_GREY, WID_CT_LIST),SetFill(1, 0), SetPadding(2,2,2,2), SetResize(1, 1),
			NWidget(WWT_EMPTY, COLOUR_GREY, WID_CT_AMOUNT),SetMinimalSize(108, 0),SetFill(1, 0), SetPadding(2,2,2,2), SetResize(1, 1),
			NWidget(WWT_EMPTY, COLOUR_GREY, WID_CT_INCOME),SetMinimalSize(108, 0),SetFill(1, 0), SetPadding(2,2,2,2), SetResize(1, 1),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _cargos_desc(
	WDP_AUTO, "cm_cargo_table", 0, 0,
	WC_CARGOS, WC_NONE,
	WDF_CONSTRUCTION,
	_nested_cargos_widgets
);

void ShowCompanyCargos(CompanyID company)
{
	if (!Company::IsValidID(company)) return;
	AllocateWindowDescFront<CargosWindow>(_cargos_desc, company);
}

} // namespace citymania
