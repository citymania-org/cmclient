#include "../stdafx.h"

#include "cm_main.hpp"
#include "cm_command_type.hpp"
#include "cm_hotkeys.hpp"
#include "cm_minimap.hpp"

#include "../network/network_func.h"
#include "../timer/timer.h"
#include "../window_func.h"

#include "../safeguards.h"

namespace citymania {

up<Game> _game = nullptr;

static IntervalTimer<TimerGameCalendar> _event_new_month({TimerGameCalendar::MONTH, TimerGameCalendar::Priority::NONE}, [](auto) {
    Emit(event::NewMonth{});
});

void ResetGame() {
    _game = make_up<Game>();
    ResetEffectiveActionCounter();
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

void ToggleSmallMap() {
    SmallMapWindow *w = dynamic_cast<citymania::SmallMapWindow*>(FindWindowById(WC_SMALLMAP, 0));
    if (w == nullptr) ShowSmallMap();
    else w->Close();
}

void NetworkClientSendChatToServer(const std::string &msg) {
    NetworkClientSendChat(NETWORK_ACTION_CHAT_CLIENT, DESTTYPE_CLIENT, CLIENT_ID_SERVER, msg);
}

} // namespace citymania
