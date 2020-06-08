/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file town_gui.cpp GUI for towns. */

#include "stdafx.h"
#include "town.h"
#include "viewport_func.h"
#include "error.h"
#include "gui.h"
#include "command_func.h"
#include "company_func.h"
#include "company_base.h"
#include "company_gui.h"
#include "goal_base.h"
#include "network/network.h"
#include "string_func.h"
#include "strings_func.h"
#include "sound_func.h"
#include "tilehighlight_func.h"
#include "sortlist_type.h"
#include "road_cmd.h"
#include "landscape.h"
#include "querystring_gui.h"
#include "window_func.h"
#include "townname_func.h"
#include "core/geometry_func.hpp"
#include "genworld.h"
#include "stringfilter_type.h"
#include "widgets/dropdown_func.h"
#include "town_kdtree.h"

#include "widgets/town_widget.h"

#include "table/strings.h"

#include "safeguards.h"

#include "hotkeys.h"
#include <list>
#include "console_func.h"

struct CargoX {
	int id;
	int from;
};
void ShowCBTownWindow(uint town);
static void DrawExtraTownInfo (const Rect &r, uint &y, Town *town, uint line, bool show_house_states_info=false);

bool TownExecuteAction(const Town *town, uint action){
	if(!(action == HK_STATUE && HasBit(town->statues, _current_company))){ //don't built statue when there is one
		return DoCommandP(town->xy, town->index, action, CMD_DO_TOWN_ACTION | CMD_MSG(STR_ERROR_CAN_T_DO_THIS));
	}
	return false;
}

TownKdtree _town_local_authority_kdtree(&Kdtree_TownXYFunc);

typedef GUIList<const Town*> GUITownList;

static const NWidgetPart _nested_town_authority_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN, WID_TA_CAPTION), SetDataTip(STR_LOCAL_AUTHORITY_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_TEXTBTN, COLOUR_BROWN, WID_TA_ZONE_BUTTON), SetMinimalSize(50, 0), SetMinimalTextLines(1, WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM + 2), SetDataTip(STR_LOCAL_AUTHORITY_ZONE, STR_LOCAL_AUTHORITY_ZONE_TOOLTIP),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, WID_TA_RATING_INFO), SetMinimalSize(317, 92), SetResize(1, 1), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_BROWN, WID_TA_COMMAND_LIST), SetMinimalSize(305, 52), SetResize(1, 0), SetDataTip(0x0, STR_LOCAL_AUTHORITY_ACTIONS_TOOLTIP), SetScrollbar(WID_TA_SCROLLBAR), EndContainer(),
		NWidget(NWID_VSCROLLBAR, COLOUR_BROWN, WID_TA_SCROLLBAR),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, WID_TA_ACTION_INFO), SetMinimalSize(317, 52), SetResize(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_TA_EXECUTE),  SetMinimalSize(317, 12), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_LOCAL_AUTHORITY_DO_IT_BUTTON, STR_LOCAL_AUTHORITY_DO_IT_TOOLTIP),
		NWidget(WWT_RESIZEBOX, COLOUR_BROWN),
	EndContainer()
};

/** Town authority window. */
struct TownAuthorityWindow : Window {
private:
	Town *town;    ///< Town being displayed.
	int sel_index; ///< Currently selected town action, \c 0 to \c TACT_COUNT-1, \c -1 means no action selected.
	Scrollbar *vscroll;
	uint displayed_actions_on_previous_painting; ///< Actions that were available on the previous call to OnPaint()

	/**
	 * Get the position of the Nth set bit.
	 *
	 * If there is no Nth bit set return -1
	 *
	 * @param bits The value to search in
	 * @param n The Nth set bit from which we want to know the position
	 * @return The position of the Nth set bit
	 */
	static int GetNthSetBit(uint32 bits, int n)
	{
		if (n >= 0) {
			uint i;
			FOR_EACH_SET_BIT(i, bits) {
				n--;
				if (n < 0) return i;
			}
		}
		return -1;
	}

public:
	TownAuthorityWindow(WindowDesc *desc, WindowNumber window_number) : Window(desc), sel_index(-1), displayed_actions_on_previous_painting(0)
	{
		this->town = Town::Get(window_number);
		this->InitNested(window_number);
		this->vscroll = this->GetScrollbar(WID_TA_SCROLLBAR);
		this->vscroll->SetCapacity((this->GetWidget<NWidgetBase>(WID_TA_COMMAND_LIST)->current_y - WD_FRAMERECT_TOP - WD_FRAMERECT_BOTTOM) / FONT_HEIGHT_NORMAL);
	}

	void OnPaint() override
	{
		int numact;
		uint buttons = GetMaskOfTownActions(&numact, _local_company, this->town);
		if (buttons != displayed_actions_on_previous_painting) this->SetDirty();
		displayed_actions_on_previous_painting = buttons;

		this->vscroll->SetCount(numact + 1);

		if (this->sel_index != -1 && !HasBit(buttons, this->sel_index)) {
			this->sel_index = -1;
		}

		this->SetWidgetLoweredState(WID_TA_ZONE_BUTTON, this->town->show_zone);
		this->SetWidgetDisabledState(WID_TA_EXECUTE, this->sel_index == -1);

		this->DrawWidgets();
		if (!this->IsShaded()) this->DrawRatings();
	}

	/** Draw the contents of the ratings panel. May request a resize of the window if the contents does not fit. */
	void DrawRatings()
	{
		NWidgetBase *nwid = this->GetWidget<NWidgetBase>(WID_TA_RATING_INFO);
		uint left = nwid->pos_x + WD_FRAMERECT_LEFT;
		uint right = nwid->pos_x + nwid->current_x - 1 - WD_FRAMERECT_RIGHT;

		uint y = nwid->pos_y + WD_FRAMERECT_TOP;

		DrawString(left, right, y, STR_LOCAL_AUTHORITY_COMPANY_RATINGS);
		y += FONT_HEIGHT_NORMAL;

		Dimension icon_size = GetSpriteSize(SPR_COMPANY_ICON);
		int icon_width      = icon_size.width;
		int icon_y_offset   = (FONT_HEIGHT_NORMAL - icon_size.height) / 2;

		Dimension exclusive_size = GetSpriteSize(SPR_EXCLUSIVE_TRANSPORT);
		int exclusive_width      = exclusive_size.width;
		int exclusive_y_offset   = (FONT_HEIGHT_NORMAL - exclusive_size.height) / 2;

		bool rtl = _current_text_dir == TD_RTL;
		uint text_left      = left  + (rtl ? 0 : icon_width + exclusive_width + 4);
		uint text_right     = right - (rtl ? icon_width + exclusive_width + 4 : 0);
		uint icon_left      = rtl ? right - icon_width : left;
		uint exclusive_left = rtl ? right - icon_width - exclusive_width - 2 : left + icon_width + 2;

		/* Draw list of companies */
		for (const Company *c : Company::Iterate()) {
			if ((HasBit(this->town->have_ratings, c->index) || this->town->exclusivity == c->index)) {
				DrawCompanyIcon(c->index, icon_left, y + icon_y_offset);

				SetDParam(0, c->index);
				SetDParam(1, c->index);

				int r = this->town->ratings[c->index];
				StringID str = STR_CARGO_RATING_APPALLING;
				if (r > RATING_APPALLING) str++;
				if (r > RATING_VERYPOOR)  str++;
				if (r > RATING_POOR)      str++;
				if (r > RATING_MEDIOCRE)  str++;
				if (r > RATING_GOOD)      str++;
				if (r > RATING_VERYGOOD)  str++;
				if (r > RATING_EXCELLENT) str++;

				SetDParam(2, str);
				SetDParam(3, this->town->ratings[c->index]);
				if (this->town->exclusivity == c->index) {
					DrawSprite(SPR_EXCLUSIVE_TRANSPORT, COMPANY_SPRITE_COLOUR(c->index), exclusive_left, y + exclusive_y_offset);
				}

				DrawString(text_left, text_right, y, STR_LOCAL_AUTHORITY_COMPANY_RATING_NUM);
				y += FONT_HEIGHT_NORMAL;
			}
		}

		y = y + WD_FRAMERECT_BOTTOM - nwid->pos_y; // Compute needed size of the widget.
		if (y > nwid->current_y) {
			/* If the company list is too big to fit, mark ourself dirty and draw again. */
			ResizeWindow(this, 0, y - nwid->current_y, false);
		}
	}

	void SetStringParameters(int widget) const override
	{
		if (widget == WID_TA_CAPTION) SetDParam(0, this->window_number);
	}

