/** @file zoning_gui.cpp */
#include "stdafx.h"
#include "widgets/dropdown_func.h"
#include "table/strings.h"
#include "strings_func.h"
#include "gfx_func.h"
#include "core/geometry_func.hpp"
#include "zoning.h"
#include "hotkeys.h"

const StringID _zone_types[] = {
	//STR_ZONING_NO_ZONING,
	STR_ZONING_AUTHORITY,
	STR_ZONING_CAN_BUILD,
	STR_ZONING_STA_CATCH,
	STR_ZONING_BUL_UNSER,
	STR_ZONING_IND_UNSER,
	STR_ZONING_TOWN_ZONES,
	STR_ZONING_CB_BORDERS,
	STR_ZONING_CB_TOWN_BORDERS,
	STR_ZONING_ADVERTISEMENT_ZONES,
	STR_ZONING_TOWN_GROWTH_TILES,
};

enum ZoningToolbarWidgets {
	ZTW_CAPTION,
	ZTW_OUTER_FIRST,
	ZTW_INNER_FIRST = ZTW_OUTER_FIRST + 10,
	ZTW_INNER_END = ZTW_INNER_FIRST + 10,
};

struct ZoningWindow : public Window {
	uint maxwidth;
	uint maxheight;

	ZoningWindow(WindowDesc *desc, int window_number) : Window(desc) {
		int zone_types_size = lengthof(_zone_types);
		Dimension dim;
		this->maxwidth = 0;
		this->maxheight = 0;
		for (int i = 0; i < zone_types_size; i++) {
			dim = GetStringBoundingBox(_zone_types[i]);
			this->maxwidth = max(this->maxwidth, dim.width);
			this->maxheight = max(this->maxheight, dim.height);
		}

		this->InitNested(window_number);
		this->InvalidateData();
		if(_zoning.outer != CHECKNOTHING) this->LowerWidget(ZTW_OUTER_FIRST + _zoning.outer - 1); //-1:skip CHECKNOTHING
		if(_zoning.inner != CHECKNOTHING) this->LowerWidget(ZTW_INNER_FIRST + _zoning.inner - 1);
	}

	virtual void OnPaint() {
		this->DrawWidgets();
	}

	virtual void OnClick(Point pt, int widget, int click_count) {
		bool outer = true;
		bool deselect = false;
		EvaluationMode clicked;
		if (widget >= ZTW_OUTER_FIRST && widget < ZTW_INNER_FIRST){
			clicked = (EvaluationMode)(widget - ZTW_OUTER_FIRST + 1); //+1:skip CHECKNOTHING
			deselect = _zoning.outer == clicked;
			_zoning.outer = deselect ? CHECKNOTHING : clicked;
		}
		else if (widget >= ZTW_INNER_FIRST && widget < ZTW_INNER_END){
			clicked = (EvaluationMode)(widget - ZTW_INNER_FIRST + 1);
			deselect = _zoning.inner == clicked;
			_zoning.inner = deselect ? CHECKNOTHING : clicked;
			outer = false;
		}
		else return;

		this->RaiseAllWidgets(outer);
		if(!deselect) this->ToggleWidgetLoweredState(widget);
		this->InvalidateData();
		MarkWholeScreenDirty();
	}

	void DrawWidget(const Rect &r, int widget) const
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
		byte clk_dif = this->IsWidgetLowered(widget) ? 1 : 0;
		int x = r.left + WD_FRAMERECT_LEFT;
		int y = r.top;

		DrawString(rtl ? r.left : x + clk_dif + 1, (rtl ? r.right + clk_dif : r.right), y + 1 + clk_dif, strid, TC_FROMSTRING, SA_LEFT);
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize) {
		if (widget >= ZTW_OUTER_FIRST && widget < ZTW_INNER_END){
			size->width = this->maxwidth + padding.width + 8;
			size->height = this->maxheight + 2;
		}
	}

	void RaiseAllWidgets(bool outer){
		byte start = outer ? ZTW_OUTER_FIRST : ZTW_INNER_FIRST;
		byte end = outer ? ZTW_INNER_FIRST : ZTW_INNER_END;
		for(byte i = start; i < end; i++){
			if(this->IsWidgetLowered(i)){
				this->ToggleWidgetLoweredState(i);
				break;
			}
		}
	}

	virtual EventState OnHotkey(int hotkey)
	{
		return Window::OnHotkey(hotkey);
	}

	static HotkeyList hotkeys;
};


