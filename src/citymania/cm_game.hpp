#ifndef CMEXT_GAME_HPP
#define CMEXT_GAME_HPP

#include "../town.h"

#include "cm_event.hpp"

namespace citymania {

class Game {
public:
    event::Dispatcher events;

    Game();
};

} // namespace citymania

#endif