	void DrawWidget(const Rect &r, int widget) const override
	{
		switch (widget) {
			case WID_TA_ACTION_INFO:
				if (this->sel_index != -1) {
					SetDParam(0, _price[PR_TOWN_ACTION] * _town_action_costs[this->sel_index] >> 8);
					DrawStringMultiLine(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, r.top + WD_FRAMERECT_TOP, r.bottom - WD_FRAMERECT_BOTTOM,
								STR_LOCAL_AUTHORITY_ACTION_TOOLTIP_SMALL_ADVERTISING + this->sel_index);
				}
				break;
			case WID_TA_COMMAND_LIST: {
				int numact;
				uint buttons = GetMaskOfTownActions(&numact, _local_company, this->town);
				int y = r.top + WD_FRAMERECT_TOP;
				int pos = this->vscroll->GetPosition();

				if (--pos < 0) {
					DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_LOCAL_AUTHORITY_ACTIONS_TITLE);
					y += FONT_HEIGHT_NORMAL;
				}

				for (int i = 0; buttons; i++, buttons >>= 1) {
					if (pos <= -5) break; ///< Draw only the 5 fitting lines

					if ((buttons & 1) && --pos < 0) {
						DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y,
								STR_LOCAL_AUTHORITY_ACTION_SMALL_ADVERTISING_CAMPAIGN + i, this->sel_index == i ? TC_WHITE : TC_ORANGE);
						y += FONT_HEIGHT_NORMAL;
					}
				}
				break;
			}
		}
	}

	void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize) override
	{
		switch (widget) {
			case WID_TA_ACTION_INFO: {
				assert(size->width > padding.width && size->height > padding.height);
				size->width -= WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT;
				size->height -= WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
				Dimension d = {0, 0};
				for (int i = 0; i < TACT_COUNT; i++) {
					SetDParam(0, _price[PR_TOWN_ACTION] * _town_action_costs[i] >> 8);
					d = maxdim(d, GetStringMultiLineBoundingBox(STR_LOCAL_AUTHORITY_ACTION_TOOLTIP_SMALL_ADVERTISING + i, *size));
				}
				*size = maxdim(*size, d);
				size->width += WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT;
				size->height += WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
				break;
			}

			case WID_TA_COMMAND_LIST:
				size->height = WD_FRAMERECT_TOP + 5 * FONT_HEIGHT_NORMAL + WD_FRAMERECT_BOTTOM;
				size->width = GetStringBoundingBox(STR_LOCAL_AUTHORITY_ACTIONS_TITLE).width;
				for (uint i = 0; i < TACT_COUNT; i++ ) {
					size->width = max(size->width, GetStringBoundingBox(STR_LOCAL_AUTHORITY_ACTION_SMALL_ADVERTISING_CAMPAIGN + i).width);
				}
				size->width += WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT;
				break;

			case WID_TA_RATING_INFO:
				resize->height = FONT_HEIGHT_NORMAL;
				size->height = WD_FRAMERECT_TOP + 9 * FONT_HEIGHT_NORMAL + WD_FRAMERECT_BOTTOM;
				break;
		}
	}

	void OnClick(Point pt, int widget, int click_count) override
	{
		switch (widget) {
			case WID_TA_ZONE_BUTTON: {
				bool new_show_state = !this->town->show_zone;
				TownID index = this->town->index;

				new_show_state ? _town_local_authority_kdtree.Insert(index) : _town_local_authority_kdtree.Remove(index);

				this->town->show_zone = new_show_state;
				this->SetWidgetLoweredState(widget, new_show_state);
				MarkWholeScreenDirty();
				break;
			}

			case WID_TA_COMMAND_LIST: {
				int y = this->GetRowFromWidget(pt.y, WID_TA_COMMAND_LIST, 1, FONT_HEIGHT_NORMAL);
				if (!IsInsideMM(y, 0, 5)) return;

				y = GetNthSetBit(GetMaskOfTownActions(nullptr, _local_company, this->town), y + this->vscroll->GetPosition() - 1);
				if (y >= 0) {
					this->sel_index = y;
					this->SetDirty();
				}
				/* When double-clicking, continue */
				if (click_count == 1 || y < 0) break;
				FALLTHROUGH;
			}

			case WID_TA_EXECUTE:
				DoCommandP(this->town->xy, this->window_number, this->sel_index, CMD_DO_TOWN_ACTION | CMD_MSG(STR_ERROR_CAN_T_DO_THIS));
				break;
		}
	}

	void OnHundredthTick() override
	{
		this->SetDirty();
	}

	virtual EventState OnHotkey(int hotkey)
	{
		TownExecuteAction(this->town, hotkey);
		return ES_HANDLED;
	}

	static HotkeyList hotkeys;
};

static Hotkey town_hotkeys[] = {
	Hotkey((uint16)0, "small_advert", HK_SADVERT),
	Hotkey((uint16)0, "medium_advert", HK_MADVERT),
	Hotkey(WKC_CTRL | 'D', "large_advert", HK_LADVERT),
	Hotkey(WKC_CTRL | 'S', "build_statue", HK_STATUE),
	Hotkey(WKC_CTRL | 'F', "fund_buildings", HK_FUND),
	HOTKEY_LIST_END
};
HotkeyList TownAuthorityWindow::hotkeys("town_gui", town_hotkeys);

static WindowDesc _town_authority_desc(
	WDP_AUTO, "view_town_authority", 317, 222,
	WC_TOWN_AUTHORITY, WC_NONE,
	0,
	_nested_town_authority_widgets, lengthof(_nested_town_authority_widgets),
	&TownAuthorityWindow::hotkeys
);

static void ShowTownAuthorityWindow(uint town)
{
	AllocateWindowDescFront<TownAuthorityWindow>(&_town_authority_desc, town);
}

// static int TownTicksToDays(int ticks) {
//  	return (ticks * TOWN_GROWTH_TICKS + DAY_TICKS / 2) / DAY_TICKS;
// }

/* Town view window. */
struct TownViewWindow : Window {
private:
	Town *town; ///< Town displayed by the window.

public:
	static const int WID_TV_HEIGHT_NORMAL = 150;
	bool wcb_disable;

	TownViewWindow(WindowDesc *desc, WindowNumber window_number) : Window(desc)
	{
		this->CreateNestedTree();

		this->town = Town::Get(window_number);
		if (this->town->larger_town) this->GetWidget<NWidgetCore>(WID_TV_CAPTION)->widget_data = STR_TOWN_VIEW_CITY_CAPTION;

		this->FinishInitNested(window_number);

		this->flags |= WF_DISABLE_VP_SCROLL;
		NWidgetViewport *nvp = this->GetWidget<NWidgetViewport>(WID_TV_VIEWPORT);
		nvp->InitializeViewport(this, this->town->xy, ZOOM_LVL_NEWS);

		/* disable renaming town in network games if you are not the server */
		this->SetWidgetDisabledState(WID_TV_CHANGE_NAME, _networking && !_network_server);
		// extern bool _novahost;
		// this->wcb_disable = !_networking || !_novahost || this->town->larger_town || _game_mode == GM_EDITOR;
		// this->wcb_disable = (_game_mode == GM_EDITOR);
		this->SetWidgetDisabledState(WID_TV_CB, _game_mode == GM_EDITOR);
	}

	~TownViewWindow()
	{
		SetViewportCatchmentTown(Town::Get(this->window_number), false);
	}

	void SetStringParameters(int widget) const override
	{
		if (widget == WID_TV_CAPTION){
			SetDParam(0, this->town->index);
		}
		if (widget == WID_TV_CB){
			// if(this->wcb_disable) SetDParam(0, STR_EMPTY);
			// else
			SetDParam(0, STR_BUTTON_CB_YES);
		}
	}

	virtual void OnHundrethTick() {
		this->SetDirty();
	}

	void OnPaint() override
	{
		extern const Town *_viewport_highlight_town;
		this->SetWidgetLoweredState(WID_TV_CATCHMENT, _viewport_highlight_town == this->town);

		this->DrawWidgets();
	}

	void DrawWidget(const Rect &r, int widget) const override
	{
		if (widget != WID_TV_INFO) return;

		uint y = r.top + WD_FRAMERECT_TOP;

		SetDParam(0, this->town->cache.population);
		SetDParam(1, this->town->cache.num_houses);
		DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_LEFT, y, STR_TOWN_VIEW_POPULATION_HOUSES);

