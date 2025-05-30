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
#include "house.h"
#include "newgrf_cargo.h"
#include "newgrf_house.h"
#include "newgrf_text.h"
#include "picker_gui.h"
#include "command_func.h"
#include "company_func.h"
#include "company_base.h"
#include "company_gui.h"
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
#include "core/backup_type.hpp"
#include "core/geometry_func.hpp"
#include "genworld.h"
#include "fios.h"
#include "stringfilter_type.h"
#include "dropdown_func.h"
#include "town_kdtree.h"
#include "town_cmd.h"
#include "timer/timer.h"
#include "timer/timer_game_calendar.h"
#include "timer/timer_window.h"
#include "zoom_func.h"
#include "hotkeys.h"

#include "widgets/town_widget.h"

#include "table/strings.h"

#include <optional>
#include <sstream>

/* CityMania start */
#include "goal_base.h"
#include "hotkeys.h"
#include <list>
#include "console_func.h"

#include "citymania/cm_commands.hpp"
#include "citymania/cm_hotkeys.hpp"
/* CityMania end */

#include "safeguards.h"

struct CargoX {
	int id;
	int from;
};
void ShowCBTownWindow(uint town);
static void DrawExtraTownInfo (Rect &r, Town *town, uint line, bool show_house_states_info=false);

bool TownExecuteAction(const Town *town, uint action){
	if(!(action == HK_STATUE && HasBit(town->statues, _current_company))){ //don't built statue when there is one
		return citymania::cmd::DoTownAction(town->xy, town->index, action)
			.with_error(STR_ERROR_CAN_T_DO_THIS)
			.post();
	}
	return false;
}

TownKdtree _town_local_authority_kdtree{};

typedef GUIList<const Town*, const bool &> GUITownList;

static constexpr NWidgetPart _nested_town_authority_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN, WID_TA_CAPTION), SetDataTip(STR_LOCAL_AUTHORITY_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_TEXTBTN, COLOUR_BROWN, WID_TA_ZONE_BUTTON), SetMinimalSize(50, 0), SetDataTip(STR_LOCAL_AUTHORITY_ZONE, STR_LOCAL_AUTHORITY_ZONE_TOOLTIP),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, WID_TA_RATING_INFO), SetMinimalSize(317, 92), SetResize(1, 1), EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, WID_TA_COMMAND_LIST), SetMinimalSize(317, 52), SetResize(1, 0), SetDataTip(0x0, STR_LOCAL_AUTHORITY_ACTIONS_TOOLTIP), EndContainer(),
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
	uint displayed_actions_on_previous_painting; ///< Actions that were available on the previous call to OnPaint()
	TownActions enabled_actions; ///< Actions that are enabled in settings.
	TownActions available_actions; ///< Actions that are available to execute for the current company.
	StringID action_tooltips[TACT_COUNT];

	Dimension icon_size;      ///< Dimensions of company icon
	Dimension exclusive_size; ///< Dimensions of exlusive icon

	/**
	 * Get the position of the Nth set bit.
	 *
	 * If there is no Nth bit set return -1
	 *
	 * @param n The Nth set bit from which we want to know the position
	 * @return The position of the Nth set bit, or -1 if no Nth bit set.
	 */
	int GetNthSetBit(int n)
	{
		if (n >= 0) {
			for (uint i : SetBitIterator(this->enabled_actions)) {
				n--;
				if (n < 0) return i;
			}
		}
		return -1;
	}

	/**
	 * Gets all town authority actions enabled in settings.
	 *
	 * @return Bitmask of actions enabled in the settings.
	 */
	static TownActions GetEnabledActions()
	{
		TownActions enabled = TACT_ALL;

		if (!_settings_game.economy.fund_roads) CLRBITS(enabled, TACT_ROAD_REBUILD);
		if (!_settings_game.economy.fund_buildings) CLRBITS(enabled, TACT_FUND_BUILDINGS);
		if (!_settings_game.economy.exclusive_rights) CLRBITS(enabled, TACT_BUY_RIGHTS);
		if (!_settings_game.economy.bribe) CLRBITS(enabled, TACT_BRIBE);

		return enabled;
	}

public:
	TownAuthorityWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc), sel_index(-1), displayed_actions_on_previous_painting(0), available_actions(TACT_NONE)
	{
		this->town = Town::Get(window_number);
		this->enabled_actions = GetEnabledActions();

		auto realtime = TimerGameEconomy::UsingWallclockUnits();
		this->action_tooltips[0] = STR_LOCAL_AUTHORITY_ACTION_TOOLTIP_SMALL_ADVERTISING;
		this->action_tooltips[1] = STR_LOCAL_AUTHORITY_ACTION_TOOLTIP_MEDIUM_ADVERTISING;
		this->action_tooltips[2] = STR_LOCAL_AUTHORITY_ACTION_TOOLTIP_LARGE_ADVERTISING;
		this->action_tooltips[3] = realtime ? STR_LOCAL_AUTHORITY_ACTION_TOOLTIP_ROAD_RECONSTRUCTION_MINUTES : STR_LOCAL_AUTHORITY_ACTION_TOOLTIP_ROAD_RECONSTRUCTION_MONTHS;
		this->action_tooltips[4] = STR_LOCAL_AUTHORITY_ACTION_TOOLTIP_STATUE_OF_COMPANY;
		this->action_tooltips[5] = STR_LOCAL_AUTHORITY_ACTION_TOOLTIP_NEW_BUILDINGS;
		this->action_tooltips[6] = realtime ? STR_LOCAL_AUTHORITY_ACTION_TOOLTIP_EXCLUSIVE_TRANSPORT_MINUTES : STR_LOCAL_AUTHORITY_ACTION_TOOLTIP_EXCLUSIVE_TRANSPORT_MONTHS;
		this->action_tooltips[7] = STR_LOCAL_AUTHORITY_ACTION_TOOLTIP_BRIBE;

		this->InitNested(window_number);
	}

	void OnInit() override
	{
		this->icon_size      = GetSpriteSize(SPR_COMPANY_ICON);
		this->exclusive_size = GetSpriteSize(SPR_EXCLUSIVE_TRANSPORT);
	}

	void OnPaint() override
	{
		this->available_actions = GetMaskOfTownActions(_local_company, this->town);
		if (this->available_actions != displayed_actions_on_previous_painting) this->SetDirty();
		displayed_actions_on_previous_painting = this->available_actions;

		this->SetWidgetLoweredState(WID_TA_ZONE_BUTTON, this->town->show_zone);
		this->SetWidgetDisabledState(WID_TA_EXECUTE, (this->sel_index == -1) || !HasBit(this->available_actions, this->sel_index));

		this->DrawWidgets();
		if (!this->IsShaded())
		{
			this->DrawRatings();
			this->DrawActions();
		}
	}

	/** Draw the contents of the ratings panel. May request a resize of the window if the contents does not fit. */
	void DrawRatings()
	{
		Rect r = this->GetWidget<NWidgetBase>(WID_TA_RATING_INFO)->GetCurrentRect().Shrink(WidgetDimensions::scaled.framerect);

		int text_y_offset      = (this->resize.step_height - GetCharacterHeight(FS_NORMAL)) / 2;
		int icon_y_offset      = (this->resize.step_height - this->icon_size.height) / 2;
		int exclusive_y_offset = (this->resize.step_height - this->exclusive_size.height) / 2;

		DrawString(r.left, r.right, r.top + text_y_offset, STR_LOCAL_AUTHORITY_COMPANY_RATINGS);
		r.top += this->resize.step_height;

		bool rtl = _current_text_dir == TD_RTL;
		Rect icon      = r.WithWidth(this->icon_size.width, rtl);
		Rect exclusive = r.Indent(this->icon_size.width + WidgetDimensions::scaled.hsep_normal, rtl).WithWidth(this->exclusive_size.width, rtl);
		Rect text      = r.Indent(this->icon_size.width + WidgetDimensions::scaled.hsep_normal + this->exclusive_size.width + WidgetDimensions::scaled.hsep_normal, rtl);

		/* Draw list of companies */
		for (const Company *c : Company::Iterate()) {
			if ((HasBit(this->town->have_ratings, c->index) || this->town->exclusivity == c->index)) {
				DrawCompanyIcon(c->index, icon.left, text.top + icon_y_offset);

				SetDParam(0, c->index);
				SetDParam(1, c->index);

				int rating = this->town->ratings[c->index];
				StringID str = STR_CARGO_RATING_APPALLING;
				if (rating > RATING_APPALLING) str++;
				if (rating > RATING_VERYPOOR)  str++;
				if (rating > RATING_POOR)      str++;
				if (rating > RATING_MEDIOCRE)  str++;
				if (rating > RATING_GOOD)      str++;
				if (rating > RATING_VERYGOOD)  str++;
				if (rating > RATING_EXCELLENT) str++;

				SetDParam(2, str);
				SetDParam(3, this->town->ratings[c->index]);
				if (this->town->exclusivity == c->index) {
					DrawSprite(SPR_EXCLUSIVE_TRANSPORT, COMPANY_SPRITE_COLOUR(c->index), exclusive.left, text.top + exclusive_y_offset);
				}

				DrawString(text.left, text.right, text.top + text_y_offset, CM_STR_LOCAL_AUTHORITY_COMPANY_RATING_NUM);
				text.top += this->resize.step_height;
			}
		}

		text.bottom = text.top - 1;
		if (text.bottom > r.bottom) {
			/* If the company list is too big to fit, mark ourself dirty and draw again. */
			ResizeWindow(this, 0, text.bottom - r.bottom, false);
		}
	}

	/** Draws the contents of the actions panel. May re-initialise window to resize panel, if the list does not fit. */
	void DrawActions()
	{
		Rect r = this->GetWidget<NWidgetBase>(WID_TA_COMMAND_LIST)->GetCurrentRect().Shrink(WidgetDimensions::scaled.framerect);

		DrawString(r, STR_LOCAL_AUTHORITY_ACTIONS_TITLE);
		r.top += GetCharacterHeight(FS_NORMAL);

		/* Draw list of actions */
		for (int i = 0; i < TACT_COUNT; i++) {
			/* Don't show actions if disabled in settings. */
			if (!HasBit(this->enabled_actions, i)) continue;

			/* Set colour of action based on ability to execute and if selected. */
			TextColour action_colour = TC_GREY | TC_NO_SHADE;
			if (HasBit(this->available_actions, i)) action_colour = TC_ORANGE;
			if (this->sel_index == i) action_colour = TC_WHITE;

			DrawString(r, STR_LOCAL_AUTHORITY_ACTION_SMALL_ADVERTISING_CAMPAIGN + i, action_colour);
			r.top += GetCharacterHeight(FS_NORMAL);
		}
	}

	void SetStringParameters(WidgetID widget) const override
	{
		if (widget == WID_TA_CAPTION) SetDParam(0, this->window_number);
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_TA_ACTION_INFO:
				if (this->sel_index != -1) {
					Money action_cost = _price[PR_TOWN_ACTION] * _town_action_costs[this->sel_index] >> 8;
					bool affordable = Company::IsValidID(_local_company) && action_cost < GetAvailableMoney(_local_company);

					SetDParam(0, action_cost);
					DrawStringMultiLine(r.Shrink(WidgetDimensions::scaled.framerect),
						this->action_tooltips[this->sel_index],
						affordable ? TC_YELLOW : TC_RED);
				}
				break;
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_TA_ACTION_INFO: {
				assert(size.width > padding.width && size.height > padding.height);
				Dimension d = {0, 0};
				for (int i = 0; i < TACT_COUNT; i++) {
					SetDParam(0, _price[PR_TOWN_ACTION] * _town_action_costs[i] >> 8);
					d = maxdim(d, GetStringMultiLineBoundingBox(this->action_tooltips[i], size));
				}
				d.width += padding.width;
				d.height += padding.height;
				size = maxdim(size, d);
				break;
			}

			case WID_TA_COMMAND_LIST:
				size.height = (TACT_COUNT + 1) * GetCharacterHeight(FS_NORMAL) + padding.height;
				size.width = GetStringBoundingBox(STR_LOCAL_AUTHORITY_ACTIONS_TITLE).width;
				for (uint i = 0; i < TACT_COUNT; i++ ) {
					size.width = std::max(size.width, GetStringBoundingBox(STR_LOCAL_AUTHORITY_ACTION_SMALL_ADVERTISING_CAMPAIGN + i).width + padding.width);
				}
				size.width += padding.width;
				break;

			case WID_TA_RATING_INFO:
				resize.height = std::max({this->icon_size.height + WidgetDimensions::scaled.vsep_normal, this->exclusive_size.height + WidgetDimensions::scaled.vsep_normal, (uint)GetCharacterHeight(FS_NORMAL)});
				size.height = 9 * resize.height + padding.height;
				break;
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
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
				int y = this->GetRowFromWidget(pt.y, WID_TA_COMMAND_LIST, 1, GetCharacterHeight(FS_NORMAL)) - 1;

				y = GetNthSetBit(y);
				if (y >= 0) {
					this->sel_index = y;
					this->SetDirty();
				}

				/* When double-clicking, continue */
				if (click_count == 1 || y < 0 || !HasBit(this->available_actions, y)) break;
				[[fallthrough]];
			}

			case WID_TA_EXECUTE:
				Command<CMD_DO_TOWN_ACTION>::Post(STR_ERROR_CAN_T_DO_THIS, this->town->xy, this->window_number, this->sel_index);
				break;
		}
	}

	/** Redraw the whole window on a regular interval. */
	IntervalTimer<TimerWindow> redraw_interval = {std::chrono::seconds(3), [this](auto) {
		this->SetDirty();
	}};

	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;

		this->enabled_actions = this->GetEnabledActions();
		if (!HasBit(this->enabled_actions, this->sel_index)) {
			this->sel_index = -1;
		}
	}

	EventState OnHotkey(int hotkey) override
	{
		TownExecuteAction(this->town, hotkey);
		return ES_HANDLED;
	}

	static inline HotkeyList hotkeys{"cm_town_authority", {
		Hotkey((uint16)0, "small_advert", HK_SADVERT),
		Hotkey((uint16)0, "medium_advert", HK_MADVERT),
		Hotkey(WKC_CTRL | 'D', "large_advert", HK_LADVERT),
		Hotkey(WKC_CTRL | 'S', "build_statue", HK_STATUE),
		Hotkey(WKC_CTRL | 'F', "fund_buildings", HK_FUND)
	}};
};

