#include "../stdafx.h"

#include "../viewport_func.h"
#include "../window_func.h"
#include "../window_gui.h"
#include "../window_type.h"
#include "../zoom_func.h"
#include "../zoom_type.h"

#include "cm_locations.hpp"

#include "../safeguards.h"

namespace citymania {

struct Location {
    ZoomLevel zoom;
    int32 scrollpos_x = 0;
    int32 scrollpos_y = 0;
};

Location _locations[NUM_LOCATIONS];

void SaveLocation(uint slot) {
    assert(slot < NUM_LOCATIONS);
    auto &loc = _locations[slot];

    Window *w = FindWindowById(WC_MAIN_WINDOW, 0);
    auto &vp = w->viewport;

    loc.zoom = vp->zoom;
    loc.scrollpos_x = vp->scrollpos_x;
    loc.scrollpos_y = vp->scrollpos_y;
}

void LoadLocation(uint slot) {
    assert(slot < NUM_LOCATIONS);
    auto &loc = _locations[slot];
    if (!loc.scrollpos_y && !loc.scrollpos_x) return;

    Window *w = FindWindowById(WC_MAIN_WINDOW, 0);
    auto &vp = w->viewport;

    vp->zoom = loc.zoom;
    vp->follow_vehicle = VehicleID::Invalid();
    vp->dest_scrollpos_x = loc.scrollpos_x;
    vp->dest_scrollpos_y = loc.scrollpos_y;
    vp->virtual_width = ScaleByZoom(vp->width, vp->zoom);
    vp->virtual_height = ScaleByZoom(vp->height, vp->zoom);
    w->SetDirty();
}

EventState HandleToolbarHotkey(int hotkey) {
    if (hotkey >= CM_MTHK_LOCATIONS_GOTO_START && hotkey < CM_MTHK_LOCATIONS_GOTO_END) {
        LoadLocation(hotkey - CM_MTHK_LOCATIONS_GOTO_START);
        return ES_HANDLED;
    }
    if (hotkey >= CM_MTHK_LOCATIONS_SET_START && hotkey < CM_MTHK_LOCATIONS_SET_END) {
        SaveLocation(hotkey - CM_MTHK_LOCATIONS_SET_START);
        return ES_HANDLED;
    }
    return ES_NOT_HANDLED;
}


} // namespace citymania
