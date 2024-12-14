#include "../stdafx.h"

#include "cm_hotkeys.hpp"
#include "cm_settings.hpp"
#include "cm_station_gui.hpp"

#include "../newgrf_station.h"
#include "../settings_type.h"
#include "../sound_func.h"
#include "../tilehighlight_func.h"
#include "../viewport_func.h"
#include "../window_func.h"
#include "../window_gui.h"
#include "../window_type.h"
#include "../widgets/rail_widget.h"
#include "../widgets/road_widget.h"

#include <optional>
#include <queue>

#include "../safeguards.h"

extern citymania::RailStationGUISettings _railstation; ///< Settings of the station builder GUI

namespace citymania {

bool _fn_mod = false;
bool _remove_mod = false;
bool _estimate_mod = false;

bool _middle_button_down;     ///< Is middle mouse button pressed?
bool _middle_button_clicked;  ///< Is middle mouse button clicked?

static uint32 _effective_actions = 0;
static std::optional<std::chrono::steady_clock::time_point> _first_effective_tick = {};
static std::queue<std::chrono::steady_clock::time_point> _last_actions;

const std::chrono::minutes EPM_PERIOD(1);  ///< Actions per minute measuring period is, suprisingly, one minute

static void PurgeLastActions() {
    auto now = std::chrono::steady_clock::now();
    while (!_last_actions.empty() && _last_actions.front() <= now)
        _last_actions.pop();
}

void CountEffectiveAction() {
    auto now = std::chrono::steady_clock::now();
    if (!_first_effective_tick) _first_effective_tick = now;
    _effective_actions++;
    PurgeLastActions();
    _last_actions.push(now + EPM_PERIOD);
}

void ResetEffectiveActionCounter() {
    _first_effective_tick = {};
    _effective_actions = 0;
    std::queue<std::chrono::steady_clock::time_point>().swap(_last_actions);  // clear the queue
}

std::pair<uint32, uint32> GetEPM() {
    auto now = std::chrono::steady_clock::now();
    if (!_first_effective_tick) return std::make_pair(0, 0);
    PurgeLastActions();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - *_first_effective_tick).count();
    if (ms == 0) return std::make_pair(0, 0);
    return std::make_pair(_effective_actions * 60000 / ms,
                          _last_actions.size());
}

bool HasSeparateRemoveMod() {
    return (_settings_client.gui.cm_fn_mod != _settings_client.gui.cm_remove_mod);
}

void UpdateModKeys(bool shift_pressed, bool ctrl_pressed, bool alt_pressed, bool command_pressed) {
    bool mod_pressed[(size_t)ModKey::END] = {false};
    if (shift_pressed) mod_pressed[(size_t)ModKey::SHIFT] = true;
    if (ctrl_pressed) mod_pressed[(size_t)ModKey::CTRL] = true;
    if (alt_pressed) mod_pressed[(size_t)ModKey::ALT] = true;
    if (command_pressed) mod_pressed[(size_t)ModKey::COMMAND] = true;
    bool fn_mod_prev = _fn_mod;
    bool remove_mod_prev = _remove_mod;
    _fn_mod = mod_pressed[(size_t)_settings_client.gui.cm_fn_mod];
    _remove_mod = mod_pressed[(size_t)_settings_client.gui.cm_remove_mod];
    _estimate_mod = mod_pressed[(size_t)_settings_client.gui.cm_estimate_mod];

    if (fn_mod_prev != _fn_mod) {
        for (auto w : Window::IterateFromFront()) {
            if (w->CM_OnFnModStateChange() == ES_HANDLED) break;
        }
    }
    if (remove_mod_prev != _remove_mod) {
        for (auto w : Window::IterateFromFront()) {
            if (w->CM_OnRemoveModStateChange() == ES_HANDLED) break;
        }
    }
}

ToolRemoveMode RailToolbar_GetRemoveMode(int widget) {
    switch(widget) {
        case WID_RAT_BUILD_NS:
        case WID_RAT_BUILD_X:
        case WID_RAT_BUILD_EW:
        case WID_RAT_BUILD_Y:
        case WID_RAT_AUTORAIL:
        case WID_RAT_POLYRAIL:
            return ToolRemoveMode::MOD;

        case WID_RAT_BUILD_WAYPOINT:
        case WID_RAT_BUILD_STATION:
        case WID_RAT_BUILD_SIGNALS:
            return HasSeparateRemoveMod() ? ToolRemoveMode::MOD : ToolRemoveMode::BUTTON;

        default:
            return ToolRemoveMode::NONE;
    }
}

bool RailToolbar_IsRemoveInverted([[maybe_unused]] int widget) {
    return false;
    // return (RailToolbar_GetRemoveMode(widget) != ToolRemoveMode::NONE && citymania::_fn_mod);
}

void RailToolbar_UpdateRemoveWidgetStatus(Window *w, int widget, bool remove_active) {
    if (widget == WID_RAT_REMOVE) return;

    if (RailToolbar_GetRemoveMode(widget) == citymania::ToolRemoveMode::NONE || !w->IsWidgetLowered(widget)) {
        w->DisableWidget(WID_RAT_REMOVE);
        w->RaiseWidget(WID_RAT_REMOVE);
    } else {
        w->EnableWidget(WID_RAT_REMOVE);
        w->SetWidgetLoweredState(WID_RAT_REMOVE, remove_active);
        SetSelectionRed(remove_active);
    }
    w->SetWidgetDirty(WID_RAT_REMOVE);

    /* handle station builder */
    if (w->IsWidgetLowered(WID_RAT_BUILD_STATION)) {
        if (remove_active) {
            /* starting drag & drop remove */
            if (!_settings_client.gui.station_dragdrop) {
                SetTileSelectSize(1, 1);
            } else {
                VpSetPlaceSizingLimit(-1);
            }
        } else {
            /* starting station build mode */
            if (!_settings_client.gui.station_dragdrop) {
                int x = _settings_client.gui.station_numtracks;
                int y = _settings_client.gui.station_platlength;
                if (_railstation.orientation == 0) Swap(x, y);
                SetTileSelectSize(x, y);
            } else {
                VpSetPlaceSizingLimit(_settings_game.station.station_spread);
            }
        }
    }
}

