#ifndef CMEXT_HOTKEYS_HPP
#define CMEXT_HOTKEYS_HPP

namespace citymania {

extern bool _fn_mod;
extern bool _estimate_mod;
extern bool _remove_mod;

void UpdateModKeys(bool shift_pressed, bool ctrl_pressed, bool alt_pressed);

} // namespace citymania

#endif