		SetDParam(0, 1 << CT_PASSENGERS);
		SetDParam(1, this->town->supplied[CT_PASSENGERS].old_act);
		SetDParam(2, this->town->supplied[CT_PASSENGERS].old_max);
		DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_LEFT, y += FONT_HEIGHT_NORMAL, STR_TOWN_VIEW_CARGO_LAST_MONTH_MAX);

		SetDParam(0, 1 << CT_MAIL);
		SetDParam(1, this->town->supplied[CT_MAIL].old_act);
		SetDParam(2, this->town->supplied[CT_MAIL].old_max);
		DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_LEFT, y += FONT_HEIGHT_NORMAL, STR_TOWN_VIEW_CARGO_LAST_MONTH_MAX);

		DrawExtraTownInfo(r, y, this->town, FONT_HEIGHT_NORMAL, true); //CB

		bool first = true;
		for (int i = TE_BEGIN; i < TE_END; i++) {
			if (this->town->goal[i] == 0) continue;
			if (this->town->goal[i] == TOWN_GROWTH_WINTER && (TileHeight(this->town->xy) < LowestSnowLine() || this->town->cache.population <= 90)) continue;
			if (this->town->goal[i] == TOWN_GROWTH_DESERT && (GetTropicZone(this->town->xy) != TROPICZONE_DESERT || this->town->cache.population <= 60)) continue;

			if (first) {
				DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_LEFT, y += FONT_HEIGHT_NORMAL, STR_TOWN_VIEW_CARGO_FOR_TOWNGROWTH);
				first = false;
			}

			bool rtl = _current_text_dir == TD_RTL;
			uint cargo_text_left = r.left + WD_FRAMERECT_LEFT + (rtl ? 0 : 20);
			uint cargo_text_right = r.right - WD_FRAMERECT_RIGHT - (rtl ? 20 : 0);

			const CargoSpec *cargo = FindFirstCargoWithTownEffect((TownEffect)i);
			assert(cargo != nullptr);

			StringID string;

			if (this->town->goal[i] == TOWN_GROWTH_DESERT || this->town->goal[i] == TOWN_GROWTH_WINTER) {
				/* For 'original' gameplay, don't show the amount required (you need 1 or more ..) */
				string = STR_TOWN_VIEW_CARGO_FOR_TOWNGROWTH_DELIVERED_GENERAL;
				if (this->town->received[i].old_act == 0) {
					string = STR_TOWN_VIEW_CARGO_FOR_TOWNGROWTH_REQUIRED_GENERAL;

					if (this->town->goal[i] == TOWN_GROWTH_WINTER && TileHeight(this->town->xy) < GetSnowLine()) {
						string = STR_TOWN_VIEW_CARGO_FOR_TOWNGROWTH_REQUIRED_WINTER;
					}
				}

				SetDParam(0, cargo->name);
			} else {
				string = STR_TOWN_VIEW_CARGO_FOR_TOWNGROWTH_DELIVERED;
				if (this->town->received[i].old_act < this->town->goal[i]) {
					string = STR_TOWN_VIEW_CARGO_FOR_TOWNGROWTH_REQUIRED;
				}

				SetDParam(0, cargo->Index());
				SetDParam(1, this->town->received[i].old_act);
				SetDParam(2, cargo->Index());
				SetDParam(3, this->town->goal[i]);
			}
			DrawString(cargo_text_left, cargo_text_right, y += FONT_HEIGHT_NORMAL, string);
		}

		if (HasBit(this->town->flags, TOWN_IS_GROWING)) {
			SetDParam(0, RoundDivSU(this->town->growth_rate + 1, DAY_TICKS));
			DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_LEFT, y += FONT_HEIGHT_NORMAL, this->town->fund_buildings_months == 0 ? STR_TOWN_VIEW_TOWN_GROWS_EVERY : STR_TOWN_VIEW_TOWN_GROWS_EVERY_FUNDED);
		} else {
			DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_LEFT, y += FONT_HEIGHT_NORMAL, STR_TOWN_VIEW_TOWN_GROW_STOPPED);
		}

		/* only show the town noise, if the noise option is activated. */
		if (_settings_game.economy.station_noise_level) {
			SetDParam(0, this->town->noise_reached);
			SetDParam(1, this->town->MaxTownNoise());
			DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_LEFT, y += FONT_HEIGHT_NORMAL, STR_TOWN_VIEW_NOISE_IN_TOWN);
		}

		if (this->town->text != nullptr) {
			SetDParamStr(0, this->town->text);
			DrawStringMultiLine(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y += FONT_HEIGHT_NORMAL, UINT16_MAX, STR_JUST_RAW_STRING, TC_BLACK);
		}
	}

	void OnClick(Point pt, int widget, int click_count) override
	{
		switch (widget) {
			case WID_TV_CENTER_VIEW: // scroll to location
				if (_ctrl_pressed) {
					ShowExtraViewPortWindow(this->town->xy);
				} else {
					ScrollMainWindowToTile(this->town->xy);
				}
				break;

			case WID_TV_SHOW_AUTHORITY: // town authority
				ShowTownAuthorityWindow(this->window_number);
				break;

			case WID_TV_CHANGE_NAME: // rename
				SetDParam(0, this->window_number);
				ShowQueryString(STR_TOWN_NAME, STR_TOWN_VIEW_RENAME_TOWN_BUTTON, MAX_LENGTH_TOWN_NAME_CHARS, this, CS_ALPHANUMERAL, QSF_ENABLE_DEFAULT | QSF_LEN_IN_CHARS);
				break;

			case WID_TV_CATCHMENT:
				SetViewportCatchmentTown(Town::Get(this->window_number), !this->IsWidgetLowered(WID_TV_CATCHMENT));
				break;

			case WID_TV_EXPAND: { // expand town - only available on Scenario editor
				/* Warn the user if towns are not allowed to build roads, but do this only once per OpenTTD run. */
				static bool _warn_town_no_roads = false;

				if (!_settings_game.economy.allow_town_roads && !_warn_town_no_roads) {
					ShowErrorMessage(STR_ERROR_TOWN_EXPAND_WARN_NO_ROADS, INVALID_STRING_ID, WL_WARNING);
					_warn_town_no_roads = true;
				}

				DoCommandP(0, this->window_number, 0, CMD_EXPAND_TOWN | CMD_MSG(STR_ERROR_CAN_T_EXPAND_TOWN));
				break;
			}

			case WID_TV_CB:
				// if(_networking)
				ShowCBTownWindow(this->window_number);
				break;

			case WID_TV_DELETE: // delete town - only available on Scenario editor
				DoCommandP(0, this->window_number, 0, CMD_DELETE_TOWN | CMD_MSG(STR_ERROR_TOWN_CAN_T_DELETE));
				break;
		}
	}

	void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize) override
	{
		switch (widget) {
			case WID_TV_INFO:
				size->height = GetDesiredInfoHeight(size->width);
				break;
			// case WID_TV_CB:
				// if(this->wcb_disable || !CB_Enabled()) size->width = 0;
				// break;
		}
	}

	/**
	 * Gets the desired height for the information panel.
	 * @return the desired height in pixels.
	 */
	uint GetDesiredInfoHeight(int width) const
	{
		uint aimed_height = 8 * FONT_HEIGHT_NORMAL + WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;

		bool first = true;
		for (int i = TE_BEGIN; i < TE_END; i++) {
			if (this->town->goal[i] == 0) continue;
			if (this->town->goal[i] == TOWN_GROWTH_WINTER && (TileHeight(this->town->xy) < LowestSnowLine() || this->town->cache.population <= 90)) continue;
			if (this->town->goal[i] == TOWN_GROWTH_DESERT && (GetTropicZone(this->town->xy) != TROPICZONE_DESERT || this->town->cache.population <= 60)) continue;

			if (first) {
				aimed_height += FONT_HEIGHT_NORMAL;
				first = false;
			}
			aimed_height += FONT_HEIGHT_NORMAL;
		}
		aimed_height += FONT_HEIGHT_NORMAL;

		if (_settings_game.economy.station_noise_level) aimed_height += FONT_HEIGHT_NORMAL;

		if (this->town->text != nullptr) {
			SetDParamStr(0, this->town->text);
			aimed_height += GetStringHeight(STR_JUST_RAW_STRING, width - WD_FRAMERECT_LEFT - WD_FRAMERECT_RIGHT);
		}

		return aimed_height;
	}

	void ResizeWindowAsNeeded()
	{
		const NWidgetBase *nwid_info = this->GetWidget<NWidgetBase>(WID_TV_INFO);
		uint aimed_height = GetDesiredInfoHeight(nwid_info->current_x);
		if (aimed_height > nwid_info->current_y || (aimed_height < nwid_info->current_y && nwid_info->current_y > nwid_info->smallest_y)) {
			this->ReInit();
		}
	}

	void OnResize() override
	{
		if (this->viewport != nullptr) {
			NWidgetViewport *nvp = this->GetWidget<NWidgetViewport>(WID_TV_VIEWPORT);
			nvp->UpdateViewportCoordinates(this);

			ScrollWindowToTile(this->town->xy, this, true); // Re-center viewport.
		}
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData(int data = 0, bool gui_scope = true) override
	{
		if (!gui_scope) return;
		/* Called when setting station noise or required cargoes have changed, in order to resize the window */
		this->SetDirty(); // refresh display for current size. This will allow to avoid glitches when downgrading
		this->ResizeWindowAsNeeded();
	}

	void OnQueryTextFinished(char *str) override
	{
		if (str == nullptr) return;

		DoCommandP(0, this->window_number, 0, CMD_RENAME_TOWN | CMD_MSG(STR_ERROR_CAN_T_RENAME_TOWN), nullptr, str);
	}

	virtual EventState OnHotkey(int hotkey)
	{
		if(hotkey == WID_TV_CB) ShowCBTownWindow(this->window_number);
		else if (hotkey == HK_STATUE + 0x80){
			TownExecuteAction(this->town, HK_STATUE);
			return ES_NOT_HANDLED;
		}
		return Window::OnHotkey(hotkey);
	}

	static HotkeyList hotkeys;
};

static Hotkey town_window_hotkeys[] = {
	Hotkey((uint16)0, "location", WID_TV_CENTER_VIEW),
	Hotkey((uint16)0, "local_authority", WID_TV_SHOW_AUTHORITY),
	Hotkey((uint16)0, "cb_window", WID_TV_CB),
	Hotkey(WKC_CTRL | 'S', "build_statue", HK_STATUE + 0x80),
	HOTKEY_LIST_END
};
HotkeyList TownViewWindow::hotkeys("town_window", town_window_hotkeys);

static const NWidgetPart _nested_town_game_view_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN, WID_TV_CAPTION), SetDataTip(STR_TOWN_VIEW_TOWN_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN),
		NWidget(WWT_INSET, COLOUR_BROWN), SetPadding(2, 2, 2, 2),
			NWidget(NWID_VIEWPORT, INVALID_COLOUR, WID_TV_VIEWPORT), SetMinimalSize(254, 86), SetFill(1, 0), SetResize(1, 1), SetPadding(1, 1, 1, 1),
		EndContainer(),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, WID_TV_INFO), SetMinimalSize(260, 32), SetResize(1, 0), SetFill(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_HORIZONTAL), //, NC_EQUALSIZE
			NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_TV_CENTER_VIEW), SetMinimalSize(60, 12), SetFill(1, 1), SetResize(1, 0), SetDataTip(STR_BUTTON_LOCATION, STR_TOWN_VIEW_CENTER_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_TV_SHOW_AUTHORITY), SetMinimalSize(80, 12), SetFill(1, 1), SetResize(1, 0), SetDataTip(STR_TOWN_VIEW_LOCAL_AUTHORITY_BUTTON, STR_TOWN_VIEW_LOCAL_AUTHORITY_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_TV_CHANGE_NAME), SetMinimalSize(60, 12), SetFill(1, 1), SetResize(1, 0), SetDataTip(STR_BUTTON_RENAME, STR_TOWN_VIEW_RENAME_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_TV_CB), SetMinimalSize(20, 12), SetFill(1, 1), SetResize(1, 0), SetDataTip(STR_BUTTON_CB, 0),
		EndContainer(),
		NWidget(WWT_TEXTBTN, COLOUR_BROWN, WID_TV_CATCHMENT), SetMinimalSize(14, 12), SetFill(0, 1), SetDataTip(STR_BUTTON_CATCHMENT, STR_TOOLTIP_CATCHMENT),
		NWidget(WWT_RESIZEBOX, COLOUR_BROWN),
	EndContainer(),
};

static WindowDesc _town_game_view_desc(
	WDP_AUTO, "view_town", 260, TownViewWindow::WID_TV_HEIGHT_NORMAL,
	WC_TOWN_VIEW, WC_NONE,
	0,
	_nested_town_game_view_widgets, lengthof(_nested_town_game_view_widgets),
	&TownViewWindow::hotkeys
);

static const NWidgetPart _nested_town_editor_view_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN, WID_TV_CAPTION), SetDataTip(STR_TOWN_VIEW_TOWN_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_TV_CHANGE_NAME), SetMinimalSize(76, 14), SetDataTip(STR_BUTTON_RENAME, STR_TOWN_VIEW_RENAME_TOOLTIP),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN),
		NWidget(WWT_INSET, COLOUR_BROWN), SetPadding(2, 2, 2, 2),
			NWidget(NWID_VIEWPORT, INVALID_COLOUR, WID_TV_VIEWPORT), SetMinimalSize(254, 86), SetFill(1, 1), SetResize(1, 1), SetPadding(1, 1, 1, 1),
		EndContainer(),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, WID_TV_INFO), SetMinimalSize(260, 32), SetResize(1, 0), SetFill(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
			NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_TV_CENTER_VIEW), SetMinimalSize(80, 12), SetFill(1, 1), SetResize(1, 0), SetDataTip(STR_BUTTON_LOCATION, STR_TOWN_VIEW_CENTER_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_TV_EXPAND), SetMinimalSize(80, 12), SetFill(1, 1), SetResize(1, 0), SetDataTip(STR_TOWN_VIEW_EXPAND_BUTTON, STR_TOWN_VIEW_EXPAND_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_TV_DELETE), SetMinimalSize(80, 12), SetFill(1, 1), SetResize(1, 0), SetDataTip(STR_TOWN_VIEW_DELETE_BUTTON, STR_TOWN_VIEW_DELETE_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_TV_CB), SetMinimalSize(20, 12), SetFill(1, 1), SetResize(1, 0), SetDataTip(STR_BUTTON_CB, 0),
		EndContainer(),
		NWidget(WWT_TEXTBTN, COLOUR_BROWN, WID_TV_CATCHMENT), SetMinimalSize(14, 12), SetFill(0, 1), SetDataTip(STR_BUTTON_CATCHMENT, STR_TOOLTIP_CATCHMENT),
		NWidget(WWT_RESIZEBOX, COLOUR_BROWN),
	EndContainer(),
};