bool RailToolbar_RemoveModChanged(Window *w, bool invert_remove, bool remove_active, bool button_clicked) {
    if (w->IsWidgetDisabled(WID_RAT_REMOVE)) return false;

    CloseWindowById(WC_SELECT_STATION, 0);
    for (uint i = WID_RAT_BUILD_NS; i < WID_RAT_REMOVE; i++) {
        if (w->IsWidgetLowered(i)) {
            auto old_active = remove_active;
            switch (RailToolbar_GetRemoveMode(i)) {
                case ToolRemoveMode::BUTTON:
                    if (button_clicked) remove_active = !w->IsWidgetLowered(WID_RAT_REMOVE);
                    break;
                case ToolRemoveMode::MOD:
                    if (_remove_mod || !button_clicked) remove_active = (_remove_mod != invert_remove);
                    else remove_active = !w->IsWidgetLowered(WID_RAT_REMOVE);
                    break;
                default:
                    break;
            }
            if (old_active != remove_active) {
                if (button_clicked && _settings_client.sound.click_beep) SndPlayFx(SND_15_BEEP);
                RailToolbar_UpdateRemoveWidgetStatus(w, i, remove_active);
            }
            return remove_active;
        }
    }
    return remove_active;
}

ToolRemoveMode RoadToolbar_GetRemoveMode(int widget) {
    switch(widget) {
        case WID_ROT_ROAD_X:
        case WID_ROT_ROAD_Y:
        case WID_ROT_AUTOROAD:
            return ToolRemoveMode::MOD;

        case WID_ROT_BUS_STATION:
        case WID_ROT_TRUCK_STATION:
            return HasSeparateRemoveMod() ? ToolRemoveMode::MOD : ToolRemoveMode::BUTTON;

        default:
            return ToolRemoveMode::NONE;
    }
}

void RoadToolbar_UpdateOptionWidgetStatus(Window *w, int widget, bool remove_active, bool is_road) {

    switch (widget) {
        case WID_ROT_REMOVE:
        case WID_ROT_ONE_WAY:
            return;

        case WID_ROT_BUS_STATION:
        case WID_ROT_TRUCK_STATION:
            if (is_road) w->DisableWidget(WID_ROT_ONE_WAY);
            break;

        case WID_ROT_ROAD_X:
        case WID_ROT_ROAD_Y:
        case WID_ROT_AUTOROAD:
            if (is_road) w->SetWidgetDisabledState(WID_ROT_ONE_WAY, !w->IsWidgetLowered(widget));
            break;

        default:
            if (is_road) {
                w->SetWidgetDisabledState(WID_ROT_ONE_WAY, true);
                w->SetWidgetLoweredState(WID_ROT_ONE_WAY, false);
            }

            break;
    }


    if (RoadToolbar_GetRemoveMode(widget) == citymania::ToolRemoveMode::NONE || !w->IsWidgetLowered(widget)) {
        w->DisableWidget(WID_ROT_REMOVE);
        w->RaiseWidget(WID_ROT_REMOVE);
    } else {
        w->EnableWidget(WID_ROT_REMOVE);
        w->SetWidgetLoweredState(WID_ROT_REMOVE, remove_active);
        SetSelectionRed(remove_active);
    }
    w->SetWidgetDirty(WID_ROT_REMOVE);
}

bool RoadToolbar_RemoveModChanged(Window *w, bool remove_active, bool button_clicked, bool is_road) {
    if (w->IsWidgetDisabled(WID_ROT_REMOVE)) return false;
    CloseWindowById(WC_SELECT_STATION, 0);
    for (uint i = WID_ROT_ROAD_X; i < WID_ROT_REMOVE; i++) {
        if (w->IsWidgetLowered(i)) {
            auto old_active = remove_active;
            switch (RoadToolbar_GetRemoveMode(i)) {
                case ToolRemoveMode::BUTTON:
                    if (button_clicked) remove_active = !w->IsWidgetLowered(WID_ROT_REMOVE);
                    break;
                case ToolRemoveMode::MOD:
                    if (_remove_mod || !button_clicked) remove_active = _remove_mod;
                    else remove_active = !w->IsWidgetLowered(WID_ROT_REMOVE);
                    break;
                default:
                    break;
            }
            if (old_active != remove_active) {
                if (button_clicked && _settings_client.sound.click_beep) SndPlayFx(SND_15_BEEP);
                RoadToolbar_UpdateOptionWidgetStatus(w, i, remove_active, is_road);
            }
            return remove_active;
        }
    }
    return remove_active;
}

bool ChooseSignalDragBehaviour() {
    if (_settings_client.gui.cm_invert_fn_for_signal_drag) return !_fn_mod;
    return _fn_mod;
}

void CountHotkeyStats(const HotkeyList *list, int hotkey) {
    auto h = list->CMGetHotkey(hotkey);
    if (!h) return;
    auto key = fmt::format("{}.{}", h->first, h->second.name);
    _game_session_stats.cm.hotkeys[key]++;
}

} // namespace citymania
