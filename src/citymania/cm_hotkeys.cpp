#include "../stdafx.h"

#include "cm_hotkeys.hpp"
#include "cm_settings.hpp"
#include "cm_station_gui.hpp"  // StationPickerSelection

#include "../newgrf_station.h"
#include "../settings_type.h"
#include "../sound_func.h"
#include "../tilehighlight_func.h"
#include "../viewport_func.h"
#include "../vehicle_base.h"
#include "../window_func.h"
#include "../window_gui.h"
#include "../window_type.h"
#include "../widgets/rail_widget.h"
#include "../widgets/road_widget.h"

#include <optional>
#include <queue>

#include "../safeguards.h"

extern StationPickerSelection _station_gui; ///< Settings of the station picker.
extern bool _generating_world;

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
    if (_generating_world) return;
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
    if (ms < 1000) return std::make_pair(0, _last_actions.size());
    return std::make_pair(_effective_actions * 60000 / ms,
                          _last_actions.size());
}

bool HasSeparateRemoveMod() {
    return (_settings_client.gui.cm_fn_mod != _settings_client.gui.cm_remove_mod);
}

void UpdateModKeys(bool shift_pressed, bool ctrl_pressed, bool alt_pressed, bool command_pressed) {
    bool mod_pressed[(size_t)ModKey::End] = {false};
    if (shift_pressed) mod_pressed[(size_t)ModKey::Shift] = true;
    if (ctrl_pressed) mod_pressed[(size_t)ModKey::Ctrl] = true;
    if (alt_pressed) mod_pressed[(size_t)ModKey::Alt] = true;
    if (command_pressed) mod_pressed[(size_t)ModKey::Command] = true;
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
                if (_station_gui.axis == 0) std::swap(x, y);
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

static StationOrderModAction GetStationOrderModAction()
{
    if (_ctrl_pressed) {
        if (_shift_pressed)
            return (StationOrderModAction)_settings_client.gui.cm_ctrl_shift_station_mod;
        else if (_alt_pressed)
            return (StationOrderModAction)_settings_client.gui.cm_alt_ctrl_station_mod;
        else
            return (StationOrderModAction)_settings_client.gui.cm_ctrl_station_mod;
    } else if (_shift_pressed) {
        if (_alt_pressed)
            return (StationOrderModAction)_settings_client.gui.cm_alt_shift_station_mod;
        else
            return (StationOrderModAction)_settings_client.gui.cm_shift_station_mod;
    } else if (_alt_pressed)
        return (StationOrderModAction)_settings_client.gui.cm_alt_station_mod;
    return StationOrderModAction::None;
}

DepotOrderModAction GetDepotOrderModAction() {
    if (_ctrl_pressed) {
        if (_shift_pressed) return (DepotOrderModAction)_settings_client.gui.cm_ctrl_shift_depot_mod;
        else return (DepotOrderModAction)_settings_client.gui.cm_ctrl_depot_mod;
    } else if (_shift_pressed) {
        return (DepotOrderModAction)_settings_client.gui.cm_shift_depot_mod;
    }
    return DepotOrderModAction::None;
}

StationModOrders GetStationModOrders(const Vehicle *v)
{
    StationModOrders res = {
        OLF_LOAD_IF_POSSIBLE,
        OUF_UNLOAD_IF_POSSIBLE,
        FeederOrderMod::None,
    };

    switch(GetStationOrderModAction()) {
        case StationOrderModAction::None:
            break;

        case StationOrderModAction::FullLoad:
            res.load = OLF_FULL_LOAD_ANY;
            break;

        case StationOrderModAction::Transfer:
            res.unload = OUFB_TRANSFER;
            if (_settings_client.gui.cm_no_loading_on_transfer_order)
                res.load = OLFB_NO_LOAD;
            break;

        case StationOrderModAction::UnloadAll:
            res.unload = OUFB_UNLOAD;
            if (_settings_client.gui.cm_no_loading_on_unload_order)
                res.load = OLFB_NO_LOAD;
            break;

        case StationOrderModAction::FeederLoad:
            if (v->GetNumOrders() > 0) res.mod = FeederOrderMod::Load;
            res.unload = OUFB_NO_UNLOAD;
            res.load = OLF_FULL_LOAD_ANY;
            break;

        case StationOrderModAction::FeederUnload:
            if (v->GetNumOrders() > 0) res.mod = FeederOrderMod::Unload;
            res.unload = OUFB_TRANSFER;
            res.load = OLFB_NO_LOAD;
            break;

        case StationOrderModAction::NoLoad:
            res.load = OLFB_NO_LOAD;
            break;

        case StationOrderModAction::NoUnload:
            res.unload = OUFB_NO_UNLOAD;
            break;

        default: NOT_REACHED();
    }
    return res;
}

} // namespace citymania