static WindowDesc _town_authority_desc(
	WDP_AUTO, "view_town_authority", 317, 222,
	WC_TOWN_AUTHORITY, WC_NONE,
	0,
	_nested_town_authority_widgets,
	&TownAuthorityWindow::hotkeys
);

static void ShowTownAuthorityWindow(uint town)
{
	AllocateWindowDescFront<TownAuthorityWindow>(_town_authority_desc, town);
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

	TownViewWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc)
	{
		this->CreateNestedTree();

		this->town = Town::Get(window_number);
		if (this->town->larger_town) this->GetWidget<NWidgetCore>(WID_TV_CAPTION)->widget_data = STR_TOWN_VIEW_CITY_CAPTION;

		this->FinishInitNested(window_number);

		this->flags |= WF_DISABLE_VP_SCROLL;
		NWidgetViewport *nvp = this->GetWidget<NWidgetViewport>(WID_TV_VIEWPORT);
		nvp->InitializeViewport(this, this->town->xy, ScaleZoomGUI(ZOOM_LVL_TOWN));

		/* disable renaming town in network games if you are not the server */
		this->SetWidgetDisabledState(WID_TV_CHANGE_NAME, _networking && !_network_server);
		this->SetWidgetDisabledState(WID_TV_CB, _game_mode == GM_EDITOR);
	}

	void Close([[maybe_unused]] int data = 0) override
	{
		SetViewportCatchmentTown(Town::Get(this->window_number), false);
		this->Window::Close();
	}

	void SetStringParameters(WidgetID widget) const override
	{
		if (widget == WID_TV_CAPTION){
			SetDParam(0, this->town->index);
		}
		if (widget == WID_TV_CB){
			// if(this->wcb_disable) SetDParam(0, STR_EMPTY);
			// else
			SetDParam(0, CM_STR_BUTTON_CB_YES);
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

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_TV_INFO) return;

		Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);

		SetDParam(0, this->town->cache.population);
		SetDParam(1, this->town->cache.num_houses);
		DrawString(tr, STR_TOWN_VIEW_POPULATION_HOUSES);
		tr.top += GetCharacterHeight(FS_NORMAL);

		StringID str_last_period = TimerGameEconomy::UsingWallclockUnits() ? STR_TOWN_VIEW_CARGO_LAST_MINUTE_MAX : STR_TOWN_VIEW_CARGO_LAST_MONTH_MAX;

		for (auto tpe : {TPE_PASSENGERS, TPE_MAIL}) {
			for (const CargoSpec *cs : CargoSpec::town_production_cargoes[tpe]) {
				CargoID cid = cs->Index();
				SetDParam(0, 1ULL << cid);
				SetDParam(1, this->town->supplied[cid].old_act);
				SetDParam(2, this->town->supplied[cid].old_max);
				DrawString(tr, str_last_period);
				tr.top += GetCharacterHeight(FS_NORMAL);
			}
		}

		DrawExtraTownInfo(tr, this->town, GetCharacterHeight(FS_NORMAL), true);  // CM (CB)

		bool first = true;
		for (int i = TAE_BEGIN; i < TAE_END; i++) {
			if (this->town->goal[i] == 0) continue;
			if (this->town->goal[i] == TOWN_GROWTH_WINTER && (TileHeight(this->town->xy) < LowestSnowLine() || this->town->cache.population <= 90)) continue;
			if (this->town->goal[i] == TOWN_GROWTH_DESERT && (GetTropicZone(this->town->xy) != TROPICZONE_DESERT || this->town->cache.population <= 60)) continue;

			if (first) {
				DrawString(tr, STR_TOWN_VIEW_CARGO_FOR_TOWNGROWTH);
				tr.top += GetCharacterHeight(FS_NORMAL);
				first = false;
			}

			bool rtl = _current_text_dir == TD_RTL;

			const CargoSpec *cargo = FindFirstCargoWithTownAcceptanceEffect((TownAcceptanceEffect)i);
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
			DrawString(tr.Indent(20, rtl), string);
			tr.top += GetCharacterHeight(FS_NORMAL);
		}

		if (HasBit(this->town->flags, TOWN_IS_GROWING)) {
			SetDParam(0, RoundDivSU(this->town->growth_rate + 1, Ticks::DAY_TICKS));
			DrawString(tr, this->town->fund_buildings_months == 0 ? STR_TOWN_VIEW_TOWN_GROWS_EVERY : STR_TOWN_VIEW_TOWN_GROWS_EVERY_FUNDED);
			tr.top += GetCharacterHeight(FS_NORMAL);
		} else {
			DrawString(tr, STR_TOWN_VIEW_TOWN_GROW_STOPPED);
			tr.top += GetCharacterHeight(FS_NORMAL);
		}

		/* only show the town noise, if the noise option is activated. */
		if (_settings_game.economy.station_noise_level) {
			SetDParam(0, this->town->noise_reached);
			SetDParam(1, this->town->MaxTownNoise());
			DrawString(tr, STR_TOWN_VIEW_NOISE_IN_TOWN);
			tr.top += GetCharacterHeight(FS_NORMAL);
		}

		if (!this->town->text.empty()) {
			SetDParamStr(0, this->town->text);
			tr.top = DrawStringMultiLine(tr, STR_JUST_RAW_STRING, TC_BLACK);
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_TV_CENTER_VIEW: // scroll to location
				if (citymania::_fn_mod) {
					ShowExtraViewportWindow(this->town->xy);
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
				Command<CMD_EXPAND_TOWN>::Post(STR_ERROR_CAN_T_EXPAND_TOWN, this->window_number, 0);
				break;
			}

			case WID_TV_CB:
				// if(_networking)
				ShowCBTownWindow(this->window_number);
				break;

			case WID_TV_DELETE: // delete town - only available on Scenario editor
				Command<CMD_DELETE_TOWN>::Post(STR_ERROR_TOWN_CAN_T_DELETE, this->window_number);
				break;
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_TV_INFO:
				size.height = GetDesiredInfoHeight(size.width) + padding.height;
				break;
		}
	}

	/**
	 * Gets the desired height for the information panel.
	 * @return the desired height in pixels.
	 */
	uint GetDesiredInfoHeight(int width) const
	{
		uint aimed_height = static_cast<uint>(1 + CargoSpec::town_production_cargoes[TPE_PASSENGERS].size() + CargoSpec::town_production_cargoes[TPE_MAIL].size()) * GetCharacterHeight(FS_NORMAL);
		aimed_height += 4 * GetCharacterHeight(FS_NORMAL);  // CM (extra town info)

		bool first = true;
		for (int i = TAE_BEGIN; i < TAE_END; i++) {
			if (this->town->goal[i] == 0) continue;
			if (this->town->goal[i] == TOWN_GROWTH_WINTER && (TileHeight(this->town->xy) < LowestSnowLine() || this->town->cache.population <= 90)) continue;
			if (this->town->goal[i] == TOWN_GROWTH_DESERT && (GetTropicZone(this->town->xy) != TROPICZONE_DESERT || this->town->cache.population <= 60)) continue;

			if (first) {
				aimed_height += GetCharacterHeight(FS_NORMAL);
				first = false;
			}
			aimed_height += GetCharacterHeight(FS_NORMAL);
		}
		aimed_height += GetCharacterHeight(FS_NORMAL);

		if (_settings_game.economy.station_noise_level) aimed_height += GetCharacterHeight(FS_NORMAL);

		if (!this->town->text.empty()) {
			SetDParamStr(0, this->town->text);
			aimed_height += GetStringHeight(STR_JUST_RAW_STRING, width - WidgetDimensions::scaled.framerect.Horizontal());
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

	void OnMouseWheel(int wheel) override
	{
		if (_settings_client.gui.scrollwheel_scrolling != SWS_OFF) {
			DoZoomInOutWindow(wheel < 0 ? ZOOM_IN : ZOOM_OUT, this);
		}
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;
		/* Called when setting station noise or required cargoes have changed, in order to resize the window */
		this->SetDirty(); // refresh display for current size. This will allow to avoid glitches when downgrading
		this->ResizeWindowAsNeeded();
	}

	void OnQueryTextFinished(std::optional<std::string> str) override
	{
		if (!str.has_value()) return;

		Command<CMD_RENAME_TOWN>::Post(STR_ERROR_CAN_T_RENAME_TOWN, this->window_number, *str);
	}

	EventState OnHotkey(int hotkey) override
	{
		if(hotkey == WID_TV_CB) ShowCBTownWindow(this->window_number);
		else if (hotkey == HK_STATUE + 0x80){
			TownExecuteAction(this->town, HK_STATUE);
			return ES_NOT_HANDLED;
		}
		return Window::OnHotkey(hotkey);
	}

	IntervalTimer<TimerGameCalendar> daily_interval = {{TimerGameCalendar::DAY, TimerGameCalendar::Priority::NONE}, [this](auto) {
		/* Refresh after possible snowline change */
		this->SetDirty();
	}};

	static inline HotkeyList hotkeys{"cm_town_view", {
		Hotkey((uint16)0, "location", WID_TV_CENTER_VIEW),
		Hotkey((uint16)0, "local_authority", WID_TV_SHOW_AUTHORITY),
		Hotkey((uint16)0, "cb_window", WID_TV_CB),
		Hotkey(WKC_CTRL | 'S', "build_statue", HK_STATUE + 0x80),
	}};
};

static constexpr NWidgetPart _nested_town_game_view_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_PUSHIMGBTN, COLOUR_BROWN, WID_TV_CHANGE_NAME), SetAspect(WidgetDimensions::ASPECT_RENAME), SetDataTip(SPR_RENAME, STR_TOWN_VIEW_RENAME_TOOLTIP),
		NWidget(WWT_CAPTION, COLOUR_BROWN, WID_TV_CAPTION), SetDataTip(STR_TOWN_VIEW_TOWN_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHIMGBTN, COLOUR_BROWN, WID_TV_CENTER_VIEW), SetAspect(WidgetDimensions::ASPECT_LOCATION), SetDataTip(SPR_GOTO_LOCATION, STR_TOWN_VIEW_CENTER_TOOLTIP),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN),
		NWidget(WWT_INSET, COLOUR_BROWN), SetPadding(2, 2, 2, 2),
			NWidget(NWID_VIEWPORT, INVALID_COLOUR, WID_TV_VIEWPORT), SetMinimalSize(254, 86), SetFill(1, 0), SetResize(1, 1),
		EndContainer(),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, WID_TV_INFO), SetMinimalSize(260, 32), SetResize(1, 0), SetFill(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_TV_SHOW_AUTHORITY), SetMinimalSize(80, 12), SetFill(1, 1), SetResize(1, 0), SetDataTip(STR_TOWN_VIEW_LOCAL_AUTHORITY_BUTTON, STR_TOWN_VIEW_LOCAL_AUTHORITY_TOOLTIP),
		NWidget(WWT_TEXTBTN, COLOUR_BROWN, WID_TV_CATCHMENT), SetMinimalSize(40, 12), SetFill(1, 1), SetResize(1, 0), SetDataTip(STR_BUTTON_CATCHMENT, STR_TOOLTIP_CATCHMENT),
		NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_TV_CB), SetMinimalSize(20, 12), SetFill(1, 1), SetResize(1, 0), SetDataTip(CM_STR_BUTTON_CB, 0),
		NWidget(WWT_RESIZEBOX, COLOUR_BROWN),
	EndContainer(),
};

