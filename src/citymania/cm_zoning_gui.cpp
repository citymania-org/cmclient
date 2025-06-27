#include "../stdafx.h"

#include "cm_zoning.hpp"

#include "../gfx_func.h"
#include "../hotkeys.h"
#include "../strings_func.h"
#include "../core/geometry_func.hpp"
#include "../table/strings.h"
#include "../dropdown_func.h"

#include "../safeguards.h"

namespace citymania {

const StringID _zone_types[] = {
	//STR_ZONING_NO_ZONING,
	CM_STR_ZONING_AUTHORITY,
	CM_STR_ZONING_CAN_BUILD,
	CM_STR_ZONING_STA_CATCH,
	CM_STR_ZONING_ACTIVE_STATIONS,
	CM_STR_ZONING_BUL_UNSER,
	CM_STR_ZONING_IND_UNSER,
	CM_STR_ZONING_TOWN_ZONES,
	CM_STR_ZONING_CB_ACCEPTANCE,
	CM_STR_ZONING_CB_TOWN_LIMIT,
	CM_STR_ZONING_ADVERTISEMENT_ZONES,
	CM_STR_ZONING_TOWN_GROWTH_TILES,
};

const int ZONES_COUNT = 11;

enum ZoningToolbarWidgets {
	ZTW_CAPTION,
	ZTW_OUTER_FIRST,
	ZTW_INNER_FIRST = ZTW_OUTER_FIRST + ZONES_COUNT,
	ZTW_INNER_END = ZTW_INNER_FIRST + ZONES_COUNT,
};

struct ZoningWindow : public Window {
	uint maxwidth;
	uint maxheight;

	ZoningWindow(WindowDesc &desc, int window_number) : Window(desc) {
		Dimension dim;
		this->maxwidth = 0;
		this->maxheight = 0;
		for (int i = 0; i < ZONES_COUNT; i++) {
			dim = GetStringBoundingBox(_zone_types[i]);
			this->maxwidth = std::max(this->maxwidth, dim.width);
			this->maxheight = std::max(this->maxheight, dim.height);
		}

		this->InitNested(window_number);
		this->InvalidateData();
		if(_zoning.outer != EvaluationMode::CHECKNOTHING) this->LowerWidget(ZTW_OUTER_FIRST + _zoning.outer - 1); //-1:skip CHECKNOTHING
		if(_zoning.inner != EvaluationMode::CHECKNOTHING) this->LowerWidget(ZTW_INNER_FIRST + _zoning.inner - 1);
	}

	void OnPaint() override
	{
		this->DrawWidgets();
	}

	void OnClick(Point /* pt */, int widget, int /* click_count */) override
	{
		bool outer = true;
		bool deselect = false;
		EvaluationMode clicked;
		if (widget >= ZTW_OUTER_FIRST && widget < ZTW_INNER_FIRST){
			clicked = (EvaluationMode)(widget - ZTW_OUTER_FIRST + 1); //+1:skip CHECKNOTHING
			deselect = _zoning.outer == clicked;
			_zoning.outer = deselect ? EvaluationMode::CHECKNOTHING : clicked;
		}
		else if (widget >= ZTW_INNER_FIRST && widget < ZTW_INNER_END){
			clicked = (EvaluationMode)(widget - ZTW_INNER_FIRST + 1);
			deselect = _zoning.inner == clicked;
			_zoning.inner = deselect ? EvaluationMode::CHECKNOTHING : clicked;
			outer = false;
		}
		else return;

		this->RaiseAllWidgets(outer);
		if(!deselect) this->ToggleWidgetLoweredState(widget);
		this->InvalidateData();
		MarkWholeScreenDirty();
	}

	void DrawWidget(const Rect &r, int widget) const override
	{
		StringID strid = STR_EMPTY;
		if (widget >= ZTW_OUTER_FIRST && widget < ZTW_INNER_FIRST){
			strid = _zone_types[widget - ZTW_OUTER_FIRST];
		}
		else if (widget >= ZTW_INNER_FIRST && widget < ZTW_INNER_END){
			strid = _zone_types[widget - ZTW_INNER_FIRST];
		}
		else return;

		bool rtl = _current_text_dir == TD_RTL;
		uint8_t clk_dif = this->IsWidgetLowered(widget) ? 1 : 0;
		int x = r.left + WidgetDimensions::scaled.framerect.left;
		int y = r.top;

		DrawString(rtl ? r.left : x + clk_dif + 1, (rtl ? r.right + clk_dif : r.right), y + 1 + clk_dif, strid, TC_FROMSTRING, SA_LEFT);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		if (widget >= ZTW_OUTER_FIRST && widget < ZTW_INNER_END){
			size.width = this->maxwidth + padding.width + 8;
			size.height = this->maxheight + 2;
		}
	}

