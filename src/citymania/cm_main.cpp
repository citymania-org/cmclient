#include "../stdafx.h"

#include "cm_main.hpp"

#include "../smallmap_gui.h"
#include "../window_func.h"

#include "../safeguards.h"

namespace citymania {

up<Game> _game = nullptr;

void ResetGame() {
    _game = make_up<Game>();
}

void SwitchToMode(SwitchMode new_mode) {
    ResetGame();
}

void ToggleSmallMap() {
    SmallMapWindow *w = dynamic_cast<SmallMapWindow*>(FindWindowById(WC_SMALLMAP, 0));
    if (w == nullptr) ShowSmallMap();
    delete w;
}

} // namespace citymania