static WindowDesc _town_game_view_desc(
	WDP_AUTO, "view_town", 260, TownViewWindow::WID_TV_HEIGHT_NORMAL,
	WC_TOWN_VIEW, WC_NONE,
	0,
	_nested_town_game_view_widgets,
	&TownViewWindow::hotkeys
);

static constexpr NWidgetPart _nested_town_editor_view_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_PUSHIMGBTN, COLOUR_BROWN, WID_TV_CHANGE_NAME), SetAspect(WidgetDimensions::ASPECT_RENAME), SetDataTip(SPR_RENAME, STR_TOWN_VIEW_RENAME_TOOLTIP),
		NWidget(WWT_CAPTION, COLOUR_BROWN, WID_TV_CAPTION), SetDataTip(STR_TOWN_VIEW_TOWN_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHIMGBTN, COLOUR_BROWN, WID_TV_CENTER_VIEW), SetAspect(WidgetDimensions::ASPECT_LOCATION), SetDataTip(SPR_GOTO_LOCATION, STR_TOWN_VIEW_CENTER_TOOLTIP),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN),
		NWidget(WWT_INSET, COLOUR_BROWN), SetPadding(2, 2, 2, 2),
			NWidget(NWID_VIEWPORT, INVALID_COLOUR, WID_TV_VIEWPORT), SetMinimalSize(254, 86), SetFill(1, 1), SetResize(1, 1),
		EndContainer(),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, WID_TV_INFO), SetMinimalSize(260, 32), SetResize(1, 0), SetFill(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_TV_EXPAND), SetMinimalSize(80, 12), SetFill(1, 1), SetResize(1, 0), SetDataTip(STR_TOWN_VIEW_EXPAND_BUTTON, STR_TOWN_VIEW_EXPAND_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_TV_DELETE), SetMinimalSize(80, 12), SetFill(1, 1), SetResize(1, 0), SetDataTip(STR_TOWN_VIEW_DELETE_BUTTON, STR_TOWN_VIEW_DELETE_TOOLTIP),
		NWidget(WWT_TEXTBTN, COLOUR_BROWN, WID_TV_CATCHMENT), SetMinimalSize(40, 12), SetFill(1, 1), SetResize(1, 0), SetDataTip(STR_BUTTON_CATCHMENT, STR_TOOLTIP_CATCHMENT),
		NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_TV_CB), SetMinimalSize(20, 12), SetFill(1, 1), SetResize(1, 0), SetDataTip(CM_STR_BUTTON_CB, 0),
		NWidget(WWT_RESIZEBOX, COLOUR_BROWN),
	EndContainer(),
};

static WindowDesc _town_editor_view_desc(
	WDP_AUTO, "view_town_scen", 260, TownViewWindow::WID_TV_HEIGHT_NORMAL,
	WC_TOWN_VIEW, WC_NONE,
	0,
	_nested_town_editor_view_widgets
);

void ShowTownViewWindow(TownID town)
{
	if (_game_mode == GM_EDITOR) {
		AllocateWindowDescFront<TownViewWindow>(_town_editor_view_desc, town);
	} else {
		AllocateWindowDescFront<TownViewWindow>(_town_game_view_desc, town);
	}
}

static constexpr NWidgetPart _nested_town_directory_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN, WID_TD_CAPTION), SetDataTip(CM_STR_TOWN_DIRECTORY_CAPTION_EXTRA, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_TEXTBTN, COLOUR_BROWN, WID_TD_SORT_ORDER), SetDataTip(STR_BUTTON_SORT_BY, STR_TOOLTIP_SORT_ORDER),
				NWidget(WWT_DROPDOWN, COLOUR_BROWN, WID_TD_SORT_CRITERIA), SetDataTip(STR_JUST_STRING, STR_TOOLTIP_SORT_CRITERIA),
				NWidget(WWT_EDITBOX, COLOUR_BROWN, WID_TD_FILTER), SetFill(1, 0), SetResize(1, 0), SetDataTip(STR_LIST_FILTER_OSKTITLE, STR_LIST_FILTER_TOOLTIP),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_BROWN, WID_TD_LIST), SetDataTip(0x0, STR_TOWN_DIRECTORY_LIST_TOOLTIP),
							SetFill(1, 0), SetResize(1, 1), SetScrollbar(WID_TD_SCROLLBAR), EndContainer(),
			NWidget(WWT_PANEL, COLOUR_BROWN),
				NWidget(WWT_TEXT, COLOUR_BROWN, WID_TD_WORLD_POPULATION), SetPadding(2, 0, 2, 2), SetFill(1, 0), SetResize(1, 0), SetDataTip(STR_TOWN_POPULATION, STR_NULL),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_VSCROLLBAR, COLOUR_BROWN, WID_TD_SCROLLBAR),
			NWidget(WWT_RESIZEBOX, COLOUR_BROWN),
		EndContainer(),
	EndContainer(),
};

/** Enum referring to the Hotkeys in the town directory window */
enum TownDirectoryHotkeys {
	TDHK_FOCUS_FILTER_BOX, ///< Focus the filter box
};

/** Town directory window class. */
struct TownDirectoryWindow : public Window {
private:
	/* Runtime saved values */
	static Listing last_sorting;

	/* Constants for sorting towns */
	static inline const StringID sorter_names[] = {
		STR_SORT_BY_NAME,
		STR_SORT_BY_POPULATION,
		STR_SORT_BY_RATING,
	};
	static const std::initializer_list<GUITownList::SortFunction * const> sorter_funcs;

	StringFilter string_filter;             ///< Filter for towns
	QueryString townname_editbox;           ///< Filter editbox

	GUITownList towns{TownDirectoryWindow::last_sorting.order};

	Scrollbar *vscroll;

	void BuildSortTownList()
	{
		if (this->towns.NeedRebuild()) {
			this->towns.clear();
			this->towns.reserve(Town::GetNumItems());

			for (const Town *t : Town::Iterate()) {
				if (this->string_filter.IsEmpty()) {
					this->towns.push_back(t);
					continue;
				}
				this->string_filter.ResetState();
				this->string_filter.AddLine(t->GetCachedName());
				if (this->string_filter.GetState()) this->towns.push_back(t);
			}

			this->towns.RebuildDone();
			this->vscroll->SetCount(this->towns.size()); // Update scrollbar as well.
		}
		/* Always sort the towns. */
		this->towns.Sort();
		this->SetWidgetDirty(WID_TD_LIST); // Force repaint of the displayed towns.
	}

