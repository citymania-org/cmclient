/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file statusbar_gui.cpp The GUI for the bottom status bar. */

#include "stdafx.h"
#include "core/backup_type.hpp"
#include "gfx_func.h"
#include "news_func.h"
#include "company_func.h"
#include "string_func.h"
#include "strings_func.h"
#include "company_base.h"
#include "tilehighlight_func.h"
#include "news_gui.h"
#include "company_gui.h"
#include "window_gui.h"
#include "saveload/saveload.h"
#include "window_func.h"
#include "statusbar_gui.h"
#include "toolbar_gui.h"
#include "core/geometry_func.hpp"
#include "zoom_func.h"
#include "timer/timer.h"
#include "timer/timer_game_calendar.h"
#include "timer/timer_window.h"

#include "widgets/statusbar_widget.h"

#include "table/strings.h"
#include "table/sprites.h"

#include "citymania/cm_hotkeys.hpp"
#include "citymania/cm_commands.hpp"

#include "safeguards.h"

static bool DrawScrollingStatusText(const NewsItem &ni, int scroll_pos, int left, int right, int top, int bottom)
{
	/* Replace newlines and the likes with spaces. */
	std::string message = StrMakeValid(ni.GetStatusText(), StringValidationSetting::ReplaceTabCrNlWithSpace);

	DrawPixelInfo tmp_dpi;
	if (!FillDrawPixelInfo(&tmp_dpi, left, top, right - left, bottom)) return true;

	int width = GetStringBoundingBox(message).width;
	int pos = (_current_text_dir == TD_RTL) ? (scroll_pos - width) : (right - scroll_pos - left);

	AutoRestoreBackup dpi_backup(_cur_dpi, &tmp_dpi);
	DrawString(pos, INT16_MAX, 0, message, TC_LIGHT_BLUE, SA_LEFT | SA_FORCE);

	return (_current_text_dir == TD_RTL) ? (pos < right - left) : (pos + width > 0);
}

struct StatusBarWindow : Window {
	bool saving = false;
	int ticker_scroll = TICKER_STOP;

	static const int TICKER_STOP    = 1640; ///< scrolling is finished when counter reaches this value
	static const int COUNTER_STEP   =    2; ///< this is subtracted from active counters every tick
	static constexpr auto REMINDER_START = std::chrono::milliseconds(1350); ///< time in ms for reminder notification (red dot on the right) to stay

	StatusBarWindow(WindowDesc &desc) : Window(desc)
	{
		this->InitNested();
		this->flags.Reset(WindowFlag::WhiteBorder);
		PositionStatusbar(this);
	}

	Point OnInitialPosition([[maybe_unused]] int16_t sm_width, [[maybe_unused]] int16_t sm_height, [[maybe_unused]] int window_number) override
	{
		Point pt = { 0, _screen.height - sm_height };
		return pt;
	}