static WindowDesc _town_editor_view_desc(
	WDP_AUTO, "view_town_scen", 260, TownViewWindow::WID_TV_HEIGHT_NORMAL,
	WC_TOWN_VIEW, WC_NONE,
	0,
	_nested_town_editor_view_widgets, lengthof(_nested_town_editor_view_widgets)
);

void ShowTownViewWindow(TownID town)
{
	if (_game_mode == GM_EDITOR) {
		AllocateWindowDescFront<TownViewWindow>(&_town_editor_view_desc, town);
	} else {
		AllocateWindowDescFront<TownViewWindow>(&_town_game_view_desc, town);
	}
}

static const NWidgetPart _nested_town_directory_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN, TDW_CAPTION_TEXT), SetDataTip(STR_TOWN_DIRECTORY_CAPTION_EXTRA, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_TEXTBTN, COLOUR_BROWN, WID_TD_SORT_ORDER), SetDataTip(STR_BUTTON_SORT_BY, STR_TOOLTIP_SORT_ORDER),
				NWidget(WWT_DROPDOWN, COLOUR_BROWN, WID_TD_SORT_CRITERIA), SetDataTip(STR_JUST_STRING, STR_TOOLTIP_SORT_CRITERIA),
				NWidget(WWT_EDITBOX, COLOUR_BROWN, WID_TD_FILTER), SetFill(35, 12), SetDataTip(STR_LIST_FILTER_OSKTITLE, STR_LIST_FILTER_TOOLTIP),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_BROWN, WID_TD_LIST), SetMinimalSize(196, 0), SetDataTip(0x0, STR_TOWN_DIRECTORY_LIST_TOOLTIP),
							SetFill(1, 0), SetResize(0, 10), SetScrollbar(WID_TD_SCROLLBAR), EndContainer(),
			NWidget(WWT_PANEL, COLOUR_BROWN),
				NWidget(WWT_TEXT, COLOUR_BROWN, WID_TD_WORLD_POPULATION), SetPadding(2, 0, 0, 2), SetMinimalSize(196, 12), SetFill(1, 0), SetDataTip(STR_TOWN_POPULATION, STR_NULL),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_VSCROLLBAR, COLOUR_BROWN, WID_TD_SCROLLBAR),
			NWidget(WWT_RESIZEBOX, COLOUR_BROWN),
		EndContainer(),
	EndContainer(),
};

/** Town directory window class. */
struct TownDirectoryWindow : public Window {
private:
	/* Runtime saved values */
	static Listing last_sorting;

	/* Constants for sorting towns */
	static const StringID sorter_names[];
	static GUITownList::SortFunction * const sorter_funcs[];

	StringFilter string_filter;             ///< Filter for towns
	QueryString townname_editbox;           ///< Filter editbox

	GUITownList towns;

	Scrollbar *vscroll;

	void BuildSortTownList()
	{
		if (this->towns.NeedRebuild()) {
			this->towns.clear();

			for (const Town *t : Town::Iterate()) {
				if (this->string_filter.IsEmpty()) {
					this->towns.push_back(t);
					continue;
				}
				this->string_filter.ResetState();
				this->string_filter.AddLine(t->GetCachedName());
				if (this->string_filter.GetState()) this->towns.push_back(t);
			}

			this->towns.shrink_to_fit();
			this->towns.RebuildDone();
			this->vscroll->SetCount((uint)this->towns.size()); // Update scrollbar as well.
		}
		/* Always sort the towns. */
		this->towns.Sort();
		this->SetWidgetDirty(WID_TD_LIST); // Force repaint of the displayed towns.
	}

	/** Sort by town name */
	static bool TownNameSorter(const Town * const &a, const Town * const &b)
	{
		return strnatcmp(a->GetCachedName(), b->GetCachedName()) < 0; // Sort by name (natural sorting).
	}

	/** Sort by population (default descending, as big towns are of the most interest). */
	static bool TownPopulationSorter(const Town * const &a, const Town * const &b)
	{
		uint32 a_population = a->cache.population;
		uint32 b_population = b->cache.population;
		if (a_population == b_population) return TownDirectoryWindow::TownNameSorter(a, b);
		return a_population < b_population;
	}

	/** Sort by real population (default descending, as big towns are of the most interest). */
	static bool TownRealPopulationSorter(const Town * const &a, const Town * const &b)
	{
		uint32 a_population = a->cache.potential_pop;
		uint32 b_population = b->cache.potential_pop;
		if (a_population == b_population) return TownDirectoryWindow::TownNameSorter(a, b);
		return a_population < b_population;
	}

	/** Sort by number of houses (default descending, as big towns are of the most interest). */
	static bool TownHousesSorter(const Town * const &a, const Town * const &b)
	{
		uint32 a_houses = a->cache.num_houses;
		uint32 b_houses = b->cache.num_houses;
		if (a_houses == b_houses) return TownDirectoryWindow::TownPopulationSorter(a, b);
		return a_houses < b_houses;
	}

	/** Sort by town rating */
	static bool TownRatingSorter(const Town * const &a, const Town * const &b)
	{
		bool before = !TownDirectoryWindow::last_sorting.order; // Value to get 'a' before 'b'.

		/* Towns without rating are always after towns with rating. */
		if (HasBit(a->have_ratings, _local_company)) {
			if (HasBit(b->have_ratings, _local_company)) {
				int16 a_rating = a->ratings[_local_company];
				int16 b_rating = b->ratings[_local_company];
				if (a_rating == b_rating) return TownDirectoryWindow::TownNameSorter(a, b);
				return a_rating < b_rating;
			}
			return before;
		}
		if (HasBit(b->have_ratings, _local_company)) return !before;

		/* Sort unrated towns always on ascending town name. */
		if (before) return TownDirectoryWindow::TownNameSorter(a, b);
		return !TownDirectoryWindow::TownNameSorter(a, b);
	}

public:
	TownDirectoryWindow(WindowDesc *desc) : Window(desc), townname_editbox(MAX_LENGTH_TOWN_NAME_CHARS * MAX_CHAR_LENGTH, MAX_LENGTH_TOWN_NAME_CHARS)
	{
		this->CreateNestedTree();

		this->vscroll = this->GetScrollbar(WID_TD_SCROLLBAR);

		this->towns.SetListing(this->last_sorting);
		this->towns.SetSortFuncs(TownDirectoryWindow::sorter_funcs);
		this->towns.ForceRebuild();
		this->BuildSortTownList();

		this->FinishInitNested(0);

		this->querystrings[WID_TD_FILTER] = &this->townname_editbox;
		this->townname_editbox.cancel_button = QueryString::ACTION_CLEAR;
	}

	void SetStringParameters(int widget) const override
	{
		switch (widget) {
			case WID_TD_WORLD_POPULATION:
				SetDParam(0, GetWorldPopulation());
				break;

			case WID_TD_SORT_CRITERIA:
				SetDParam(0, TownDirectoryWindow::sorter_names[this->towns.SortType()]);
				break;
			case TDW_CAPTION_TEXT: {
				uint16 town_number = 0;
				uint16 city_number = 0;
				for (Town *t : Town::Iterate()) {
					if(t->larger_town) city_number++;
					town_number++;
				}
				SetDParam(0, city_number);
				SetDParam(1, town_number);
				break;
			}
		}
	}

	/**
	 * Get the string to draw the town name.
	 * @param t Town to draw.
	 * @return The string to use.
	 */
	static StringID GetTownString(const Town *t)
	{
		return t->larger_town ? STR_TOWN_DIRECTORY_CITY : STR_TOWN_DIRECTORY_TOWN;
	}

	void DrawWidget(const Rect &r, int widget) const override
	{
		switch (widget) {
			case WID_TD_SORT_ORDER:
				this->DrawSortButtonState(widget, this->towns.IsDescSortOrder() ? SBS_DOWN : SBS_UP);
				break;

			case WID_TD_LIST: {
				int n = 0;
				int y = r.top + WD_FRAMERECT_TOP;
				if (this->towns.size() == 0) { // No towns available.
					DrawString(r.left + WD_FRAMERECT_LEFT, r.right, y, STR_TOWN_DIRECTORY_NONE);
					break;
				}

				/* At least one town available. */
				bool rtl = _current_text_dir == TD_RTL;
				Dimension icon_size = GetSpriteSize(SPR_TOWN_RATING_GOOD);
				int text_left  = r.left + WD_FRAMERECT_LEFT + (rtl ? 0 : icon_size.width + 2);
				int text_right = r.right - WD_FRAMERECT_RIGHT - (rtl ? icon_size.width + 2 : 0);
				int icon_x = rtl ? r.right - WD_FRAMERECT_RIGHT - icon_size.width : r.left + WD_FRAMERECT_LEFT;

				for (uint i = this->vscroll->GetPosition(); i < this->towns.size(); i++) {
					const Town *t = this->towns[i];
					assert(t->xy != INVALID_TILE);

					/* Draw rating icon. */
					if (_game_mode == GM_EDITOR || !HasBit(t->have_ratings, _local_company)) {
						DrawSprite(SPR_TOWN_RATING_NA, PAL_NONE, icon_x, y + (this->resize.step_height - icon_size.height) / 2);
					} else {
						SpriteID icon = SPR_TOWN_RATING_APALLING;
						if (t->ratings[_local_company] > RATING_VERYPOOR) icon = SPR_TOWN_RATING_MEDIOCRE;
						if (t->ratings[_local_company] > RATING_GOOD)     icon = SPR_TOWN_RATING_GOOD;
						DrawSprite(icon, PAL_NONE, icon_x, y + (this->resize.step_height - icon_size.height) / 2);
					}

					SetDParam(0, t->index);
					SetDParam(1, t->cache.population);
					SetDParam(2, t->cache.potential_pop);
					SetDParam(3, t->cache.num_houses);
					/* CITIES DIFFERENT COLOUR*/
					DrawString(text_left, text_right, y + (this->resize.step_height - FONT_HEIGHT_NORMAL) / 2, t->larger_town ? STR_TOWN_DIRECTORY_CITY_COLOUR : STR_TOWN_DIRECTORY_TOWN_COLOUR);

					y += this->resize.step_height;
					if (++n == this->vscroll->GetCapacity()) break; // max number of towns in 1 window
				}
				break;
			}
		}
	}