	/** Sort by town name */
	static bool TownNameSorter(const Town * const &a, const Town * const &b, const bool &)
	{
		return StrNaturalCompare(a->GetCachedName(), b->GetCachedName()) < 0; // Sort by name (natural sorting).
	}

	/** Sort by population (default descending, as big towns are of the most interest). */
	static bool TownPopulationSorter(const Town * const &a, const Town * const &b, const bool &order)
	{
		uint32_t a_population = a->cache.population;
		uint32_t b_population = b->cache.population;
		if (a_population == b_population) return TownDirectoryWindow::TownNameSorter(a, b, order);
		return a_population < b_population;
	}

	/** Sort by real population (default descending, as big towns are of the most interest). */
	static bool TownRealPopulationSorter(const Town * const &a, const Town * const &b, const bool &order)
	{
		uint32 a_population = a->cm.real_population;
		uint32 b_population = b->cm.real_population;
		if (a_population == b_population) return TownDirectoryWindow::TownNameSorter(a, b, order);
		return a_population < b_population;
	}

	/** Sort by number of houses (default descending, as big towns are of the most interest). */
	static bool TownHousesSorter(const Town * const &a, const Town * const &b, const bool &order)
	{
		uint32 a_houses = a->cache.num_houses;
		uint32 b_houses = b->cache.num_houses;
		if (a_houses == b_houses) return TownDirectoryWindow::TownPopulationSorter(a, b, order);
		return a_houses < b_houses;
	}

	/** Sort by town rating */
	static bool TownRatingSorter(const Town * const &a, const Town * const &b, const bool &order)
	{
		bool before = !order; // Value to get 'a' before 'b'.

		/* Towns without rating are always after towns with rating. */
		if (HasBit(a->have_ratings, _local_company)) {
			if (HasBit(b->have_ratings, _local_company)) {
				int16_t a_rating = a->ratings[_local_company];
				int16_t b_rating = b->ratings[_local_company];
				if (a_rating == b_rating) return TownDirectoryWindow::TownNameSorter(a, b, order);
				return a_rating < b_rating;
			}
			return before;
		}
		if (HasBit(b->have_ratings, _local_company)) return !before;

		/* Sort unrated towns always on ascending town name. */
		if (before) return TownDirectoryWindow::TownNameSorter(a, b, order);
		return TownDirectoryWindow::TownNameSorter(b, a, order);
	}

public:
	TownDirectoryWindow(WindowDesc &desc) : Window(desc), townname_editbox(MAX_LENGTH_TOWN_NAME_CHARS * MAX_CHAR_LENGTH, MAX_LENGTH_TOWN_NAME_CHARS)
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

	void SetStringParameters(WidgetID widget) const override
	{
		switch (widget) {
			case WID_TD_CAPTION:
				SetDParam(0, this->vscroll->GetCount());
				SetDParam(1, Town::GetNumItems());
				break;

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

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_TD_SORT_ORDER:
				this->DrawSortButtonState(widget, this->towns.IsDescSortOrder() ? SBS_DOWN : SBS_UP);
				break;

			case WID_TD_LIST: {
				Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);
				if (this->towns.empty()) { // No towns available.
					DrawString(tr, STR_TOWN_DIRECTORY_NONE);
					break;
				}

				/* At least one town available. */
				bool rtl = _current_text_dir == TD_RTL;
				Dimension icon_size = GetSpriteSize(SPR_TOWN_RATING_GOOD);
				int icon_x = tr.WithWidth(icon_size.width, rtl).left;
				tr = tr.Indent(icon_size.width + WidgetDimensions::scaled.hsep_normal, rtl);

				auto [first, last] = this->vscroll->GetVisibleRangeIterators(this->towns);
				for (auto it = first; it != last; ++it) {
					const Town *t = *it;
					assert(t->xy != INVALID_TILE);

					/* Draw rating icon. */
					if (_game_mode == GM_EDITOR || !HasBit(t->have_ratings, _local_company)) {
						DrawSprite(SPR_TOWN_RATING_NA, PAL_NONE, icon_x, tr.top + (this->resize.step_height - icon_size.height) / 2);
					} else {
						SpriteID icon = SPR_TOWN_RATING_APALLING;
						if (t->ratings[_local_company] > RATING_VERYPOOR) icon = SPR_TOWN_RATING_MEDIOCRE;
						if (t->ratings[_local_company] > RATING_GOOD)     icon = SPR_TOWN_RATING_GOOD;
						DrawSprite(icon, PAL_NONE, icon_x, tr.top + (this->resize.step_height - icon_size.height) / 2);
					}

					SetDParam(0, t->index);
					SetDParam(1, t->cache.population);
					SetDParam(2, t->cm.real_population);
					SetDParam(3, t->cache.num_houses);
					/* CITIES DIFFERENT COLOUR*/
					DrawString(tr.left, tr.right, tr.top + (this->resize.step_height - GetCharacterHeight(FS_NORMAL)) / 2, t->larger_town ? CM_STR_TOWN_DIRECTORY_CITY_COLOUR : CM_STR_TOWN_DIRECTORY_TOWN_COLOUR);


					tr.top += this->resize.step_height;
				}
				break;
			}
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_TD_SORT_ORDER: {
				Dimension d = GetStringBoundingBox(this->GetWidget<NWidgetCore>(widget)->widget_data);
				d.width += padding.width + Window::SortButtonWidth() * 2; // Doubled since the string is centred and it also looks better.
				d.height += padding.height;
				size = maxdim(size, d);
				break;
			}
			case WID_TD_SORT_CRITERIA: {
				Dimension d = GetStringListBoundingBox(TownDirectoryWindow::sorter_names);
				d.width += padding.width;
				d.height += padding.height;
				size = maxdim(size, d);
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
					d = maxdim(d, GetStringBoundingBox(CM_STR_TOWN_DIRECTORY_TOWN_COLOUR));
				}
				Dimension icon_size = GetSpriteSize(SPR_TOWN_RATING_GOOD);
				d.width += icon_size.width + 2;
				d.height = std::max(d.height, icon_size.height);
				resize.height = d.height;
				d.height *= 5;
				d.width += padding.width;
				d.height += padding.height;
				size = maxdim(size, d);
				break;
			}
			case WID_TD_WORLD_POPULATION: {
				SetDParamMaxDigits(0, 10);
				Dimension d = GetStringBoundingBox(STR_TOWN_POPULATION);
				d.width += padding.width;
				d.height += padding.height;
				size = maxdim(size, d);
				break;
			}
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
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
				auto it = this->vscroll->GetScrolledItemFromWidget(this->towns, pt.y, this, WID_TD_LIST, WidgetDimensions::scaled.framerect.top);
				if (it == this->towns.end()) return; // click out of town bounds

				const Town *t = *it;
				assert(t != nullptr);
				if (citymania::_fn_mod) {
					ShowExtraViewportWindow(t->xy);
				} else {
					ScrollMainWindowToTile(t->xy);
				}
				break;
			}
		}
	}

	void OnDropdownSelect(WidgetID widget, int index) override
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

	/** Redraw the whole window on a regular interval. */
	IntervalTimer<TimerWindow> rebuild_interval = {std::chrono::seconds(3), [this](auto) {
		this->BuildSortTownList();
		this->SetDirty();
	}};

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_TD_LIST, WidgetDimensions::scaled.framerect.Vertical());
	}

	void OnEditboxChanged(WidgetID wid) override
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
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
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

	EventState OnHotkey(int hotkey) override
	{
		switch (hotkey) {
			case TDHK_FOCUS_FILTER_BOX:
				this->SetFocusedWidget(WID_TD_FILTER);
				SetFocusedWindow(this); // The user has asked to give focus to the text box, so make sure this window is focused.
				break;
			default:
				return ES_NOT_HANDLED;
		}
		return ES_HANDLED;
	}

	static inline HotkeyList hotkeys {"towndirectory", {
		Hotkey('F', "focus_filter_box", TDHK_FOCUS_FILTER_BOX),
	}};
};

Listing TownDirectoryWindow::last_sorting = {false, 0};

/** Available town directory sorting functions. */
const std::initializer_list<GUITownList::SortFunction * const> TownDirectoryWindow::sorter_funcs = {
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
	_nested_town_directory_widgets,
	&TownDirectoryWindow::hotkeys
);

void ShowTownDirectory()
{
	if (BringWindowToFrontById(WC_TOWN_DIRECTORY, 0)) return;
	new TownDirectoryWindow(_town_directory_desc);
}

void CcFoundTown(Commands, const CommandCost &result, TileIndex tile)
{
	if (result.Failed()) return;

	if (_settings_client.sound.confirm) SndPlayTileFx(SND_1F_CONSTRUCTION_OTHER, tile);
	if (!_settings_client.gui.persistent_buildingtools) ResetObjectToPlace();
}

void CcFoundRandomTown(Commands, const CommandCost &result, Money, TownID town_id)
{
	if (result.Succeeded()) ScrollMainWindowToTile(Town::Get(town_id)->xy);
}

