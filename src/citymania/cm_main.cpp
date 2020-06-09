#include "../stdafx.h"

#include "cm_main.hpp"

#include "../safeguards.h"

namespace citymania {

up<Game> _game = nullptr;

void ResetGame() {
    _game = make_up<Game>();
}

void SwitchToMode(SwitchMode new_mode) {
    ResetGame();
}


} // namespace citymania