	void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize) override
	{
		switch (widget) {
			case WID_TD_SORT_ORDER: {
				Dimension d = GetStringBoundingBox(this->GetWidget<NWidgetCore>(widget)->widget_data);
				d.width += padding.width + Window::SortButtonWidth() * 2; // Doubled since the string is centred and it also looks better.
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}
			case WID_TD_SORT_CRITERIA: {
				Dimension d = {0, 0};
				for (uint i = 0; TownDirectoryWindow::sorter_names[i] != INVALID_STRING_ID; i++) {
					d = maxdim(d, GetStringBoundingBox(TownDirectoryWindow::sorter_names[i]));
				}
				d.width += padding.width;
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}
			case WID_TD_LIST: {
				Dimension d = GetStringBoundingBox(STR_TOWN_DIRECTORY_NONE);
				for (uint i = 0; i < this->towns.size(); i++) {
					const Town *t = this->towns[i];

					assert(t != nullptr);

					SetDParam(0, t->index);
					SetDParamMaxDigits(1, 8);
					SetDParamMaxDigits(2, 5);
					d = maxdim(d, GetStringBoundingBox(STR_TOWN_DIRECTORY_TOWN_COLOUR));
				}
				Dimension icon_size = GetSpriteSize(SPR_TOWN_RATING_GOOD);
				d.width += icon_size.width + 2;
				d.height = max(d.height, icon_size.height);
				resize->height = d.height;
				d.height *= 5;
				d.width += padding.width + WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT;
				d.height += padding.height + WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
				*size = maxdim(*size, d);
				break;
			}
			case WID_TD_WORLD_POPULATION: {
				SetDParamMaxDigits(0, 10);
				Dimension d = GetStringBoundingBox(STR_TOWN_POPULATION);
				d.width += padding.width;
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}
		}
	}

	void OnClick(Point pt, int widget, int click_count) override
	{
		switch (widget) {
			case WID_TD_SORT_ORDER: // Click on sort order button
				if (this->towns.SortType() != 2) { // A different sort than by rating.
					this->towns.ToggleSortOrder();
					this->last_sorting = this->towns.GetListing(); // Store new sorting order.
				} else {
					/* Some parts are always sorted ascending on name. */
					this->last_sorting.order = !this->last_sorting.order;
					this->towns.SetListing(this->last_sorting);
					this->towns.ForceResort();
					this->towns.Sort();
				}
				this->SetDirty();
				break;

			case WID_TD_SORT_CRITERIA: // Click on sort criteria dropdown
				ShowDropDownMenu(this, TownDirectoryWindow::sorter_names, this->towns.SortType(), WID_TD_SORT_CRITERIA, 0, 0);
				break;

			case WID_TD_LIST: { // Click on Town Matrix
				uint id_v = this->vscroll->GetScrolledRowFromWidget(pt.y, this, WID_TD_LIST, WD_FRAMERECT_TOP);
				if (id_v >= this->towns.size()) return; // click out of town bounds

				const Town *t = this->towns[id_v];
				assert(t != nullptr);
				if (_ctrl_pressed) {
					ShowExtraViewPortWindow(t->xy);
				} else {
					ScrollMainWindowToTile(t->xy);
				}
				break;
			}
		}
	}

	void OnDropdownSelect(int widget, int index) override
	{
		if (widget != WID_TD_SORT_CRITERIA) return;

		if (this->towns.SortType() != index) {
			this->towns.SetSortType(index);
			this->last_sorting = this->towns.GetListing(); // Store new sorting order.
			this->BuildSortTownList();
		}
	}

	void OnPaint() override
	{
		if (this->towns.NeedRebuild()) this->BuildSortTownList();
		this->DrawWidgets();
	}

	void OnHundredthTick() override
	{
		this->BuildSortTownList();
		this->SetDirty();
	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_TD_LIST);
	}

	void OnEditboxChanged(int wid) override
	{
		if (wid == WID_TD_FILTER) {
			this->string_filter.SetFilterTerm(this->townname_editbox.text.buf);
			this->InvalidateData(TDIWD_FORCE_REBUILD);
		}
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData(int data = 0, bool gui_scope = true) override
	{
		switch (data) {
			case TDIWD_FORCE_REBUILD:
				/* This needs to be done in command-scope to enforce rebuilding before resorting invalid data */
				this->towns.ForceRebuild();
				break;

			case TDIWD_POPULATION_CHANGE:
				if (this->towns.SortType() == 1) this->towns.ForceResort();
				break;

			default:
				this->towns.ForceResort();
		}
	}
};

Listing TownDirectoryWindow::last_sorting = {false, 0};

/** Names of the sorting functions. */
const StringID TownDirectoryWindow::sorter_names[] = {
	STR_SORT_BY_NAME,
	STR_SORT_BY_POPULATION,
	STR_SORT_BY_REAL_POPULATION,
	STR_SORT_BY_HOUSES,
	STR_SORT_BY_RATING,
	INVALID_STRING_ID
};

/** Available town directory sorting functions. */
GUITownList::SortFunction * const TownDirectoryWindow::sorter_funcs[] = {
	&TownNameSorter,
	&TownPopulationSorter,
	&TownRealPopulationSorter,
	&TownHousesSorter,
	&TownRatingSorter,
};

static WindowDesc _town_directory_desc(
	WDP_AUTO, "list_towns", 208, 202,
	WC_TOWN_DIRECTORY, WC_NONE,
	0,
	_nested_town_directory_widgets, lengthof(_nested_town_directory_widgets)
);

void ShowTownDirectory()
{
	if (BringWindowToFrontById(WC_TOWN_DIRECTORY, 0)) return;
	new TownDirectoryWindow(&_town_directory_desc);
}

void CcFoundTown(const CommandCost &result, TileIndex tile, uint32 p1, uint32 p2, uint32 cmd)
{
	if (result.Failed()) return;

	if (_settings_client.sound.confirm) SndPlayTileFx(SND_1F_SPLAT_OTHER, tile);
	if (!_settings_client.gui.persistent_buildingtools) ResetObjectToPlace();
}

void CcFoundRandomTown(const CommandCost &result, TileIndex tile, uint32 p1, uint32 p2, uint32 cmd)
{
	if (result.Succeeded()) ScrollMainWindowToTile(Town::Get(_new_town_id)->xy);
}

static const NWidgetPart _nested_found_town_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetDataTip(STR_FOUND_TOWN_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	/* Construct new town(s) buttons. */
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN),
		NWidget(NWID_SPACER), SetMinimalSize(0, 2),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_NEW_TOWN), SetMinimalSize(156, 12), SetFill(1, 0),
										SetDataTip(STR_FOUND_TOWN_NEW_TOWN_BUTTON, STR_FOUND_TOWN_NEW_TOWN_TOOLTIP), SetPadding(0, 2, 1, 2),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_TF_RANDOM_TOWN), SetMinimalSize(156, 12), SetFill(1, 0),
										SetDataTip(STR_FOUND_TOWN_RANDOM_TOWN_BUTTON, STR_FOUND_TOWN_RANDOM_TOWN_TOOLTIP), SetPadding(0, 2, 1, 2),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_TF_MANY_RANDOM_TOWNS), SetMinimalSize(156, 12), SetFill(1, 0),
										SetDataTip(STR_FOUND_TOWN_MANY_RANDOM_TOWNS, STR_FOUND_TOWN_RANDOM_TOWNS_TOOLTIP), SetPadding(0, 2, 0, 2),
		/* Town name selection. */
		NWidget(WWT_LABEL, COLOUR_DARK_GREEN), SetMinimalSize(156, 14), SetPadding(0, 2, 0, 2), SetDataTip(STR_FOUND_TOWN_NAME_TITLE, STR_NULL),
		NWidget(WWT_EDITBOX, COLOUR_GREY, WID_TF_TOWN_NAME_EDITBOX), SetMinimalSize(156, 12), SetPadding(0, 2, 3, 2),
										SetDataTip(STR_FOUND_TOWN_NAME_EDITOR_TITLE, STR_FOUND_TOWN_NAME_EDITOR_HELP),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_TF_TOWN_NAME_RANDOM), SetMinimalSize(78, 12), SetPadding(0, 2, 0, 2), SetFill(1, 0),
										SetDataTip(STR_FOUND_TOWN_NAME_RANDOM_BUTTON, STR_FOUND_TOWN_NAME_RANDOM_TOOLTIP),
		/* Town size selection. */
		NWidget(NWID_HORIZONTAL), SetPIP(2, 0, 2),
			NWidget(NWID_SPACER), SetFill(1, 0),
			NWidget(WWT_LABEL, COLOUR_DARK_GREEN), SetMinimalSize(148, 14), SetDataTip(STR_FOUND_TOWN_INITIAL_SIZE_TITLE, STR_NULL),
			NWidget(NWID_SPACER), SetFill(1, 0),
		EndContainer(),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(2, 0, 2),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_SIZE_SMALL), SetMinimalSize(78, 12), SetFill(1, 0),
										SetDataTip(STR_FOUND_TOWN_INITIAL_SIZE_SMALL_BUTTON, STR_FOUND_TOWN_INITIAL_SIZE_TOOLTIP),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_SIZE_MEDIUM), SetMinimalSize(78, 12), SetFill(1, 0),
										SetDataTip(STR_FOUND_TOWN_INITIAL_SIZE_MEDIUM_BUTTON, STR_FOUND_TOWN_INITIAL_SIZE_TOOLTIP),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 1),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(2, 0, 2),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_SIZE_LARGE), SetMinimalSize(78, 12), SetFill(1, 0),
										SetDataTip(STR_FOUND_TOWN_INITIAL_SIZE_LARGE_BUTTON, STR_FOUND_TOWN_INITIAL_SIZE_TOOLTIP),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_SIZE_RANDOM), SetMinimalSize(78, 12), SetFill(1, 0),
										SetDataTip(STR_FOUND_TOWN_SIZE_RANDOM, STR_FOUND_TOWN_INITIAL_SIZE_TOOLTIP),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 3),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_CITY), SetPadding(0, 2, 0, 2), SetMinimalSize(156, 12), SetFill(1, 0),
										SetDataTip(STR_FOUND_TOWN_CITY, STR_FOUND_TOWN_CITY_TOOLTIP), SetFill(1, 0),
		/* Town roads selection. */
		NWidget(NWID_HORIZONTAL), SetPIP(2, 0, 2),
			NWidget(NWID_SPACER), SetFill(1, 0),
			NWidget(WWT_LABEL, COLOUR_DARK_GREEN), SetMinimalSize(148, 14), SetDataTip(STR_FOUND_TOWN_ROAD_LAYOUT, STR_NULL),
			NWidget(NWID_SPACER), SetFill(1, 0),
		EndContainer(),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(2, 0, 2),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_LAYOUT_ORIGINAL), SetMinimalSize(78, 12), SetFill(1, 0), SetDataTip(STR_FOUND_TOWN_SELECT_LAYOUT_ORIGINAL, STR_FOUND_TOWN_SELECT_TOWN_ROAD_LAYOUT),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_LAYOUT_BETTER), SetMinimalSize(78, 12), SetFill(1, 0), SetDataTip(STR_FOUND_TOWN_SELECT_LAYOUT_BETTER_ROADS, STR_FOUND_TOWN_SELECT_TOWN_ROAD_LAYOUT),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 1),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(2, 0, 2),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_LAYOUT_GRID2), SetMinimalSize(78, 12), SetFill(1, 0), SetDataTip(STR_FOUND_TOWN_SELECT_LAYOUT_2X2_GRID, STR_FOUND_TOWN_SELECT_TOWN_ROAD_LAYOUT),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_LAYOUT_GRID3), SetMinimalSize(78, 12), SetFill(1, 0), SetDataTip(STR_FOUND_TOWN_SELECT_LAYOUT_3X3_GRID, STR_FOUND_TOWN_SELECT_TOWN_ROAD_LAYOUT),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 1),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_LAYOUT_RANDOM), SetPadding(0, 2, 0, 2), SetMinimalSize(0, 12), SetFill(1, 0),
										SetDataTip(STR_FOUND_TOWN_SELECT_LAYOUT_RANDOM, STR_FOUND_TOWN_SELECT_TOWN_ROAD_LAYOUT), SetFill(1, 0),
		NWidget(NWID_SPACER), SetMinimalSize(0, 2),
	EndContainer(),
};