static constexpr NWidgetPart _nested_found_town_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetDataTip(STR_FOUND_TOWN_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	/* Construct new town(s) buttons. */
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN),
		NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0), SetPadding(WidgetDimensions::unscaled.picker),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_NEW_TOWN), SetDataTip(STR_FOUND_TOWN_NEW_TOWN_BUTTON, STR_FOUND_TOWN_NEW_TOWN_TOOLTIP), SetFill(1, 0),
			NWidget(NWID_SELECTION, INVALID_COLOUR, WID_TF_TOWN_ACTION_SEL),
				NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_TF_RANDOM_TOWN), SetDataTip(STR_FOUND_TOWN_RANDOM_TOWN_BUTTON, STR_FOUND_TOWN_RANDOM_TOWN_TOOLTIP), SetFill(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_TF_MANY_RANDOM_TOWNS), SetDataTip(STR_FOUND_TOWN_MANY_RANDOM_TOWNS, STR_FOUND_TOWN_RANDOM_TOWNS_TOOLTIP), SetFill(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_TF_LOAD_FROM_FILE), SetDataTip(STR_FOUND_TOWN_LOAD_FROM_FILE, STR_FOUND_TOWN_LOAD_FROM_FILE_TOOLTIP), SetFill(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_TF_EXPAND_ALL_TOWNS), SetDataTip(STR_FOUND_TOWN_EXPAND_ALL_TOWNS, STR_FOUND_TOWN_EXPAND_ALL_TOWNS_TOOLTIP), SetFill(1, 0),
				EndContainer(),
			EndContainer(),

			/* Town name selection. */
			NWidget(WWT_LABEL, COLOUR_DARK_GREEN), SetDataTip(STR_FOUND_TOWN_NAME_TITLE, STR_NULL),
			NWidget(WWT_EDITBOX, COLOUR_GREY, WID_TF_TOWN_NAME_EDITBOX), SetDataTip(STR_FOUND_TOWN_NAME_EDITOR_TITLE, STR_FOUND_TOWN_NAME_EDITOR_HELP), SetFill(1, 0),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_TF_TOWN_NAME_RANDOM), SetDataTip(STR_FOUND_TOWN_NAME_RANDOM_BUTTON, STR_FOUND_TOWN_NAME_RANDOM_TOOLTIP), SetFill(1, 0),

			/* Town size selection. */
			NWidget(WWT_LABEL, COLOUR_DARK_GREEN), SetDataTip(STR_FOUND_TOWN_INITIAL_SIZE_TITLE, STR_NULL),
			NWidget(NWID_VERTICAL),
				NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
					NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_SIZE_SMALL), SetDataTip(STR_FOUND_TOWN_INITIAL_SIZE_SMALL_BUTTON, STR_FOUND_TOWN_INITIAL_SIZE_TOOLTIP), SetFill(1, 0),
					NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_SIZE_MEDIUM), SetDataTip(STR_FOUND_TOWN_INITIAL_SIZE_MEDIUM_BUTTON, STR_FOUND_TOWN_INITIAL_SIZE_TOOLTIP), SetFill(1, 0),
				EndContainer(),
				NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
					NWidget(NWID_SELECTION, INVALID_COLOUR, WID_TF_SIZE_SEL),
						NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_SIZE_LARGE), SetDataTip(STR_FOUND_TOWN_INITIAL_SIZE_LARGE_BUTTON, STR_FOUND_TOWN_INITIAL_SIZE_TOOLTIP), SetFill(1, 0),
					EndContainer(),
					NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_SIZE_RANDOM), SetDataTip(STR_FOUND_TOWN_SIZE_RANDOM, STR_FOUND_TOWN_INITIAL_SIZE_TOOLTIP), SetFill(1, 0),
				EndContainer(),
			EndContainer(),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_CITY), SetDataTip(STR_FOUND_TOWN_CITY, STR_FOUND_TOWN_CITY_TOOLTIP), SetFill(1, 0),

			/* Town roads selection. */
			NWidget(NWID_SELECTION, INVALID_COLOUR, WID_TF_ROAD_LAYOUT_SEL),
				NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0),
					NWidget(WWT_LABEL, COLOUR_DARK_GREEN), SetDataTip(STR_FOUND_TOWN_ROAD_LAYOUT, STR_NULL),
					NWidget(NWID_VERTICAL),
						NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_LAYOUT_ORIGINAL), SetDataTip(STR_FOUND_TOWN_SELECT_LAYOUT_ORIGINAL, STR_FOUND_TOWN_SELECT_TOWN_ROAD_LAYOUT), SetFill(1, 0),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_LAYOUT_BETTER), SetDataTip(STR_FOUND_TOWN_SELECT_LAYOUT_BETTER_ROADS, STR_FOUND_TOWN_SELECT_TOWN_ROAD_LAYOUT), SetFill(1, 0),
						EndContainer(),
						NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_LAYOUT_GRID2), SetDataTip(STR_FOUND_TOWN_SELECT_LAYOUT_2X2_GRID, STR_FOUND_TOWN_SELECT_TOWN_ROAD_LAYOUT), SetFill(1, 0),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_LAYOUT_GRID3), SetDataTip(STR_FOUND_TOWN_SELECT_LAYOUT_3X3_GRID, STR_FOUND_TOWN_SELECT_TOWN_ROAD_LAYOUT), SetFill(1, 0),
						EndContainer(),
						NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_LAYOUT_RANDOM), SetDataTip(STR_FOUND_TOWN_SELECT_LAYOUT_RANDOM, STR_FOUND_TOWN_SELECT_TOWN_ROAD_LAYOUT), SetFill(1, 0),
					EndContainer(),
				EndContainer(),
			EndContainer(),
		EndContainer(),
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
	uint32_t townnameparts;   ///< Generated town name
	TownNameParams params;  ///< Town name parameters

public:
	FoundTownWindow(WindowDesc &desc, WindowNumber window_number) :
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

	void OnInit() override
	{
		if (_game_mode == GM_EDITOR) return;

		this->GetWidget<NWidgetStacked>(WID_TF_TOWN_ACTION_SEL)->SetDisplayedPlane(SZSP_HORIZONTAL);
		this->GetWidget<NWidgetStacked>(WID_TF_SIZE_SEL)->SetDisplayedPlane(SZSP_VERTICAL);
		if (_settings_game.economy.found_town != TF_CUSTOM_LAYOUT) {
			this->GetWidget<NWidgetStacked>(WID_TF_ROAD_LAYOUT_SEL)->SetDisplayedPlane(SZSP_HORIZONTAL);
		} else {
			this->GetWidget<NWidgetStacked>(WID_TF_ROAD_LAYOUT_SEL)->SetDisplayedPlane(0);
		}
	}

	void RandomTownName()
	{
		this->townnamevalid = GenerateTownName(_interactive_random, &this->townnameparts);

		if (!this->townnamevalid) {
			this->townname_editbox.text.DeleteAll();
		} else {
			this->townname_editbox.text.Assign(GetTownName(&this->params, this->townnameparts));
		}
		UpdateOSKOriginalText(this, WID_TF_TOWN_NAME_EDITBOX);

		this->SetWidgetDirty(WID_TF_TOWN_NAME_EDITBOX);
	}

	void UpdateButtons(bool check_availability)
	{
		if (check_availability && _game_mode != GM_EDITOR) {
			if (_settings_game.economy.found_town != TF_CUSTOM_LAYOUT) this->town_layout = _settings_game.economy.town_layout;
			this->ReInit();
		}

		for (WidgetID i = WID_TF_SIZE_SMALL; i <= WID_TF_SIZE_RANDOM; i++) {
			this->SetWidgetLoweredState(i, i == WID_TF_SIZE_SMALL + this->town_size);
		}

		this->SetWidgetLoweredState(WID_TF_CITY, this->city);

		for (WidgetID i = WID_TF_LAYOUT_ORIGINAL; i <= WID_TF_LAYOUT_RANDOM; i++) {
			this->SetWidgetLoweredState(i, i == WID_TF_LAYOUT_ORIGINAL + this->town_layout);
		}

		this->SetDirty();
	}

	template <typename Tcallback>
	void ExecuteFoundTownCommand(TileIndex tile, bool random, StringID errstr, Tcallback cc)
	{
		std::string name;

		if (!this->townnamevalid) {
			name = this->townname_editbox.text.buf;
		} else {
			/* If user changed the name, send it */
			std::string original_name = GetTownName(&this->params, this->townnameparts);
			if (original_name != this->townname_editbox.text.buf) name = this->townname_editbox.text.buf;
		}

		bool success = Command<CMD_FOUND_TOWN>::Post(errstr, cc,
				tile, this->town_size, this->city, this->town_layout, random, townnameparts, name);

		/* Rerandomise name, if success and no cost-estimation. */
		if (success && !citymania::_estimate_mod) this->RandomTownName();
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_TF_NEW_TOWN:
				HandlePlacePushButton(this, WID_TF_NEW_TOWN, SPR_CURSOR_TOWN, HT_RECT, CM_DDSP_NEW_TOWN);
				break;

			case WID_TF_RANDOM_TOWN:
				this->ExecuteFoundTownCommand(0, true, STR_ERROR_CAN_T_GENERATE_TOWN, CcFoundRandomTown);
				break;

			case WID_TF_TOWN_NAME_RANDOM:
				this->RandomTownName();
				this->SetFocusedWidget(WID_TF_TOWN_NAME_EDITBOX);
				break;

			case WID_TF_MANY_RANDOM_TOWNS: {
				Backup<bool> old_generating_world(_generating_world, true);
				UpdateNearestTownForRoadTiles(true);
				if (!GenerateTowns(this->town_layout)) {
					ShowErrorMessage(STR_ERROR_CAN_T_GENERATE_TOWN, STR_ERROR_NO_SPACE_FOR_TOWN, WL_INFO);
				}
				UpdateNearestTownForRoadTiles(false);
				old_generating_world.Restore();
				break;
			}

			case WID_TF_LOAD_FROM_FILE:
				ShowSaveLoadDialog(FT_TOWN_DATA, SLO_LOAD);
				break;

			case WID_TF_EXPAND_ALL_TOWNS:
				for (Town *t : Town::Iterate()) {
					Command<CMD_EXPAND_TOWN>::Do(DC_EXEC, t->index, 0);
				}
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

				/* If we are in the editor, sync the settings of the current game to the chosen layout,
				 * so that importing towns from file uses the selected layout. */
				if (_game_mode == GM_EDITOR) _settings_game.economy.town_layout = this->town_layout;

				this->UpdateButtons(false);
				break;
		}
	}

	void OnPlaceObject([[maybe_unused]] Point pt, TileIndex tile) override
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
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;
		this->UpdateButtons(true);
	}
};

static WindowDesc _found_town_desc(
	WDP_AUTO, "build_town", 160, 162,
	WC_FOUND_TOWN, WC_NONE,
	WDF_CONSTRUCTION,
	_nested_found_town_widgets
);

void ShowFoundTownWindow()
{
	if (_game_mode != GM_EDITOR && !Company::IsValidID(_local_company)) return;
	AllocateWindowDescFront<FoundTownWindow>(_found_town_desc, 0);
}

void InitializeTownGui()
{
	_town_local_authority_kdtree.Clear();
}

/**
 * Draw representation of a house tile for GUI purposes.
 * @param x Position x of image.
 * @param y Position y of image.
 * @param spec House spec to draw.
 * @param house_id House ID to draw.
 * @param view The house's 'view'.
 */
void DrawNewHouseTileInGUI(int x, int y, const HouseSpec *spec, HouseID house_id, int view)
{
	HouseResolverObject object(house_id, INVALID_TILE, nullptr, CBID_NO_CALLBACK, 0, 0, true, view);
	const SpriteGroup *group = object.Resolve();
	if (group == nullptr || group->type != SGT_TILELAYOUT) return;

	uint8_t stage = TOWN_HOUSE_COMPLETED;
	const DrawTileSprites *dts = reinterpret_cast<const TileLayoutSpriteGroup *>(group)->ProcessRegisters(&stage);

	PaletteID palette = GENERAL_SPRITE_COLOUR(spec->random_colour[0]);
	if (HasBit(spec->callback_mask, CBM_HOUSE_COLOUR)) {
		uint16_t callback = GetHouseCallback(CBID_HOUSE_COLOUR, 0, 0, house_id, nullptr, INVALID_TILE, true, view);
		if (callback != CALLBACK_FAILED) {
			/* If bit 14 is set, we should use a 2cc colour map, else use the callback value. */
			palette = HasBit(callback, 14) ? GB(callback, 0, 8) + SPR_2CCMAP_BASE : callback;
		}
	}

	SpriteID image = dts->ground.sprite;
	PaletteID pal  = dts->ground.pal;

	if (HasBit(image, SPRITE_MODIFIER_CUSTOM_SPRITE)) image += stage;
	if (HasBit(pal, SPRITE_MODIFIER_CUSTOM_SPRITE)) pal += stage;

	if (GB(image, 0, SPRITE_WIDTH) != 0) {
		DrawSprite(image, GroundSpritePaletteTransform(image, pal, palette), x, y);
	}

	DrawNewGRFTileSeqInGUI(x, y, dts, stage, palette);
}

