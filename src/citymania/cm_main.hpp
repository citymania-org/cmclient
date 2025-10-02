#ifndef CMEXT_MAIN_HPP
#define CMEXT_MAIN_HPP

#include "cm_type.hpp"
#include "cm_game.hpp"

namespace citymania {

extern up<Game> _game;

void ResetGame();
void SwitchToMode(SwitchMode new_mode);

} // namespace citymania

#endif