	void FindWindowPlacementAndResize([[maybe_unused]] int def_width, [[maybe_unused]] int def_height) override
	{
		Window::FindWindowPlacementAndResize(_toolbar_width, def_height);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		Dimension d;
		switch (widget) {
			case WID_S_LEFT:
				d = GetStringBoundingBox(GetString(STR_JUST_DATE_LONG, GetParamMaxValue(TimerGameCalendar::DateAtStartOfYear(CalendarTime::MAX_YEAR).base())));
				break;

			case WID_S_RIGHT: {
				int64_t max_money = UINT32_MAX;
				for (const Company *c : Company::Iterate()) max_money = std::max<int64_t>(c->money, max_money);
				d = GetStringBoundingBox(GetString(STR_JUST_CURRENCY_LONG, 100LL * max_money));
				break;
			}

			case CM_WID_S_APM:
				if (!_settings_client.gui.cm_show_apm) {
					size = Dimension(0, 0);
					return;
				}
				d = GetStringBoundingBox(GetString(CM_STR_STATUSBAR_APM, 999, 999, 9999));
				break;

			default:
				return;
		}

		d.width += padding.width;
		d.height += padding.height;
		size = maxdim(d, size);
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		Rect tr = r.Shrink(WidgetDimensions::scaled.framerect, RectPadding::zero);
		tr.top = CenterBounds(r.top, r.bottom, GetCharacterHeight(FS_NORMAL));
		switch (widget) {
			case WID_S_LEFT:
				/* Draw the date */
				DrawString(tr, GetString(STR_JUST_DATE_LONG, TimerGameCalendar::date), TC_WHITE, SA_HOR_CENTER);
				break;

			case WID_S_RIGHT: {
				if (_local_company == COMPANY_SPECTATOR) {
					DrawString(tr, STR_STATUSBAR_SPECTATOR, TC_FROMSTRING, SA_HOR_CENTER);
				} else if (_settings_game.difficulty.infinite_money) {
					DrawString(tr, STR_STATUSBAR_INFINITE_MONEY, TC_FROMSTRING, SA_HOR_CENTER);
				} else {
					/* Draw company money, if any */
					const Company *c = Company::GetIfValid(_local_company);
					if (c != nullptr) {
						DrawString(tr, GetString(STR_JUST_CURRENCY_LONG, c->money), TC_WHITE, SA_HOR_CENTER);
					}
				}
				break;
			}

			case WID_S_MIDDLE:
				/* Draw status bar */
				if (this->saving) { // true when saving is active
					DrawString(tr, STR_STATUSBAR_SAVING_GAME, TC_FROMSTRING, SA_HOR_CENTER | SA_VERT_CENTER);
				} else if (_do_autosave) {
					DrawString(tr, STR_STATUSBAR_AUTOSAVE, TC_FROMSTRING, SA_HOR_CENTER);
				} else if (_pause_mode.Any()) {
					StringID msg = _pause_mode.Test(PauseMode::LinkGraph) ? STR_STATUSBAR_PAUSED_LINK_GRAPH : STR_STATUSBAR_PAUSED;
					DrawString(tr, msg, TC_FROMSTRING, SA_HOR_CENTER);
				} else if (this->ticker_scroll < TICKER_STOP && GetStatusbarNews() != nullptr && !GetStatusbarNews()->headline.empty()) {
					/* Draw the scrolling news text */
					if (!DrawScrollingStatusText(*GetStatusbarNews(), ScaleGUITrad(this->ticker_scroll), tr.left, tr.right, tr.top, tr.bottom)) {
						InvalidateWindowData(WC_STATUS_BAR, 0, SBI_NEWS_DELETED);
						if (Company::IsValidID(_local_company)) {
							/* This is the default text */
							DrawString(tr, GetString(STR_STATUSBAR_COMPANY_NAME, _local_company), TC_FROMSTRING, SA_HOR_CENTER);
						}
					}
				} else {
					if (Company::IsValidID(_local_company)) {
						/* This is the default text */
						DrawString(tr, GetString(STR_STATUSBAR_COMPANY_NAME, _local_company), TC_FROMSTRING, SA_HOR_CENTER);
					}
				}

				if (!this->reminder_timeout.HasFired()) {
					Dimension icon_size = GetSpriteSize(SPR_UNREAD_NEWS);
					DrawSprite(SPR_UNREAD_NEWS, PAL_NONE, tr.right - icon_size.width, CenterBounds(r.top, r.bottom, icon_size.height));
				}
				break;

			case CM_WID_S_APM:
				if (_settings_client.gui.cm_show_apm) {
					auto epm = citymania::GetEPM();
					auto str = GetString(
						CM_STR_STATUSBAR_APM,
						epm.second,
						epm.first,
						std::min(citymania::get_average_command_lag(), 9999)
					);
					DrawString(tr, str, TC_FROMSTRING, SA_HOR_CENTER);
				}
				break;
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
		switch (data) {
			default: NOT_REACHED();
			case SBI_SAVELOAD_START:  this->saving = true;  break;
			case SBI_SAVELOAD_FINISH: this->saving = false; break;
			case SBI_SHOW_TICKER:     this->ticker_scroll = 0; break;
			case SBI_SHOW_REMINDER:   this->reminder_timeout.Reset(); break;
			case SBI_NEWS_DELETED:
				this->ticker_scroll    =   TICKER_STOP; // reset ticker ...
				this->reminder_timeout.Abort(); // ... and reminder
				break;
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_S_MIDDLE: ShowLastNewsMessage(); break;
			case WID_S_RIGHT:  if (_local_company != COMPANY_SPECTATOR) ShowCompanyFinances(_local_company); break;
			default: ResetObjectToPlace();
		}
	}

	/** Move information on the ticker slowly from one side to the other. */
	IntervalTimer<TimerWindow> ticker_scroll_interval = {std::chrono::milliseconds(15), [this](uint count) {
		if (_pause_mode.Any()) return;

		if (this->ticker_scroll < TICKER_STOP) {
			this->ticker_scroll += count;
			this->SetWidgetDirty(WID_S_MIDDLE);
		}
	}};

	TimeoutTimer<TimerWindow> reminder_timeout = {REMINDER_START, [this]() {
		this->SetWidgetDirty(WID_S_MIDDLE);
	}};

	IntervalTimer<TimerGameCalendar> daily_interval = {{TimerGameCalendar::DAY, TimerGameCalendar::Priority::NONE}, [this](auto) {
		this->SetWidgetDirty(WID_S_LEFT);
	}};

	TimeoutTimer<TimerWindow> cm_epm_interval = {std::chrono::seconds(1), [this]() {
		this->SetWidgetDirty(CM_WID_S_APM);
	}};
};

static constexpr NWidgetPart _nested_main_status_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_GREY, WID_S_LEFT), SetMinimalSize(140, 12), EndContainer(),
		NWidget(WWT_PUSHBTN, COLOUR_GREY, WID_S_MIDDLE), SetMinimalSize(40, 12), SetToolTip(STR_STATUSBAR_TOOLTIP_SHOW_LAST_NEWS), SetResize(1, 0),
		NWidget(WWT_PANEL, COLOUR_GREY, CM_WID_S_APM), SetMinimalSize(100, 12), EndContainer(),
		NWidget(WWT_PUSHBTN, COLOUR_GREY, WID_S_RIGHT), SetMinimalSize(140, 12),
	EndContainer(),
};

static WindowDesc _main_status_desc(
	WDP_MANUAL, nullptr, 0, 0,
	WC_STATUS_BAR, WC_NONE,
	{WindowDefaultFlag::NoFocus, WindowDefaultFlag::NoClose},
	_nested_main_status_widgets
);

/**
 * Checks whether the news ticker is currently being used.
 */
bool IsNewsTickerShown()
{
	const StatusBarWindow *w = dynamic_cast<StatusBarWindow*>(FindWindowById(WC_STATUS_BAR, 0));
	return w != nullptr && w->ticker_scroll < StatusBarWindow::TICKER_STOP;
}

/**
 * Show our status bar.
 */
void ShowStatusBar()
{
	new StatusBarWindow(_main_status_desc);
}

void CM_RedrawStatusBar() {
	StatusBarWindow *w = dynamic_cast<StatusBarWindow*>(FindWindowById(WC_STATUS_BAR, 0));
	w->ReInit();
}