/** Found a town window class. */
struct FoundTownWindow : Window {
private:
	TownSize town_size;     ///< Selected town size
	TownLayout town_layout; ///< Selected town layout
	bool city;              ///< Are we building a city?
	QueryString townname_editbox; ///< Townname editbox
	bool townnamevalid;     ///< Is generated town name valid?
	uint32 townnameparts;   ///< Generated town name
	TownNameParams params;  ///< Town name parameters

public:
	FoundTownWindow(WindowDesc *desc, WindowNumber window_number) :
			Window(desc),
			town_size(TSZ_MEDIUM),
			town_layout(_settings_game.economy.town_layout),
			townname_editbox(MAX_LENGTH_TOWN_NAME_CHARS * MAX_CHAR_LENGTH, MAX_LENGTH_TOWN_NAME_CHARS),
			params(_settings_game.game_creation.town_name)
	{
		this->InitNested(window_number);
		this->querystrings[WID_TF_TOWN_NAME_EDITBOX] = &this->townname_editbox;
		this->RandomTownName();
		this->UpdateButtons(true);
	}

	void RandomTownName()
	{
		this->townnamevalid = GenerateTownName(&this->townnameparts);

		if (!this->townnamevalid) {
			this->townname_editbox.text.DeleteAll();
		} else {
			GetTownName(this->townname_editbox.text.buf, &this->params, this->townnameparts, &this->townname_editbox.text.buf[this->townname_editbox.text.max_bytes - 1]);
			this->townname_editbox.text.UpdateSize();
		}
		UpdateOSKOriginalText(this, WID_TF_TOWN_NAME_EDITBOX);

		this->SetWidgetDirty(WID_TF_TOWN_NAME_EDITBOX);
	}

	void UpdateButtons(bool check_availability)
	{
		if (check_availability && _game_mode != GM_EDITOR) {
			this->SetWidgetsDisabledState(true, WID_TF_RANDOM_TOWN, WID_TF_MANY_RANDOM_TOWNS, WID_TF_SIZE_LARGE, WIDGET_LIST_END);
			this->SetWidgetsDisabledState(_settings_game.economy.found_town != TF_CUSTOM_LAYOUT,
					WID_TF_LAYOUT_ORIGINAL, WID_TF_LAYOUT_BETTER, WID_TF_LAYOUT_GRID2, WID_TF_LAYOUT_GRID3, WID_TF_LAYOUT_RANDOM, WIDGET_LIST_END);
			if (_settings_game.economy.found_town != TF_CUSTOM_LAYOUT) town_layout = _settings_game.economy.town_layout;
		}

		for (int i = WID_TF_SIZE_SMALL; i <= WID_TF_SIZE_RANDOM; i++) {
			this->SetWidgetLoweredState(i, i == WID_TF_SIZE_SMALL + this->town_size);
		}

		this->SetWidgetLoweredState(WID_TF_CITY, this->city);

		for (int i = WID_TF_LAYOUT_ORIGINAL; i <= WID_TF_LAYOUT_RANDOM; i++) {
			this->SetWidgetLoweredState(i, i == WID_TF_LAYOUT_ORIGINAL + this->town_layout);
		}

		this->SetDirty();
	}

	void ExecuteFoundTownCommand(TileIndex tile, bool random, StringID errstr, CommandCallback cc)
	{
		const char *name = nullptr;

		if (!this->townnamevalid) {
			name = this->townname_editbox.text.buf;
		} else {
			/* If user changed the name, send it */
			char buf[MAX_LENGTH_TOWN_NAME_CHARS * MAX_CHAR_LENGTH];
			GetTownName(buf, &this->params, this->townnameparts, lastof(buf));
			if (strcmp(buf, this->townname_editbox.text.buf) != 0) name = this->townname_editbox.text.buf;
		}

		bool success = DoCommandP(tile, this->town_size | this->city << 2 | this->town_layout << 3 | random << 6,
				townnameparts, CMD_FOUND_TOWN | CMD_MSG(errstr), cc, name);

		/* Rerandomise name, if success and no cost-estimation. */
		if (success && !_shift_pressed) this->RandomTownName();
	}

	void OnClick(Point pt, int widget, int click_count) override
	{
		switch (widget) {
			case WID_TF_NEW_TOWN:
				HandlePlacePushButton(this, WID_TF_NEW_TOWN, SPR_CURSOR_TOWN, HT_RECT);
				break;

			case WID_TF_RANDOM_TOWN:
				this->ExecuteFoundTownCommand(0, true, STR_ERROR_CAN_T_GENERATE_TOWN, CcFoundRandomTown);
				break;

			case WID_TF_TOWN_NAME_RANDOM:
				this->RandomTownName();
				this->SetFocusedWidget(WID_TF_TOWN_NAME_EDITBOX);
				break;

			case WID_TF_MANY_RANDOM_TOWNS:
				_generating_world = true;
				UpdateNearestTownForRoadTiles(true);
				if (!GenerateTowns(this->town_layout)) {
					ShowErrorMessage(STR_ERROR_CAN_T_GENERATE_TOWN, STR_ERROR_NO_SPACE_FOR_TOWN, WL_INFO);
				}
				UpdateNearestTownForRoadTiles(false);
				_generating_world = false;
				break;

			case WID_TF_SIZE_SMALL: case WID_TF_SIZE_MEDIUM: case WID_TF_SIZE_LARGE: case WID_TF_SIZE_RANDOM:
				this->town_size = (TownSize)(widget - WID_TF_SIZE_SMALL);
				this->UpdateButtons(false);
				break;

			case WID_TF_CITY:
				this->city ^= true;
				this->SetWidgetLoweredState(WID_TF_CITY, this->city);
				this->SetDirty();
				break;

			case WID_TF_LAYOUT_ORIGINAL: case WID_TF_LAYOUT_BETTER: case WID_TF_LAYOUT_GRID2:
			case WID_TF_LAYOUT_GRID3: case WID_TF_LAYOUT_RANDOM:
				this->town_layout = (TownLayout)(widget - WID_TF_LAYOUT_ORIGINAL);
				this->UpdateButtons(false);
				break;
		}
	}

	void OnPlaceObject(Point pt, TileIndex tile) override
	{
		this->ExecuteFoundTownCommand(tile, false, STR_ERROR_CAN_T_FOUND_TOWN_HERE, CcFoundTown);
	}

	void OnPlaceObjectAbort() override
	{
		this->RaiseButtons();
		this->UpdateButtons(false);
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData(int data = 0, bool gui_scope = true) override
	{
		if (!gui_scope) return;
		this->UpdateButtons(true);
	}
};

static WindowDesc _found_town_desc(
	WDP_AUTO, "build_town", 160, 162,
	WC_FOUND_TOWN, WC_NONE,
	WDF_CONSTRUCTION,
	_nested_found_town_widgets, lengthof(_nested_found_town_widgets)
);

void ShowFoundTownWindow()
{
	if (_game_mode != GM_EDITOR && !Company::IsValidID(_local_company)) return;
	AllocateWindowDescFront<FoundTownWindow>(&_found_town_desc, 0);
}

void InitializeTownGui()
{
	_town_local_authority_kdtree.Clear();
}

//CB
static void DrawExtraTownInfo (const Rect &r, uint &y, Town *town, uint line, bool show_house_states_info) {
	//real pop and rating
	SetDParam(0, town->cache.potential_pop);
	SetDParam(1, town->ratings[_current_company]);
	DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_LEFT, y += line, STR_TOWN_VIEW_REALPOP_RATE);
	//town stats
	// int grow_rate = 0;
	// if(town->growth_rate == TOWN_GROW_RATE_CUSTOM_NONE) grow_rate = 0;
	// else grow_rate = TownTicksToDays((town->growth_rate & ~TOWN_GROW_RATE_CUSTOM) + 1);

