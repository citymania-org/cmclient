#include "../stdafx.h"

#include "cm_main.hpp"
#include "cm_hotkeys.hpp"

#include "../smallmap_gui.h"
#include "../window_func.h"

#include "../safeguards.h"

namespace citymania {

up<Game> _game = nullptr;

void ResetGame() {
    _game = make_up<Game>();
    ResetEffectivveActionCounter();
}

void SwitchToMode(SwitchMode new_mode) {
    if (new_mode != SM_SAVE_GAME) ResetGame();
}

void ToggleSmallMap() {
    SmallMapWindow *w = dynamic_cast<SmallMapWindow*>(FindWindowById(WC_SMALLMAP, 0));
    if (w == nullptr) ShowSmallMap();
    delete w;
}

} // namespace citymania