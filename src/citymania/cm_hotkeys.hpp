#ifndef CMEXT_HOTKEYS_HPP
#define CMEXT_HOTKEYS_HPP

#include "../hotkeys.h"
#include "../order_type.h"
#include "../window_type.h"

struct Vehicle;

namespace citymania {

extern bool _fn_mod;
extern bool _estimate_mod;
extern bool _remove_mod;


enum class ToolRemoveMode : uint8 {
    NONE = 0,
    BUTTON = 1,
    MOD = 2,
};

void UpdateModKeys(bool shift_pressed, bool ctrl_pressed, bool alt_pressed, bool command_pressed);

bool RailToolbar_IsRemoveInverted(int widget);
void RailToolbar_UpdateRemoveWidgetStatus(Window *w, int widged, bool remove_active);
bool RailToolbar_RemoveModChanged(Window *w, bool invert_remove, bool remove_active, bool button_clicked);

ToolRemoveMode RoadToolbar_GetRemoveMode(int widget);
void RoadToolbar_UpdateOptionWidgetStatus(Window *w, int widget, bool remove_active, bool is_road);
bool RoadToolbar_RemoveModChanged(Window *w, bool remove_active, bool button_clicked, bool is_road);

void ResetEffectiveActionCounter();
std::pair<uint32, uint32> GetEPM();
bool ChooseSignalDragBehaviour();
void CountHotkeyStats(const HotkeyList *list, int hotkey);


enum class FeederOrderMod {
    None,
    Load,
    Unload,
};

enum class StationOrderModAction : uint8_t {
    None = 0,
    FullLoad,
    Transfer,
    UnloadAll,
    FeederLoad,
    FeederUnload,
    NoLoad,
    NoUnload,
};

enum class DepotOrderModAction : uint8_t {
    None = 0,
    Service,
    Stop,
    Unbunch,
};

struct StationModOrders {
    OrderLoadFlags load;
    OrderUnloadFlags unload;
    FeederOrderMod mod;
};

DepotOrderModAction GetDepotOrderModAction();
StationModOrders GetStationModOrders(const Vehicle *v);

} // namespace citymania

#endif
