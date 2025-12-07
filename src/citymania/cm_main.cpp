#include "../stdafx.h"

#include "cm_main.hpp"

#include "../safeguards.h"

namespace citymania {

up<Game> _game = nullptr;

static IntervalTimer<TimerGameCalendar> _event_new_month({TimerGameCalendar::MONTH, TimerGameCalendar::Priority::NONE}, [](auto) {
    Emit(event::NewMonth{});
});

void ResetGame() {
    _game = make_up<Game>();
}

void SwitchToMode(SwitchMode new_mode) {
    if (new_mode != SM_SAVE_GAME) ResetGame();
}

template <typename T>
void Emit(const T &event) {
    _game->events.emit<T>(event);
}

template void Emit<event::NewMonth>(const event::NewMonth &);
template void Emit<event::TownBuilt>(const event::TownBuilt &);
template void Emit<event::TownGrowthSucceeded>(const event::TownGrowthSucceeded &);
template void Emit<event::TownGrowthFailed>(const event::TownGrowthFailed &);
template void Emit<event::TownCachesRebuilt>(const event::TownCachesRebuilt &);
template void Emit<event::HouseRebuilt>(const event::HouseRebuilt &);
template void Emit<event::HouseBuilt>(const event::HouseBuilt &);
template void Emit<event::HouseCleared>(const event::HouseCleared &);
template void Emit<event::HouseDestroyed>(const event::HouseDestroyed &);
template void Emit<event::HouseCompleted>(const event::HouseCompleted &);
template void Emit<event::CompanyEvent>(const event::CompanyEvent &);
template void Emit<event::CargoDeliveredToIndustry>(const event::CargoDeliveredToIndustry &);
template void Emit<event::CargoDeliveredToUnknown>(const event::CargoDeliveredToUnknown &);
template void Emit<event::CargoAccepted>(const event::CargoAccepted &);
template void Emit<event::CompanyMoneyChanged>(const event::CompanyMoneyChanged &);
template void Emit<event::CompanyLoanChanged>(const event::CompanyLoanChanged &);
template void Emit<event::CompanyBalanceChanged>(const event::CompanyBalanceChanged &);
// template void Emit<event::>(const event:: &);

} // namespace citymania