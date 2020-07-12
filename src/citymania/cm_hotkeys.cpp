#include "../stdafx.h"

#include "cm_hotkeys.hpp"
#include "cm_settings.hpp"

#include "../settings_type.h"
#include "../window_gui.h"
#include "../window_type.h"

#include "../safeguards.h"

namespace citymania {

bool _fn_mod = false;
bool _remove_mod = false;
bool _estimate_mod = false;


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

} // namespace citymania