/**
 * Draw a house that does not exist.
 * @param x Position x of image.
 * @param y Position y of image.
 * @param house_id House ID to draw.
 * @param view The house's 'view'.
 */
void DrawHouseInGUI(int x, int y, HouseID house_id, int view)
{
	auto draw = [](int x, int y, HouseID house_id, int view) {
		if (house_id >= NEW_HOUSE_OFFSET) {
			/* Houses don't necessarily need new graphics. If they don't have a
			 * spritegroup associated with them, then the sprite for the substitute
			 * house id is drawn instead. */
			const HouseSpec *spec = HouseSpec::Get(house_id);
			if (spec->grf_prop.spritegroup[0] != nullptr) {
				DrawNewHouseTileInGUI(x, y, spec, house_id, view);
				return;
			} else {
				house_id = HouseSpec::Get(house_id)->grf_prop.subst_id;
			}
		}

		/* Retrieve data from the draw town tile struct */
		const DrawBuildingsTileStruct &dcts = GetTownDrawTileData()[house_id << 4 | view << 2 | TOWN_HOUSE_COMPLETED];
		DrawSprite(dcts.ground.sprite, dcts.ground.pal, x, y);

		/* Add a house on top of the ground? */
		if (dcts.building.sprite != 0) {
			Point pt = RemapCoords(dcts.subtile_x, dcts.subtile_y, 0);
			DrawSprite(dcts.building.sprite, dcts.building.pal, x + UnScaleGUI(pt.x), y + UnScaleGUI(pt.y));
		}
	};

	/* Houses can have 1x1, 1x2, 2x1 and 2x2 layouts which are individual HouseIDs. For the GUI we need
	 * draw all of the tiles with appropriate positions. */
	int x_delta = ScaleGUITrad(TILE_PIXELS);
	int y_delta = ScaleGUITrad(TILE_PIXELS / 2);

	const HouseSpec *hs = HouseSpec::Get(house_id);
	if (hs->building_flags & TILE_SIZE_2x2) {
		draw(x, y - y_delta - y_delta, house_id, view); // North corner.
		draw(x + x_delta, y - y_delta, house_id + 1, view); // West corner.
		draw(x - x_delta, y - y_delta, house_id + 2, view); // East corner.
		draw(x, y, house_id + 3, view); // South corner.
	} else if (hs->building_flags & TILE_SIZE_2x1) {
		draw(x + x_delta / 2, y - y_delta, house_id, view); // North east tile.
		draw(x - x_delta / 2, y, house_id + 1, view); // South west tile.
	} else if (hs->building_flags & TILE_SIZE_1x2) {
		draw(x - x_delta / 2, y - y_delta, house_id, view); // North west tile.
		draw(x + x_delta / 2, y, house_id + 1, view); // South east tile.
	} else {
		draw(x, y, house_id, view);
	}
}

/**
 * Get name for a prototype house.
 * @param hs HouseSpec of house.
 * @return StringID of name for house.
 */
static StringID GetHouseName(const HouseSpec *hs)
{
	uint16_t callback_res = GetHouseCallback(CBID_HOUSE_CUSTOM_NAME, 1, 0, hs->Index(), nullptr, INVALID_TILE, true);
	if (callback_res != CALLBACK_FAILED && callback_res != 0x400) {
		if (callback_res > 0x400) {
			ErrorUnknownCallbackResult(hs->grf_prop.grffile->grfid, CBID_HOUSE_CUSTOM_NAME, callback_res);
		} else {
			StringID new_name = GetGRFStringID(hs->grf_prop.grffile->grfid, 0xD000 + callback_res);
			if (new_name != STR_NULL && new_name != STR_UNDEFINED) {
				return new_name;
			}
		}
	}

	return hs->building_name;
}

class HousePickerCallbacks : public PickerCallbacks {
public:
	HousePickerCallbacks() : PickerCallbacks("fav_houses") {}

	/**
	 * Set climate mask for filtering buildings from current landscape.
	 */
	void SetClimateMask()
	{
		switch (_settings_game.game_creation.landscape) {
			case LT_TEMPERATE: this->climate_mask = HZ_TEMP; break;
			case LT_ARCTIC:    this->climate_mask = HZ_SUBARTC_ABOVE | HZ_SUBARTC_BELOW; break;
			case LT_TROPIC:    this->climate_mask = HZ_SUBTROPIC; break;
			case LT_TOYLAND:   this->climate_mask = HZ_TOYLND; break;
			default: NOT_REACHED();
		}

		/* In some cases, not all 'classes' (house zones) have distinct houses, so we need to disable those.
		 * As we need to check all types, and this cannot change with the picker window open, pre-calculate it.
		 * This loop calls GetTypeName() instead of directly checking properties so that there is no discrepancy. */
		this->class_mask = 0;

		int num_classes = this->GetClassCount();
		for (int cls_id = 0; cls_id < num_classes; ++cls_id) {
			int num_types = this->GetTypeCount(cls_id);
			for (int id = 0; id < num_types; ++id) {
				if (this->GetTypeName(cls_id, id) != INVALID_STRING_ID) {
					SetBit(this->class_mask, cls_id);
					break;
				}
			}
		}
	}

	HouseZones climate_mask;
	uint8_t class_mask; ///< Mask of available 'classes'.

	static inline int sel_class; ///< Currently selected 'class'.
	static inline int sel_type; ///< Currently selected HouseID.
	static inline int sel_view; ///< Currently selected 'view'. This is not controllable as its based on random data.

	/* Houses do not have classes like NewGRFClass. We'll make up fake classes based on town zone
	 * availability instead. */
	static inline const std::array<StringID, HZB_END> zone_names = {
		STR_HOUSE_PICKER_CLASS_ZONE1,
		STR_HOUSE_PICKER_CLASS_ZONE2,
		STR_HOUSE_PICKER_CLASS_ZONE3,
		STR_HOUSE_PICKER_CLASS_ZONE4,
		STR_HOUSE_PICKER_CLASS_ZONE5,
	};

	StringID GetClassTooltip() const override { return STR_PICKER_HOUSE_CLASS_TOOLTIP; }
	StringID GetTypeTooltip() const override { return STR_PICKER_HOUSE_TYPE_TOOLTIP; }
	bool IsActive() const override { return true; }

	bool HasClassChoice() const override { return true; }
	int GetClassCount() const override { return static_cast<int>(zone_names.size()); }

	void Close([[maybe_unused]] int data) override { ResetObjectToPlace(); }

	int GetSelectedClass() const override { return HousePickerCallbacks::sel_class; }
	void SetSelectedClass(int cls_id) const override { HousePickerCallbacks::sel_class = cls_id; }

	StringID GetClassName(int id) const override
	{
		if (id >= GetClassCount()) return INVALID_STRING_ID;
		if (!HasBit(this->class_mask, id)) return INVALID_STRING_ID;
		return zone_names[id];
	}

	int GetTypeCount(int cls_id) const override
	{
		if (cls_id < GetClassCount()) return static_cast<int>(HouseSpec::Specs().size());
		return 0;
	}

	PickerItem GetPickerItem(int cls_id, int id) const override
	{
		const auto *spec = HouseSpec::Get(id);
		if (!spec->grf_prop.HasGrfFile()) return {0, spec->Index(), cls_id, id};
		return {spec->grf_prop.grfid, spec->grf_prop.local_id, cls_id, id};
	}

	int GetSelectedType() const override { return sel_type; }
	void SetSelectedType(int id) const override { sel_type = id; }

	StringID GetTypeName(int cls_id, int id) const override
	{
		const HouseSpec *spec = HouseSpec::Get(id);
		if (spec == nullptr) return INVALID_STRING_ID;
		if (!spec->enabled) return INVALID_STRING_ID;
		if ((spec->building_availability & climate_mask) == 0) return INVALID_STRING_ID;
		if (!HasBit(spec->building_availability, cls_id)) return INVALID_STRING_ID;
		for (int i = 0; i < cls_id; i++) {
			/* Don't include if it's already included in an earlier zone. */
			if (HasBit(spec->building_availability, i)) return INVALID_STRING_ID;
		}

		return GetHouseName(spec);
	}

	bool IsTypeAvailable(int, int id) const override
	{
		const HouseSpec *hs = HouseSpec::Get(id);
		return hs->enabled;
	}

	void DrawType(int x, int y, int, int id) const override
	{
		DrawHouseInGUI(x, y, id, HousePickerCallbacks::sel_view);
	}

	void FillUsedItems(std::set<PickerItem> &items) override
	{
		auto id_count = GetBuildingHouseIDCounts();
		for (auto it = id_count.begin(); it != id_count.end(); ++it) {
			if (*it == 0) continue;
			HouseID house = static_cast<HouseID>(std::distance(id_count.begin(), it));
			const HouseSpec *hs = HouseSpec::Get(house);
			int class_index = FindFirstBit(hs->building_availability & HZ_ZONALL);
			items.insert({0, house, class_index, house});
		}
	}

	std::set<PickerItem> UpdateSavedItems(const std::set<PickerItem> &src) override
	{
		if (src.empty()) return src;

		const auto specs = HouseSpec::Specs();
		std::set<PickerItem> dst;
		for (const auto &item : src) {
			if (item.grfid == 0) {
				dst.insert(item);
			} else {
				/* Search for spec by grfid and local index. */
				auto it = std::ranges::find_if(specs, [&item](const HouseSpec &spec) { return spec.grf_prop.grfid == item.grfid && spec.grf_prop.local_id == item.local_id; });
				if (it == specs.end()) {
					/* Not preset, hide from UI. */
					dst.insert({item.grfid, item.local_id, -1, -1});
				} else {
					int class_index = FindFirstBit(it->building_availability & HZ_ZONALL);
					dst.insert( {item.grfid, item.local_id, class_index, it->Index()});
				}
			}
		}

		return dst;
	}

	static HousePickerCallbacks instance;
};
/* static */ HousePickerCallbacks HousePickerCallbacks::instance;

struct BuildHouseWindow : public PickerWindow {
	std::string house_info;

