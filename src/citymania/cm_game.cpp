#include "../stdafx.h"

#include "cm_game.hpp"

#include "../safeguards.h"

namespace citymania {

Game::Game() {
    this->events.listen<event::NewMonth>([] (const event::NewMonth &) {
        for (Town *t : Town::Iterate()) {
            t->cm.hs_last_month = t->cm.hs_total - t->cm.hs_total_prev;
            t->cm.hs_total_prev = t->cm.hs_total;
            t->cm.cs_last_month = t->cm.cs_total - t->cm.cs_total_prev;
            t->cm.cs_total_prev = t->cm.cs_total;
            t->cm.hr_last_month = t->cm.hr_total - t->cm.hr_total_prev;
            t->cm.hr_total_prev = t->cm.hr_total;

            t->cm.houses_reconstructed_last_month = t->cm.houses_reconstructed_this_month;
            t->cm.houses_reconstructed_this_month = 0;
            t->cm.houses_demolished_last_month = t->cm.houses_demolished_this_month;
            t->cm.houses_demolished_this_month = 0;
        }
    });
    this->events.listen<event::TownGrowthTick>([this] (const event::TownGrowthTick &event) {
        if (event.growth_result) {
            if (event.town->cache.num_houses <= event.prev_houses) {
                event.town->cm.hs_total++;
            }
        } else {
            event.town->cm.cs_total++;
        }
    });
    this->events.listen<event::HouseRebuilt>([this] (const event::HouseRebuilt &event) {
        if (event.was_successful) {
            event.town->cm.houses_reconstructed_this_month++;
        } else {
            event.town->cm.houses_demolished_this_month++;
        }
    });
    this->events.listen<event::HouseBuilt>([this] (const event::HouseBuilt &event) {
        event.town->cm.houses_constructing++;
    });
    this->events.listen<event::HouseCompleted>([this] (const event::HouseCompleted &event) {
        event.town->cm.houses_constructing--;
    });
}

} // namespace citymania