#include "../stdafx.h"

#include "cm_game.hpp"

#include "../company_base.h"

#include "../safeguards.h"

namespace citymania {

Game::Game() {
    this->events.listen<event::NewMonth>(event::Slot::GAME, [this] (const event::NewMonth &) {
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

        this->towns_growth_tiles_last_month = this->towns_growth_tiles;
        this->towns_growth_tiles.clear();
    });
    this->events.listen<event::TownBuilt>(event::Slot::GAME, [this] (const event::TownBuilt &event) {
        t->cm.hs_total = t->cm.hs_last_month = t->cm.hs_total_prev = 0;
        t->cm.cs_total = t->cm.cs_last_month = t->cm.cs_total_prev = 0;
        t->cm.hr_total = t->cm.hr_last_month = t->cm.hr_total_prev = 0;
    });

    this->events.listen<event::TownGrowthSucceeded>(event::Slot::GAME, [this] (const event::TownGrowthSucceeded &event) {
        if (event.town->cache.num_houses <= event.prev_houses) {
            event.town->cm.hs_total++;
            this->set_town_growth_tile(event.tile, TownGrowthTileState::HS);
        }
    });

    this->events.listen<event::TownGrowthFailed>(event::Slot::GAME, [this] (const event::TownGrowthFailed &event) {
        event.town->cm.cs_total++;
        this->set_town_growth_tile(event.tile, TownGrowthTileState::CS);
    });

    this->events.listen<event::HouseRebuilt>(event::Slot::GAME, [this] (const event::HouseRebuilt &event) {
        if (event.was_successful) {
            event.town->cm.houses_reconstructed_this_month++;
            this->set_town_growth_tile(event.tile, TownGrowthTileState::RH_REBUILT);
        } else {
            event.town->cm.houses_demolished_this_month++;
            this->set_town_growth_tile(event.tile, TownGrowthTileState::RH_REMOVED);
        }
    });

    this->events.listen<event::HouseBuilt>(event::Slot::GAME, [this] (const event::HouseBuilt &event) {
        event.town->cm.houses_constructing++;
        event.town->cm.real_population += event.house_spec->population;
        this->set_town_growth_tile(event.tile, TownGrowthTileState::NEW_HOUSE);
    });

    this->events.listen<event::HouseCleared>(event::Slot::GAME, [this] (const event::HouseCleared &event) {
        if (!event.was_completed)
            event.town->cm.houses_constructing--;
        event.town->cm.real_population -= event.house_spec->population;
    });

    this->events.listen<event::HouseCompleted>(event::Slot::GAME, [this] (const event::HouseCompleted &event) {
        event.town->cm.houses_constructing--;
    });

    this->events.listen<event::TownCachesRebuilt>(event::Slot::GAME, [this] (const event::TownCachesRebuilt &event) {
        for (Town *town : Town::Iterate()) {
            town->cm.real_population = 0;
            town->cm.houses_constructing = 0;
        }
        for (TileIndex t = 0; t < MapSize(); t++) {
            if (!IsTileType(t, MP_HOUSE)) continue;
            Town *town = Town::GetByTile(t);
            if (!IsHouseCompleted(t))
                town->cm.houses_constructing++;
            HouseID house_id = GetHouseType(t);
            town->cm.real_population += HouseSpec::Get(house_id)->population;
        }
    });

    this->events.listen<event::CargoAccepted>(event::Slot::GAME, [this] (const event::CargoAccepted &event) {
        event.company->cur_economy.cm.cargo_income[event.cargo_type] += event.profit;
    });
}

void Game::set_town_growth_tile(TileIndex tile, TownGrowthTileState state) {
    if (this->towns_growth_tiles[tile] < state) this->towns_growth_tiles[tile] = state;
}

} // namespace citymania