	BuildHouseWindow(WindowDesc &desc, Window *parent) : PickerWindow(desc, parent, 0, HousePickerCallbacks::instance)
	{
		HousePickerCallbacks::instance.SetClimateMask();
		this->ConstructWindow();
		this->InvalidateData();
	}

	void CMRotate() override {}

	void UpdateSelectSize(const HouseSpec *spec)
	{
		if (spec == nullptr) {
			SetTileSelectSize(1, 1);
			ResetObjectToPlace();
		} else {
			SetObjectToPlaceWnd(SPR_CURSOR_TOWN, PAL_NONE, HT_RECT | HT_DIAGONAL, this, CM_DDSP_BUILD_HOUSE);
			if (spec->building_flags & TILE_SIZE_2x2) {
				SetTileSelectSize(2, 2);
			} else if (spec->building_flags & TILE_SIZE_2x1) {
				SetTileSelectSize(2, 1);
			} else if (spec->building_flags & TILE_SIZE_1x2) {
				SetTileSelectSize(1, 2);
			} else if (spec->building_flags & TILE_SIZE_1x1) {
				SetTileSelectSize(1, 1);
			}
		}
	}

	/**
	 * Get a date range string for house availability year.
	 * @param min_year Earliest year house can be built.
	 * @param max_year Latest year house can be built.
	 * @return Formatted string with the date range formatted appropriately.
	 */
	static std::string GetHouseYear(TimerGameCalendar::Year min_year, TimerGameCalendar::Year max_year)
	{
		if (min_year == CalendarTime::MIN_YEAR) {
			if (max_year == CalendarTime::MAX_YEAR) {
				return GetString(STR_HOUSE_PICKER_YEARS_ANY);
			}
			SetDParam(0, max_year);
			return GetString(STR_HOUSE_PICKER_YEARS_UNTIL);
		}
		if (max_year == CalendarTime::MAX_YEAR) {
			SetDParam(0, min_year);
			return GetString(STR_HOUSE_PICKER_YEARS_FROM);
		}
		SetDParam(0, min_year);
		SetDParam(1, max_year);
		return GetString(STR_HOUSE_PICKER_YEARS);
	}

	/**
	 * Get information string for a house.
	 * @param hs HosueSpec to get information string for.
	 * @return Formatted string with information for house.
	 */
	static std::string GetHouseInformation(const HouseSpec *hs)
	{
		std::stringstream line;

		SetDParam(0, GetHouseName(hs));
		line << GetString(STR_HOUSE_PICKER_NAME);
		line << "\n";

		SetDParam(0, hs->population);
		line << GetString(STR_HOUSE_PICKER_POPULATION);
		line << "\n";

		line << GetHouseYear(hs->min_year, hs->max_year);
		line << "\n";

		uint8_t size = 0;
		if ((hs->building_flags & TILE_SIZE_1x1) != 0) size = 0x11;
		if ((hs->building_flags & TILE_SIZE_2x1) != 0) size = 0x21;
		if ((hs->building_flags & TILE_SIZE_1x2) != 0) size = 0x12;
		if ((hs->building_flags & TILE_SIZE_2x2) != 0) size = 0x22;
		SetDParam(0, GB(size, 0, 4));
		SetDParam(1, GB(size, 4, 4));
		line << GetString(STR_HOUSE_PICKER_SIZE);
		line << "\n";

		auto cargo_string = BuildCargoAcceptanceString(GetAcceptedCargoOfHouse(hs), STR_HOUSE_PICKER_CARGO_ACCEPTED);
		if (cargo_string.has_value()) line << *cargo_string;

		return line.str();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget == WID_BH_INFO) {
			if (!this->house_info.empty()) DrawStringMultiLine(r, this->house_info);
		} else {
			this->PickerWindow::DrawWidget(r, widget);
		}
	}

	void OnInvalidateData(int data = 0, bool gui_scope = true) override
	{
		this->PickerWindow::OnInvalidateData(data, gui_scope);
		if (!gui_scope) return;

		if ((data & PickerWindow::PFI_POSITION) != 0) {
			const HouseSpec *spec = HouseSpec::Get(HousePickerCallbacks::sel_type);
			UpdateSelectSize(spec);
			this->house_info = GetHouseInformation(spec);
		}
	}

	void OnPlaceObject([[maybe_unused]] Point pt, TileIndex tile) override
	{
		const HouseSpec *spec = HouseSpec::Get(HousePickerCallbacks::sel_type);
		Command<CMD_PLACE_HOUSE>::Post(STR_ERROR_CAN_T_BUILD_HOUSE, CcPlaySound_CONSTRUCTION_OTHER, tile, spec->Index());
	}

	IntervalTimer<TimerWindow> view_refresh_interval = {std::chrono::milliseconds(2500), [this](auto) {
		/* There are four different 'views' that are random based on house tile position. As this is not
		 * user-controllable, instead we automatically cycle through them. */
		HousePickerCallbacks::sel_view = (HousePickerCallbacks::sel_view + 1) % 4;
		this->SetDirty();
	}};

	static inline HotkeyList hotkeys{"buildhouse", {
		Hotkey('F', "focus_filter_box", PCWHK_FOCUS_FILTER_BOX),
	}};
};

/** Nested widget definition for the build NewGRF rail waypoint window */
static constexpr NWidgetPart _nested_build_house_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetDataTip(STR_HOUSE_PICKER_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_VERTICAL),
			NWidgetFunction(MakePickerClassWidgets),
			NWidget(WWT_PANEL, COLOUR_DARK_GREEN),
				NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_picker, 0), SetPadding(WidgetDimensions::unscaled.picker),
					NWidget(WWT_EMPTY, INVALID_COLOUR, WID_BH_INFO), SetFill(1, 1), SetMinimalTextLines(10, 0),
				EndContainer(),
			EndContainer(),
		EndContainer(),
		NWidgetFunction(MakePickerTypeWidgets),
	EndContainer(),
};

static WindowDesc _build_house_desc(
	WDP_AUTO, "build_house", 0, 0,
	WC_BUILD_HOUSE, WC_BUILD_TOOLBAR,
	WDF_CONSTRUCTION,
	_nested_build_house_widgets,
	&BuildHouseWindow::hotkeys
);

void ShowBuildHousePicker(Window *parent)
{
	if (BringWindowToFrontById(WC_BUILD_HOUSE, 0)) return;
	new BuildHouseWindow(_build_house_desc, parent);
}


