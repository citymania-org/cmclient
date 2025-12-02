#include "../stdafx.h"

#include "cm_game.hpp"

#include "../company_base.h"
#include "../timer/timer.h"

#include "../safeguards.h"

namespace citymania {

Game::Game() {
    this->events.listen<event::NewMonth>(event::Slot::GAME, [this] (const event::NewMonth &) {
        for (Town *t : Town::Iterate()) {
            t->cm.hs_last_month = t->cm.hs_this_month;
            t->cm.cs_last_month = t->cm.cs_this_month;
            t->cm.hr_last_month = t->cm.hr_this_month;
            t->cm.hs_this_month = 0;
            t->cm.cs_this_month = 0;
            t->cm.hr_this_month = 0;

            t->cm.houses_reconstructed_last_month = t->cm.houses_reconstructed_this_month;
            t->cm.houses_reconstructed_this_month = 0;
            t->cm.houses_demolished_last_month = t->cm.houses_demolished_this_month;
            t->cm.houses_demolished_this_month = 0;

            t->cm.growth_tiles_last_month.swap(t->cm.growth_tiles);
            t->cm.growth_tiles.clear();
        }

        this->towns_growth_tiles_last_month.swap(this->towns_growth_tiles);
        this->towns_growth_tiles.clear();
    });

    this->events.listen<event::TownBuilt>(event::Slot::GAME, [] (const event::TownBuilt &event) {
        auto t = event.town;
        t->cm.hs_total = t->cm.hs_last_month = t->cm.hs_this_month = 0;
        t->cm.cs_total = t->cm.cs_last_month = t->cm.cs_this_month = 0;
        t->cm.hr_total = t->cm.hr_last_month = t->cm.hr_this_month = 0;
    });

    this->events.listen<event::TownGrowthSucceeded>(event::Slot::GAME, [this] (const event::TownGrowthSucceeded &event) {
        if (event.town->cache.num_houses <= event.prev_houses) {
            event.town->cm.hs_total++;
            event.town->cm.hs_this_month++;
            this->set_town_growth_tile(event.town, event.tile, TownGrowthTileState::HS);
        }
    });

    this->events.listen<event::TownGrowthFailed>(event::Slot::GAME, [this] (const event::TownGrowthFailed &event) {
        event.town->cm.cs_total++;
        event.town->cm.cs_this_month++;
        this->set_town_growth_tile(event.town, event.tile, TownGrowthTileState::CS);
    });

    this->events.listen<event::HouseRebuilt>(event::Slot::GAME, [this] (const event::HouseRebuilt &event) {
        if (event.was_successful) {
            event.town->cm.houses_reconstructed_this_month++;
            this->set_town_growth_tile(event.town, event.tile, TownGrowthTileState::RH_REBUILT);
        } else {
            event.town->cm.houses_demolished_this_month++;
            this->set_town_growth_tile(event.town, event.tile, TownGrowthTileState::RH_REMOVED);
        }
    });

    this->events.listen<event::HouseBuilt>(event::Slot::GAME, [this] (const event::HouseBuilt &event) {
        event.town->cm.houses_constructing++;
        event.town->cm.real_population += event.house_spec->population;
        this->set_town_growth_tile(event.town, event.tile, TownGrowthTileState::NEW_HOUSE);
    });

    this->events.listen<event::HouseCleared>(event::Slot::GAME, [] (const event::HouseCleared &event) {
        if (!event.was_completed)
            event.town->cm.houses_constructing--;
        event.town->cm.real_population -= event.house_spec->population;
    });

    this->events.listen<event::HouseCompleted>(event::Slot::GAME, [] (const event::HouseCompleted &event) {
        event.town->cm.houses_constructing--;
    });

    this->events.listen<event::HouseDestroyed>(event::Slot::GAME, [this] (const event::HouseDestroyed &event) {
        const Company *company = Company::GetIfValid(event.company_id);
        if (company && company->cm.is_server) {
            this->set_town_growth_tile(event.town, event.tile, TownGrowthTileState::HR);
            event.town->cm.hr_total++;
        }
    });

    this->events.listen<event::TownCachesRebuilt>(event::Slot::GAME, [this] (const event::TownCachesRebuilt&) {
        this->towns_growth_tiles.clear();
        this->towns_growth_tiles_last_month.clear();
        for (Town *town : Town::Iterate()) {
            town->cm.real_population = 0;
            town->cm.houses_constructing = 0;
            for (auto &[tile, state] : town->cm.growth_tiles) {
                if (this->towns_growth_tiles[tile] < state) this->towns_growth_tiles[tile] = state;
            }
            for (auto &[tile, state] : town->cm.growth_tiles_last_month) {
                if (this->towns_growth_tiles_last_month[tile] < state) this->towns_growth_tiles_last_month[tile] = state;
            }
        }
        for (auto t : Map::Iterate()) {
            if (!IsTileType(t, MP_HOUSE)) continue;
            Town *town = Town::GetByTile(t);
            if (!IsHouseCompleted(t))
                town->cm.houses_constructing++;
            HouseID house_id = GetHouseType(t);
            town->cm.real_population += HouseSpec::Get(house_id)->population;
        }
    });

    this->events.listen<event::CargoAccepted>(event::Slot::GAME, [] (const event::CargoAccepted &event) {
        event.company->cur_economy.cm.cargo_income[event.cargo_type] += event.profit;
    });
}

void Game::set_town_growth_tile(Town *town, TileIndex tile, TownGrowthTileState state) {
    if (this->towns_growth_tiles[tile] < state) this->towns_growth_tiles[tile] = state;
    if (town->cm.growth_tiles[tile] < state) town->cm.growth_tiles[tile] = state;
}

} // namespace citymania