	void RaiseAllWidgets(bool outer)
	{
		uint8_t start = outer ? ZTW_OUTER_FIRST : ZTW_INNER_FIRST;
		uint8_t end = outer ? ZTW_INNER_FIRST : ZTW_INNER_END;
		for(uint8_t i = start; i < end; i++){
			if(this->IsWidgetLowered(i)){
				this->ToggleWidgetLoweredState(i);
				break;
			}
		}
	}

	EventState OnHotkey(int hotkey) override
	{
		return Window::OnHotkey(hotkey);
	}

	static EventState ZoningWindowGlobalHotkeys(int hotkey) {
		EvaluationMode zoning = (EvaluationMode)(hotkey - ZTW_OUTER_FIRST); // +1:skip CHECKNOTHING
		bool deselect = (_zoning.outer == zoning);
		_zoning.outer = deselect ? EvaluationMode::CHECKNOTHING : zoning;
		MarkWholeScreenDirty();
		return ES_HANDLED;
	}

	static inline HotkeyList hotkeys{"zoning_gui", {
		Hotkey(WKC_SHIFT | '1', "authority", ZTW_OUTER_FIRST + EvaluationMode::CHECKOPINION),
		Hotkey(WKC_SHIFT | '2', "build_status", ZTW_OUTER_FIRST + EvaluationMode::CHECKBUILD),
		Hotkey(WKC_SHIFT | '3', "station_catchment", ZTW_OUTER_FIRST + EvaluationMode::CHECKSTACATCH),
		Hotkey(WKC_SHIFT | '4', "unserved_buildings", ZTW_OUTER_FIRST + EvaluationMode::CHECKBULUNSER),
		Hotkey(WKC_SHIFT | '5', "unserved_industries", ZTW_OUTER_FIRST + EvaluationMode::CHECKINDUNSER),
		Hotkey(WKC_SHIFT | '6', "town_zone", ZTW_OUTER_FIRST + EvaluationMode::CHECKTOWNZONES),
		Hotkey(WKC_SHIFT | '7', "cb_acceptance", ZTW_OUTER_FIRST + EvaluationMode::CHECKCBACCEPTANCE),
		Hotkey(WKC_SHIFT | '8', "cb_town_limit", ZTW_OUTER_FIRST + EvaluationMode::CHECKCBTOWNLIMIT),
		Hotkey(WKC_SHIFT | '9', "advertisement", ZTW_OUTER_FIRST + EvaluationMode::CHECKTOWNADZONES),
		Hotkey(WKC_SHIFT | '0', "growth_tiles", ZTW_OUTER_FIRST + EvaluationMode::CHECKTOWNGROWTHTILES),
		Hotkey((uint16)0, "active_stations", ZTW_OUTER_FIRST + EvaluationMode::CHECKACTIVESTATIONS)
	}, ZoningWindowGlobalHotkeys};
};


/** Construct the row containing the digit keys. */
static std::unique_ptr<NWidgetBase> MakeZoningButtons()
{
	auto hor = std::make_unique<NWidgetHorizontal>(NWidContainerFlag::EqualSize);
	int zone_types_size = lengthof(_zone_types);
	hor->SetPadding(1, 1, 1, 1);

	for(int i = 0; i < 2; i++){
		auto vert = std::make_unique<NWidgetVertical>();

		int offset = (i == 0) ? ZTW_OUTER_FIRST : ZTW_INNER_FIRST;

		for (int j = 0; j < zone_types_size; j++) {
			auto leaf = std::make_unique<NWidgetBackground>(WWT_PANEL, i==0 ? COLOUR_ORANGE : COLOUR_YELLOW, offset + j);
			leaf->SetFill(1, 0);
			leaf->SetPadding(0, 0, 0, 0);
			vert->Add(std::move(leaf));
		}
		hor->Add(std::move(vert));
	}
	return hor;
}

static const NWidgetPart _nested_zoning_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, ZTW_CAPTION), SetStringTip(CM_STR_ZONING_TOOLBAR, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidgetFunction(MakeZoningButtons),
	EndContainer()
};

static WindowDesc _zoning_desc (
	WDP_AUTO, "cm_zoning", 0, 0,
	CM_WC_ZONING_TOOLBAR, WC_NONE,
	{},
	_nested_zoning_widgets,
	&ZoningWindow::hotkeys
);

void ShowZoningToolbar() {
	AllocateWindowDescFront<ZoningWindow>(_zoning_desc, 0);
}

} // namespace citymania