//CB
static void DrawExtraTownInfo (Rect &r, Town *town, uint line, bool show_house_states_info) {
	//real pop and rating
	SetDParam(0, town->cm.real_population);
	SetDParam(1, town->ratings[_current_company]);
	DrawString(r, CM_STR_TOWN_VIEW_REALPOP_RATE);
	r.top += line;
	//town stats
	// int grow_rate = 0;
	// if(town->growth_rate == TOWN_GROW_RATE_CUSTOM_NONE) grow_rate = 0;
	// else grow_rate = TownTicksToDays((town->growth_rate & ~TOWN_GROW_RATE_CUSTOM) + 1);

	SetDParam(0, town->growth_rate);
	SetDParam(1, HasBit(town->flags, TOWN_CUSTOM_GROWTH) ? CM_STR_TOWN_VIEW_GROWTH_RATE_CUSTOM : STR_EMPTY);
	// SetDParam(2, town->grow_counter < 16000 ? TownTicksToDays(town->grow_counter + 1) : -1);
	SetDParam(2, town->grow_counter);
	SetDParam(3, town->time_until_rebuild);
	SetDParam(4, HasBit(town->flags, TOWN_IS_GROWING) ? 1 : 0);
	SetDParam(5, town->fund_buildings_months);
	DrawString(r, CM_STR_TOWN_VIEW_GROWTH);
	r.top += line;

	if (show_house_states_info) {
		SetDParam(0, town->cm.houses_constructing);
		SetDParam(1, town->cm.houses_reconstructed_last_month);
		SetDParam(2, town->cm.houses_demolished_last_month);
		DrawString(r, CM_STR_TOWN_VIEW_HOUSE_STATE);
		r.top += line;
	}

	///houses stats
	SetDParam(0, town->cm.hs_total);
	SetDParam(1, town->cm.hs_last_month);
	SetDParam(2, town->cm.cs_total);
	SetDParam(3, town->cm.cs_last_month);
	SetDParam(4, town->cm.hr_total);
	SetDParam(5, town->cm.hr_last_month);
	DrawString(r, CM_STR_TOWN_VIEW_GROWTH_TILES);
	r.top += line;
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
	CBTownWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc)
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

	void OnClick([[maybe_unused]] Point pt, int widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_CB_CENTER_VIEW:
				if (citymania::_fn_mod) {
					ShowExtraViewportWindow(this->town->xy);
				} else {
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
					ShowQueryString(STR_JUST_INT, CM_STR_CB_ADVERT_REGULAR_RATING_TO_KEEP,
					                4, this, CS_NUMERAL, QSF_ACCEPT_UNCHANGED);
				} else this->OnQueryTextFinished(std::nullopt);
				break;
			case WID_CB_TOWN_VIEW: // Town view window
				ShowTownViewWindow(this->window_number);
				break;
			case WID_CB_SHOW_AUTHORITY: // town authority
				ShowTownAuthorityWindow(this->window_number);
				break;
		}
	}

	void OnQueryTextFinished(std::optional<std::string> str) override
	{
		if (!str.has_value()) SetBit(this->town->advertise_regularly, _local_company);
		else ClrBit(this->town->advertise_regularly, _local_company);
		this->town->ad_ref_goods_entry = NULL;
		this->SetWidgetLoweredState(WID_CB_ADVERT_REGULAR, HasBit(this->town->advertise_regularly, _local_company));
		this->SetWidgetDirty(WID_CB_ADVERT_REGULAR);

		if (!str.has_value()) return;
		uint val = Clamp(str->empty()? 0 : strtol(str->c_str(), NULL, 10), 1, 100);
		this->town->ad_rating_goal = ((val << 8) + 255) / 101;
	}

	void SetStringParameters(int widget) const override
	{
		if (widget == WID_CB_CAPTION){
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

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override {
		static const uint EXP_TOPPADDING = 5;
		static const uint EXP_LINESPACE  = 2; // Amount of vertical space for a horizontal (sub-)total line.

		switch(widget){
			case WID_CB_DETAILS:
				size.height = (GetCharacterHeight(FS_NORMAL) + EXP_LINESPACE) * 5;
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
					size.height = desired_height * (GetCharacterHeight(FS_NORMAL) + EXP_LINESPACE) + EXP_TOPPADDING - EXP_LINESPACE;
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
					size.height = desired_height * (GetCharacterHeight(FS_NORMAL) + EXP_LINESPACE) + EXP_TOPPADDING - EXP_LINESPACE;
				break;
			}
		}
	}

	void DrawWidget(const Rect &r, int widget) const override
	{
		static const uint EXP_LINESPACE  = GetCharacterHeight(FS_NORMAL) + 2;
		Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);

		switch(widget){
			case WID_CB_DETAILS:{
				// growing
				if(this->town->cb.growth_state == TownGrowthState::GROWING)
					DrawString(tr, CM_STR_TOWN_CB_GROWING);
				else DrawString(tr, CM_STR_TOWN_CB_NOT_GROWING);
				tr.top += GetCharacterHeight(FS_NORMAL);

				// population
				SetDParam(0, this->town->cache.population);
				SetDParam(1, this->town->cache.num_houses);
				DrawString(tr, STR_TOWN_VIEW_POPULATION_HOUSES);
				tr.top += GetCharacterHeight(FS_NORMAL);

				DrawExtraTownInfo(tr, this->town, EXP_LINESPACE, false);

				// regular funding
				if(this->town->fund_regularly != 0){
					DrawString(tr, CM_STR_CB_FUNDED_REGULARLY);
					tr.top += EXP_LINESPACE;
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
					DrawString(tr, STR_GOALS_TEXT);
					tr.top += EXP_LINESPACE;
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
					if (it2 == cargoes2.begin()) { // header
						// FIXME rtl support
						DrawString(tr.Shrink(ScaleGUITrad(14), 0, 0, 0),
							(CM_STR_TOWN_GROWTH_HEADER_CARGO + widget - WID_CB_CARGO_NAME), TC_FROMSTRING,
							(widget == WID_CB_CARGO_NAME) ? SA_LEFT : SA_RIGHT);

						tr.top += EXP_LINESPACE;
					}

					const CargoSpec *cargos = CargoSpec::Get(cargox.id);
					//cargo needed?
					if (!cargos->IsValid() || CB_GetReq(cargos->Index()) == 0) continue;

					from = CB_GetFrom(cargos->Index());
					switch(widget) {
						case WID_CB_CARGO_NAME: {
							// FIXME scaling
							GfxFillRect(tr.left, tr.top + 1, tr.left + 8, tr.top + 6, 0);
							GfxFillRect(tr.left + 1, tr.top + 2, tr.left + 7, tr.top + 5, cargos->legend_colour);

							SetDParam(0, cargos->name);
							DrawString(tr.Shrink(ScaleGUITrad(14), 0, 0, 0), CM_STR_TOWN_CB_CARGO_NAME);
							break;
						}
						case WID_CB_CARGO_AMOUNT: {
							delivered = this->town->cb.delivered[cargos->Index()];
							requirements = CB_GetTownReq(this->town->cache.population, CB_GetReq(cargos->Index()), from, true);
							SetDParam(0, delivered);

							//when required
							if (this->town->cache.population >= from) {
								if((delivered + (uint)this->town->cb.stored[cargos->Index()]) >= requirements) string_to_draw = CM_STR_TOWN_CB_CARGO_AMOUNT_GOOD;
								else string_to_draw = CM_STR_TOWN_CB_CARGO_AMOUNT_BAD;
							}
							//when not required -> all faded
							else string_to_draw = CM_STR_TOWN_CB_CARGO_AMOUNT_NOT;

							DrawString(tr, string_to_draw, TC_FROMSTRING, SA_RIGHT);
							break;
						}
						case WID_CB_CARGO_REQ: {
							requirements = CB_GetTownReq(this->town->cache.population, CB_GetReq(cargos->Index()), from, true);
							SetDParam(0, requirements);
							 //when required
							string_to_draw = (this->town->cache.population >= from) ? CM_STR_TOWN_CB_CARGO_REQ_YES : CM_STR_TOWN_CB_CARGO_REQ_NOT;
							DrawString(tr, string_to_draw, TC_FROMSTRING, SA_RIGHT);
							break;
						}
						case WID_CB_CARGO_PREVIOUS: {
							requirements = CB_GetTownReq(this->town->cache.population, CB_GetReq(cargos->Index()), from, true);
							SetDParam(0, this->town->cb.delivered_last_month[cargos->Index()]);
							if (this->town->cache.population >= from){
								// if (this->town->delivered_enough[cargos->Index()]) {
									string_to_draw = (this->town->cb.delivered_last_month[cargos->Index()] >= requirements) ? CM_STR_TOWN_CB_CARGO_PREVIOUS_YES : CM_STR_TOWN_CB_CARGO_PREVIOUS_EDGE;
								// }
								// else string_to_draw = STR_TOWN_CB_CARGO_PREVIOUS_BAD;
							}
							else string_to_draw = CM_STR_TOWN_CB_CARGO_PREVIOUS_NOT;

							DrawString(tr, string_to_draw, TC_FROMSTRING, SA_RIGHT);
							break;
						}
						case WID_CB_CARGO_STORE: {
							SetDParam(0, this->town->cb.stored[cargos->Index()]);
							if (CB_GetDecay(cargos->Index()) == 100) string_to_draw = CM_STR_TOWN_CB_CARGO_STORE_DECAY;  //when 100% decay
							else {
								if (this->town->cache.population >= from) string_to_draw = CM_STR_TOWN_CB_CARGO_STORE_YES;  //when required
								else string_to_draw = CM_STR_TOWN_CB_CARGO_STORE_NOT;
							}

							DrawString(tr, string_to_draw, TC_FROMSTRING, SA_RIGHT);
							break;
						}
						case WID_CB_CARGO_STORE_PCT: {
							uint max_storage = CB_GetMaxTownStorage(this->town, cargos->Index());
							if (CB_GetDecay(cargos->Index()) == 100 || !max_storage) string_to_draw = CM_STR_TOWN_CB_CARGO_STORE_DECAY;  //when 100% decay
							else {
								SetDParam(0, 100 * this->town->cb.stored[cargos->Index()] / max_storage);
								if (this->town->cache.population >= from) string_to_draw = CM_STR_TOWN_CB_CARGO_STORE_PCT_YES;  //when required
								else string_to_draw = CM_STR_TOWN_CB_CARGO_STORE_PCT_NOT;
							}

							DrawString(tr, string_to_draw, TC_FROMSTRING, SA_RIGHT);
							break;
						}
						case WID_CB_CARGO_FROM: {
							SetDParam(0, from);
							string_to_draw = (this->town->cache.population >= from) ? CM_STR_TOWN_CB_CARGO_FROM_YES : CM_STR_TOWN_CB_CARGO_FROM_NOT; //when required

							DrawString(tr, string_to_draw, TC_FROMSTRING, SA_RIGHT);
							break;
						}
						//last case
					}
					//switch
					tr.top += EXP_LINESPACE;
					//cargo needed?
				}
				//for cycle
			}
			break;
		}
		/* Citybuilder things enabled*/
	}

	void OnPaint() override {
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


	EventState OnHotkey(int hotkey) override
	{
		TownExecuteAction(this->town, hotkey);
		return ES_HANDLED;
	}

	static inline HotkeyList hotkeys{"cm_town_cb_view", {
		Hotkey((uint16)0, "location", WID_TV_CENTER_VIEW),
		Hotkey((uint16)0, "local_authority", WID_TV_SHOW_AUTHORITY),
		Hotkey((uint16)0, "cb_window", WID_TV_CB),
		Hotkey(WKC_CTRL | 'S', "build_statue", HK_STATUE + 0x80),
	}};
};

static const NWidgetPart _nested_cb_town_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN, WID_CB_CAPTION), SetDataTip(CM_STR_TOWN_VIEW_CB_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHIMGBTN, COLOUR_BROWN, WID_CB_CENTER_VIEW), SetMinimalSize(12, 14), SetDataTip(SPR_GOTO_LOCATION, STR_TOWN_VIEW_CENTER_TOOLTIP),
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
						NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_CB_ADVERT),SetMinimalSize(33, 12),SetFill(1, 0), SetDataTip(CM_STR_CB_LARGE_ADVERTISING_CAMPAIGN, 0),
						NWidget(NWID_SPACER), SetMinimalSize(2, 0),
						NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_CB_FUND),SetMinimalSize(33, 12),SetFill(1, 0), SetDataTip(CM_STR_CB_NEW_BUILDINGS, 0),
						NWidget(NWID_SPACER), SetMinimalSize(4, 0),
					EndContainer(),
					NWidget(NWID_SPACER), SetMinimalSize(0, 2),
					NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
						NWidget(WWT_TEXTBTN, COLOUR_BROWN, WID_CB_ADVERT_REGULAR),SetMinimalSize(33, 12),SetFill(1, 0), SetDataTip(CM_STR_CB_ADVERT_REGULAR, CM_STR_CB_ADVERT_REGULAR_TT),
 						NWidget(NWID_SPACER), SetMinimalSize(2, 0),
						NWidget(WWT_TEXTBTN, COLOUR_BROWN, WID_CB_FUND_REGULAR),SetMinimalSize(33, 12),SetFill(1, 0), SetDataTip(CM_STR_CB_FUND_REGULAR, CM_STR_CB_FUND_REGULAR_TT),
						NWidget(NWID_SPACER), SetMinimalSize(4, 0),
					EndContainer(),
					NWidget(NWID_SPACER), SetMinimalSize(0, 2),
					NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
  						NWidget(NWID_SPACER), SetMinimalSize(33, 0), SetFill(1, 0),
  						NWidget(NWID_SPACER), SetMinimalSize(2, 0),
						NWidget(WWT_TEXTBTN, COLOUR_BROWN, WID_CB_POWERFUND),SetMinimalSize(33, 12),SetFill(1, 0), SetDataTip(CM_STR_CB_POWERFUND, CM_STR_CB_POWERFUND_TT),
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
			NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_CB_TOWN_VIEW), SetMinimalSize(40, 12), SetFill(1, 1), SetResize(1, 0), SetDataTip(CM_STR_CB_GUI_TOWN_VIEW_BUTTON, CM_STR_CB_GUI_TOWN_VIEW_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_CB_SHOW_AUTHORITY), SetMinimalSize(40, 12), SetFill(1, 1), SetResize(1, 0), SetDataTip(STR_TOWN_VIEW_LOCAL_AUTHORITY_BUTTON, STR_TOWN_VIEW_LOCAL_AUTHORITY_TOOLTIP),
		EndContainer(),
		NWidget(WWT_RESIZEBOX, COLOUR_BROWN),
	EndContainer(),
};

static WindowDesc _cb_town_desc(
	WDP_AUTO, "cm_town_cb", 160, 30,
	CM_WC_CB_TOWN, WC_NONE,
	0,
	_nested_cb_town_widgets,
	&CBTownWindow::hotkeys
);

void ShowCBTownWindow(uint town) {
	AllocateWindowDescFront<CBTownWindow>(_cb_town_desc, town);
}