	SetDParam(0, town->growth_rate);
	SetDParam(1, HasBit(town->flags, TOWN_CUSTOM_GROWTH) ? STR_TOWN_VIEW_GROWTH_RATE_CUSTOM : STR_EMPTY);
	// SetDParam(2, town->grow_counter < 16000 ? TownTicksToDays(town->grow_counter + 1) : -1);
	SetDParam(2, town->grow_counter);
	SetDParam(3, town->time_until_rebuild);
	SetDParam(4, HasBit(town->flags, TOWN_IS_GROWING) ? 1 : 0);
	SetDParam(5, town->fund_buildings_months);
	DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_LEFT, y += line, STR_TOWN_VIEW_GROWTH);

	if (show_house_states_info) {
		SetDParam(0, town->houses_construction);
		SetDParam(1, town->houses_reconstruction);
		SetDParam(2, town->houses_demolished);
		DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_LEFT, y += line, STR_TOWN_VIEW_HOUSE_STATE);
	}

	///houses stats
	SetDParam(0, town->houses_skipped);
	SetDParam(1, town->houses_skipped_last_month);
	SetDParam(2, town->cycles_skipped);
	SetDParam(3, town->cycles_skipped_last_month);
	SetDParam(4, town->cb_houses_removed);
	SetDParam(5, town->cb_houses_removed_last_month);
	DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_LEFT, y += line, STR_TOWN_VIEW_GROWTH_TILES);
}

bool CB_sortCargoesByFrom(CargoX first, CargoX second){
	return (first.from < second.from) ? true : false;
}

struct CBTownWindow : Window {
private:
	Town *town;
	std::list<CargoX> cargoes;

	enum CBTownWindowPlanes {
		CBTWP_MP_GOALS = 0,
		CBTWP_MP_CB,
	};

public:
	CBTownWindow(WindowDesc *desc, WindowNumber window_number) : Window(desc)
	{
		for (uint i = 0; i < NUM_CARGO ; i++) {
			CargoX c;
			c.id = i;
			c.from = CB_GetFrom(i);
			this->cargoes.push_back(c);
		}
		cargoes.sort(CB_sortCargoesByFrom);
		this->town = Town::Get(window_number);
		this->InitNested(window_number);

		if(HasBit(this->town->fund_regularly, _local_company)) this->LowerWidget(WID_CB_FUND_REGULAR);
		if(HasBit(this->town->do_powerfund, _local_company)) this->LowerWidget(WID_CB_POWERFUND);
		if(HasBit(this->town->advertise_regularly, _local_company)) this->LowerWidget(WID_CB_ADVERT_REGULAR);
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case WID_CB_LOCATION:
			case WID_CB_CENTER_VIEW: // scroll to location
				if (_ctrl_pressed) {
					ShowExtraViewPortWindow(this->town->xy);
				}
				else {
					ScrollMainWindowToTile(this->town->xy);
				}
				break;
			case WID_CB_ADVERT:
				TownExecuteAction(this->town, HK_LADVERT);
				break;
			case WID_CB_FUND:
				TownExecuteAction(this->town, HK_FUND);
				break;
			case WID_CB_FUND_REGULAR:
				ToggleBit(this->town->fund_regularly, _local_company);
				this->SetWidgetLoweredState(widget, HasBit(this->town->fund_regularly, _local_company));
				this->SetWidgetDirty(widget);
				break;
			case WID_CB_POWERFUND:
				ToggleBit(this->town->do_powerfund, _local_company);
				this->SetWidgetLoweredState(widget, HasBit(this->town->do_powerfund, _local_company));
				this->SetWidgetDirty(widget);
				break;
			case WID_CB_ADVERT_REGULAR:
				if (!this->town->advertise_regularly) {
					SetDParam(0, ToPercent8(this->town->ad_rating_goal));
					ShowQueryString(STR_JUST_INT, STR_CB_ADVERT_REGULAR_RATING_TO_KEEP,
					                4, this, CS_NUMERAL, QSF_ACCEPT_UNCHANGED);
				} else this->OnQueryTextFinished(NULL);
				break;
			case WID_CB_TOWN_VIEW: // Town view window
				ShowTownViewWindow(this->window_number);
				break;
			case WID_CB_SHOW_AUTHORITY: // town authority
				ShowTownAuthorityWindow(this->window_number);
				break;
		}
	}

	virtual void OnQueryTextFinished(char *str)
	{
		if (str != NULL) SetBit(this->town->advertise_regularly, _local_company);
		else ClrBit(this->town->advertise_regularly, _local_company);
		this->town->ad_ref_goods_entry = NULL;
		this->SetWidgetLoweredState(WID_CB_ADVERT_REGULAR, HasBit(this->town->advertise_regularly, _local_company));
		this->SetWidgetDirty(WID_CB_ADVERT_REGULAR);

		if (str == NULL)
			return;
		uint val = Clamp(StrEmpty(str) ? 0 : strtol(str, NULL, 10), 1, 100);
		this->town->ad_rating_goal = ((val << 8) + 255) / 101;
	}

	virtual void SetStringParameters(int widget) const
	{
		if (widget == WID_TV_CAPTION){
			SetDParam(0, this->town->index);
		}
	}

	const Company *GetCompany() const {
		for (Company *c : Company::Iterate()) {
			if (c->location_of_HQ != INVALID_TILE
			    	&& DistanceMax(c->location_of_HQ, this->town->xy) < 11)
				return c;
		}
		return nullptr;
	}

	void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		static const uint EXP_TOPPADDING = 5;
		static const uint EXP_LINESPACE  = 2; // Amount of vertical space for a horizontal (sub-)total line.

		switch(widget){
			case WID_CB_DETAILS:
				size->height = (FONT_HEIGHT_NORMAL + EXP_LINESPACE) * 5;
				break;
			case WID_CB_GOALS: {
				uint desired_height = 0;
				auto company = GetCompany();
				if (company) {
					for(const Goal *g : Goal::Iterate()) {
						if (g->company == company->index) {
							desired_height++;
						}
					}
				}
				if (desired_height > 0)
					size->height = desired_height * (FONT_HEIGHT_NORMAL + EXP_LINESPACE) + EXP_TOPPADDING - EXP_LINESPACE;
				break;
			}
			case WID_CB_CARGO_NAME:
			case WID_CB_CARGO_AMOUNT:
			case WID_CB_CARGO_REQ:
			case WID_CB_CARGO_STORE:
			case WID_CB_CARGO_STORE_PCT:
			case WID_CB_CARGO_FROM:
			case WID_CB_CARGO_PREVIOUS: {
				uint desired_height = 0;
				for(CargoID cargo = 0; cargo < NUM_CARGO; cargo++){
					if(CB_GetReq(cargo) > 0) desired_height++;
				}
				if (desired_height > 0)
					size->height = desired_height * (FONT_HEIGHT_NORMAL + EXP_LINESPACE) + EXP_TOPPADDING - EXP_LINESPACE;
				break;
			}
		}
	}

	void DrawWidget(const Rect &r, int widget) const
	{
		static const uint EXP_LINESPACE  = FONT_HEIGHT_NORMAL + 2;
		uint y = r.top + WD_FRAMERECT_TOP;
		switch(widget){
			case WID_CB_DETAILS:{
				//growing
				if(this->town->cb.growth_state == TownGrowthState::GROWING) DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_LEFT, y, STR_TOWN_CB_GROWING );
				else DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_LEFT, y, STR_TOWN_CB_NOT_GROWING );
				//population
				SetDParam(0, this->town->cache.population);
				SetDParam(1, this->town->cache.num_houses);
				DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_LEFT, y += EXP_LINESPACE, STR_TOWN_VIEW_POPULATION_HOUSES);

				DrawExtraTownInfo(r, y, this->town, EXP_LINESPACE, false);
				//regular funding
				if(this->town->fund_regularly != 0){
					DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_LEFT, y += EXP_LINESPACE, STR_CB_FUNDED_REGULARLY);
				}
				break;
			}
			case WID_CB_GOALS: {
				auto company = GetCompany();
				if (!company) break;
				for(const Goal *g : Goal::Iterate()) {
					if (g->company != company->index)
						continue;
					SetDParamStr(0, g->text);
					DrawString(r.left + WD_FRAMERECT_LEFT,
					           r.right - WD_FRAMERECT_RIGHT,
					           y, STR_GOALS_TEXT);
					y += EXP_LINESPACE;
				}
				break;
			}
			/* Citybuilder things*/
			case WID_CB_CARGO_NAME:
			case WID_CB_CARGO_AMOUNT:
			case WID_CB_CARGO_REQ:
			case WID_CB_CARGO_STORE:
			case WID_CB_CARGO_STORE_PCT:
			case WID_CB_CARGO_FROM:
			case WID_CB_CARGO_PREVIOUS: {
				if (this->town->larger_town) break;
				uint delivered;
				uint requirements;
				uint from;
				StringID string_to_draw;

				//for cycle
				std::list<CargoX> cargoes2 = this->cargoes;
				std::list<CargoX>::iterator it2;
				for (it2 = cargoes2.begin(); it2 != cargoes2.end(); ++it2) {
					CargoX cargox;
					cargox = *it2;
					if (it2 == cargoes2.begin()) { //header
						DrawString(r.left + WD_FRAMERECT_LEFT + 14, r.right - WD_FRAMERECT_LEFT, y,
							(STR_TOWN_GROWTH_HEADER_CARGO + widget - WID_CB_CARGO_NAME), TC_FROMSTRING,
							(widget == WID_CB_CARGO_NAME) ? SA_LEFT : SA_RIGHT);

						y += (FONT_HEIGHT_NORMAL + 2);
					}

					const CargoSpec *cargos = CargoSpec::Get(cargox.id);
					//cargo needed?
					if (!cargos->IsValid() || CB_GetReq(cargos->Index()) == 0) continue;

					from = CB_GetFrom(cargos->Index());

					switch(widget) {
						case WID_CB_CARGO_NAME: {
							int rect_x = (r.left + WD_FRAMERECT_LEFT);
							GfxFillRect(rect_x, y + 1, rect_x + 8, y + 6, 0);
							GfxFillRect(rect_x + 1, y + 2, rect_x + 7, y + 5, cargos->legend_colour);

							SetDParam(0, cargos->name);
							DrawString(r.left + WD_FRAMERECT_LEFT + 14, r.right - WD_FRAMERECT_LEFT, y, STR_TOWN_CB_CARGO_NAME);
							break;
						}
						case WID_CB_CARGO_AMOUNT: {
							delivered = this->town->cb.delivered[cargos->Index()];
							requirements = CB_GetTownReq(this->town->cache.population, CB_GetReq(cargos->Index()), from, true);
							SetDParam(0, delivered);

							//when required
							if (this->town->cache.population >= from) {
								if((delivered + (uint)this->town->cb.stored[cargos->Index()]) >= requirements) string_to_draw = STR_TOWN_CB_CARGO_AMOUNT_GOOD;
								else string_to_draw = STR_TOWN_CB_CARGO_AMOUNT_BAD;
							}
							//when not required -> all faded
							else string_to_draw = STR_TOWN_CB_CARGO_AMOUNT_NOT;

							DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_LEFT, y, string_to_draw, TC_FROMSTRING, SA_RIGHT);
							break;
						}
						case WID_CB_CARGO_REQ: {
							requirements = CB_GetTownReq(this->town->cache.population, CB_GetReq(cargos->Index()), from, true);
							SetDParam(0, requirements);
							 //when required
							string_to_draw = (this->town->cache.population >= from) ? STR_TOWN_CB_CARGO_REQ_YES : STR_TOWN_CB_CARGO_REQ_NOT;
							DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_LEFT, y, string_to_draw, TC_FROMSTRING, SA_RIGHT);
							break;
						}
						case WID_CB_CARGO_PREVIOUS: {
							requirements = CB_GetTownReq(this->town->cache.population, CB_GetReq(cargos->Index()), from, true);
							SetDParam(0, this->town->cb.delivered_last_month[cargos->Index()]);
							if (this->town->cache.population >= from){
								// if (this->town->delivered_enough[cargos->Index()]) {
									string_to_draw = (this->town->cb.delivered_last_month[cargos->Index()] >= requirements) ? STR_TOWN_CB_CARGO_PREVIOUS_YES : STR_TOWN_CB_CARGO_PREVIOUS_EDGE;
								// }
								// else string_to_draw = STR_TOWN_CB_CARGO_PREVIOUS_BAD;
							}
							else string_to_draw = STR_TOWN_CB_CARGO_PREVIOUS_NOT;

							DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_LEFT, y, string_to_draw, TC_FROMSTRING, SA_RIGHT);
							break;
						}
						case WID_CB_CARGO_STORE: {
							SetDParam(0, this->town->cb.stored[cargos->Index()]);
							if (CB_GetDecay(cargos->Index()) == 100) string_to_draw = STR_TOWN_CB_CARGO_STORE_DECAY;  //when 100% decay
							else {
								if (this->town->cache.population >= from) string_to_draw = STR_TOWN_CB_CARGO_STORE_YES;  //when required
								else string_to_draw = STR_TOWN_CB_CARGO_STORE_NOT;
							}

							DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_LEFT, y, string_to_draw, TC_FROMSTRING, SA_RIGHT);
							break;
						}
						case WID_CB_CARGO_STORE_PCT: {
							uint max_storage = CB_GetMaxTownStorage(this->town, cargos->Index());
							if (CB_GetDecay(cargos->Index()) == 100 || !max_storage) string_to_draw = STR_TOWN_CB_CARGO_STORE_DECAY;  //when 100% decay
							else {
								SetDParam(0, 100 * this->town->cb.stored[cargos->Index()] / max_storage);
								if (this->town->cache.population >= from) string_to_draw = STR_TOWN_CB_CARGO_STORE_PCT_YES;  //when required
								else string_to_draw = STR_TOWN_CB_CARGO_STORE_PCT_NOT;
							}

							DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_LEFT, y, string_to_draw, TC_FROMSTRING, SA_RIGHT);
							break;
						}
						case WID_CB_CARGO_FROM: {
							SetDParam(0, from);
							string_to_draw = (this->town->cache.population >= from) ? STR_TOWN_CB_CARGO_FROM_YES : STR_TOWN_CB_CARGO_FROM_NOT; //when required

							DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, string_to_draw, TC_FROMSTRING, SA_RIGHT);
							break;
						}
						//last case
					}
					//switch
					y += (FONT_HEIGHT_NORMAL + 2);
					//cargo needed?
				}
				//for cycle
			}
			break;
		}
		/* Citybuilder things enabled*/
	}

	virtual void OnPaint() {
		if (!this->IsShaded()) {
			int plane = CB_Enabled() ? CBTWP_MP_CB : CBTWP_MP_GOALS;
			NWidgetStacked *wi = this->GetWidget<NWidgetStacked>(WID_CB_SELECT_REQUIREMENTS);
			if (plane != wi->shown_plane) {
				wi->SetDisplayedPlane(plane);
				this->InvalidateData();
				return;
			}
		}
		this->DrawWidgets();
	}


	virtual EventState OnHotkey(int hotkey)
	{
		TownExecuteAction(this->town, hotkey);
		return ES_HANDLED;
	}

	static HotkeyList hotkeys;
};

