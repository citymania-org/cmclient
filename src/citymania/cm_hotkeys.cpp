#include "../stdafx.h"

#include "cm_hotkeys.hpp"
#include "cm_settings.hpp"

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

#include <queue>

#include "../safeguards.h"

struct RailStationGUISettings {
    Axis orientation;                 ///< Currently selected rail station orientation

    bool newstations;                 ///< Are custom station definitions available?
    StationClassID station_class;     ///< Currently selected custom station class (if newstations is \c true )
    byte station_type;                ///< %Station type within the currently selected custom station class (if newstations is \c true )
    byte station_count;               ///< Number of custom stations (if newstations is \c true )
};
extern RailStationGUISettings _railstation; ///< Settings of the station builder GUI
extern uint32 _realtime_tick;

namespace citymania {

bool _fn_mod = false;
bool _remove_mod = false;
bool _estimate_mod = false;

uint32 _effective_actions = 0;
uint32 _first_effective_tick = 0;
std::queue<uint32> _last_actions;

static void PurgeLastActions() {
    while (!_last_actions.empty() && _last_actions.front() <= _realtime_tick)
        _last_actions.pop();
}

void CountEffectiveAction() {
    if (!_first_effective_tick) _first_effective_tick = _realtime_tick;
    _effective_actions++;
    PurgeLastActions();
    _last_actions.push(_realtime_tick + 60000);
}

void ResetEffectivveActionCounter() {
    _first_effective_tick = 0;
    _effective_actions = 0;
    std::queue<uint32>().swap(_last_actions);  // clear the queue
}

std::pair<uint32, uint32> GetEPM() {
    if (!_first_effective_tick || _realtime_tick <= _first_effective_tick) return std::make_pair(0, 0);
    PurgeLastActions();
    return std::make_pair(_effective_actions * 60000 / (_realtime_tick - _first_effective_tick), _last_actions.size());
}

void UpdateModKeys(bool shift_pressed, bool ctrl_pressed, bool alt_pressed) {
    ModKey key = ModKey::NONE;
    if (alt_pressed) key = ModKey::ALT;
    if (ctrl_pressed) key = ModKey::CTRL;
    if (shift_pressed) key = ModKey::SHIFT;
    bool fn_mod_prev = _fn_mod;
    bool remove_mod_prev = _remove_mod;
    _fn_mod = (_settings_client.gui.cm_fn_mod == key);
    _remove_mod = (_settings_client.gui.cm_remove_mod == key);
    _estimate_mod = (_settings_client.gui.cm_estimate_mod == key);

    Window *w;
    if (fn_mod_prev != _fn_mod) {
        FOR_ALL_WINDOWS_FROM_FRONT(w) {
            if (w->CM_OnFnModStateChange() == ES_HANDLED) break;
        }
    }
    if (remove_mod_prev != _remove_mod) {
        FOR_ALL_WINDOWS_FROM_FRONT(w) {
            if (w->CM_OnRemoveModStateChange() == ES_HANDLED) break;
        }
    }
}

bool HasSeparateRemoveMod() {
    return (_settings_client.gui.cm_fn_mod != _settings_client.gui.cm_remove_mod);
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

bool RailToolbar_IsRemoveInverted(int widget) {
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

    DeleteWindowById(WC_SELECT_STATION, 0);
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
    DeleteWindowById(WC_SELECT_STATION, 0);
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


} // namespace citymania