static EventState ZoningWindowGlobalHotkeys(int hotkey) {
	EvaluationMode zoning = (EvaluationMode)(hotkey - ZTW_OUTER_FIRST + 1); // +1:skip CHECKNOTHING
	bool deselect = (_zoning.outer == zoning);
	_zoning.outer = deselect ? CHECKNOTHING : zoning;
	MarkWholeScreenDirty();
	return ES_HANDLED;
}

static Hotkey zoning_hotkeys[] = {
	Hotkey(WKC_SHIFT | '1', "authority", ZTW_OUTER_FIRST),
	Hotkey(WKC_SHIFT | '2', "build_status", ZTW_OUTER_FIRST + 1),
	Hotkey(WKC_SHIFT | '3', "station_catchment", ZTW_OUTER_FIRST + 2),
	Hotkey(WKC_SHIFT | '4', "unserved_buildings", ZTW_OUTER_FIRST + 3),
	Hotkey(WKC_SHIFT | '5', "unserved_industries", ZTW_OUTER_FIRST + 4),
	Hotkey(WKC_SHIFT | '6', "town_zone", ZTW_OUTER_FIRST + 5),
	Hotkey(WKC_SHIFT | '7', "CB_acceptance", ZTW_OUTER_FIRST + 6),
	Hotkey(WKC_SHIFT | '8', "CB_build_borders", ZTW_OUTER_FIRST + 7),
	Hotkey(WKC_SHIFT | '9', "advertisement", ZTW_OUTER_FIRST + 8),
	Hotkey(WKC_SHIFT | '0', "growth_tiles", ZTW_OUTER_FIRST + 9),
	HOTKEY_LIST_END
};

HotkeyList ZoningWindow::hotkeys("zoning_gui", zoning_hotkeys, ZoningWindowGlobalHotkeys);


/** Construct the row containing the digit keys. */
static NWidgetBase *MakeZoningButtons(int *biggest_index)
{
	NWidgetHorizontal *hor = new NWidgetHorizontal(NC_EQUALSIZE);
	int zone_types_size = lengthof(_zone_types);
	hor->SetPadding(1, 1, 1, 1);

	for(int i = 0; i < 2; i++){
		NWidgetVertical *ver = new NWidgetVertical;

		int offset = (i == 0) ? ZTW_OUTER_FIRST : ZTW_INNER_FIRST;

		for (int j = 0; j < zone_types_size; j++) {
			NWidgetBackground *leaf = new NWidgetBackground(WWT_PANEL, i==0 ? COLOUR_ORANGE : COLOUR_YELLOW, offset + j, NULL);
			leaf->SetFill(1, 0);
			leaf->SetPadding(0, 0, 0, 0);
			ver->Add(leaf);
		}
		hor->Add(ver);
	}
	*biggest_index = ZTW_INNER_END - 1;
	return hor;
}

static const NWidgetPart _nested_zoning_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, ZTW_CAPTION), SetDataTip(STR_ZONING_TOOLBAR, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidgetFunction(MakeZoningButtons),
	EndContainer()
};

static WindowDesc _zoning_desc (
	WDP_AUTO, NULL, 0, 0,
	WC_ZONING_TOOLBAR, WC_NONE,
	0,
	_nested_zoning_widgets, lengthof(_nested_zoning_widgets),
	&ZoningWindow::hotkeys
);

void ShowZoningToolbar() {
	AllocateWindowDescFront<ZoningWindow>(&_zoning_desc, 0);
}