HotkeyList CBTownWindow::hotkeys("town_gui", town_hotkeys);

static const NWidgetPart _nested_cb_town_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN, WID_TV_CAPTION), SetDataTip(STR_TOWN_VIEW_CB_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_SPACER), SetMinimalSize(0, 5),  SetResize(1, 0), SetFill(1, 0),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_EMPTY, COLOUR_BROWN, WID_CB_DETAILS), SetMinimalSize(66, 0), SetResize(1, 0), SetFill(1, 0),
				NWidget(NWID_VERTICAL),
					NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
						NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_CB_ADVERT),SetMinimalSize(33, 12),SetFill(1, 0), SetDataTip(STR_CB_LARGE_ADVERTISING_CAMPAIGN, 0),
						NWidget(NWID_SPACER), SetMinimalSize(2, 0),
						NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_CB_FUND),SetMinimalSize(33, 12),SetFill(1, 0), SetDataTip(STR_CB_NEW_BUILDINGS, 0),
						NWidget(NWID_SPACER), SetMinimalSize(4, 0),
					EndContainer(),
					NWidget(NWID_SPACER), SetMinimalSize(0, 2),
					NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
						NWidget(WWT_TEXTBTN, COLOUR_BROWN, WID_CB_ADVERT_REGULAR),SetMinimalSize(33, 12),SetFill(1, 0), SetDataTip(STR_CB_ADVERT_REGULAR, STR_CB_ADVERT_REGULAR_TT),
 						NWidget(NWID_SPACER), SetMinimalSize(2, 0),
						NWidget(WWT_TEXTBTN, COLOUR_BROWN, WID_CB_FUND_REGULAR),SetMinimalSize(33, 12),SetFill(1, 0), SetDataTip(STR_CB_FUND_REGULAR, STR_CB_FUND_REGULAR_TT),
						NWidget(NWID_SPACER), SetMinimalSize(4, 0),
					EndContainer(),
					NWidget(NWID_SPACER), SetMinimalSize(0, 2),
					NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
  						NWidget(NWID_SPACER), SetMinimalSize(33, 0), SetFill(1, 0),
  						NWidget(NWID_SPACER), SetMinimalSize(2, 0),
						NWidget(WWT_TEXTBTN, COLOUR_BROWN, WID_CB_POWERFUND),SetMinimalSize(33, 12),SetFill(1, 0), SetDataTip(STR_CB_POWERFUND, STR_CB_POWERFUND_TT),
						NWidget(NWID_SPACER), SetMinimalSize(4, 0),
					EndContainer(),
				EndContainer(),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(0, 5),  SetResize(1, 0), SetFill(1, 0),
			NWidget(NWID_SELECTION, INVALID_COLOUR, WID_CB_SELECT_REQUIREMENTS),
				NWidget(WWT_EMPTY, COLOUR_BROWN, WID_CB_GOALS),SetMinimalSize(50 + 35 + 35 + 40 + 35 + 30, 0), SetResize(1, 0), SetFill(1, 0),
				NWidget(NWID_HORIZONTAL),
					NWidget(WWT_EMPTY, COLOUR_BROWN, WID_CB_CARGO_NAME),SetMinimalSize(50, 0), SetResize(0, 0), SetFill(1, 0),
					NWidget(WWT_EMPTY, COLOUR_BROWN, WID_CB_CARGO_AMOUNT),SetMinimalSize(35, 0), SetResize(1, 0), SetFill(0, 0),
					NWidget(WWT_EMPTY, COLOUR_BROWN, WID_CB_CARGO_REQ),SetMinimalSize(35, 0), SetResize(1, 0), SetFill(0, 0),
					NWidget(WWT_EMPTY, COLOUR_BROWN, WID_CB_CARGO_PREVIOUS),SetMinimalSize(40, 0),  SetResize(1, 0), SetFill(0, 0),
					NWidget(WWT_EMPTY, COLOUR_BROWN, WID_CB_CARGO_STORE),SetMinimalSize(35, 0), SetResize(1, 0), SetFill(0, 0),
					NWidget(WWT_EMPTY, COLOUR_BROWN, WID_CB_CARGO_STORE_PCT),SetMinimalSize(30, 0), SetResize(1, 0), SetFill(0, 0),
				EndContainer(),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(0, 0), SetResize(1, 0), SetFill(1, 1),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_CB_CENTER_VIEW), SetMinimalSize(30, 12), SetFill(1, 1), SetResize(1, 0), SetDataTip(STR_BUTTON_LOCATION, STR_TOWN_VIEW_CENTER_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_CB_TOWN_VIEW), SetMinimalSize(40, 12), SetFill(1, 1), SetResize(1, 0), SetDataTip(STR_CB_GUI_TOWN_VIEW_BUTTON, STR_CB_GUI_TOWN_VIEW_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_CB_SHOW_AUTHORITY), SetMinimalSize(40, 12), SetFill(1, 1), SetResize(1, 0), SetDataTip(STR_TOWN_VIEW_LOCAL_AUTHORITY_BUTTON, STR_TOWN_VIEW_LOCAL_AUTHORITY_TOOLTIP),
		EndContainer(),
		NWidget(WWT_RESIZEBOX, COLOUR_BROWN),
	EndContainer(),
};

static WindowDesc _cb_town_desc(
	WDP_AUTO, "cb_town", 160, 30,
	WC_CB_TOWN, WC_NONE,
	0,
	_nested_cb_town_widgets, lengthof(_nested_cb_town_widgets),
	&CBTownWindow::hotkeys
);

void ShowCBTownWindow(uint town) {
	AllocateWindowDescFront<CBTownWindow>(&_cb_town_desc, town);
}
