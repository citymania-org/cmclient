#ifndef CITYMANIA_LOCATIONS_HPP
#define CITYMANIA_LOCATIONS_HPP

namespace citymania {

static const uint NUM_LOCATIONS = 9;

#define CM_MTHK_LOCATIONS_START 240
static const int CM_MTHK_LOCATIONS_GOTO_START = CM_MTHK_LOCATIONS_START;
static const int CM_MTHK_LOCATIONS_GOTO_END = CM_MTHK_LOCATIONS_GOTO_START + NUM_LOCATIONS;
static const int CM_MTHK_LOCATIONS_SET_START = CM_MTHK_LOCATIONS_GOTO_END;
static const int CM_MTHK_LOCATIONS_SET_END = CM_MTHK_LOCATIONS_SET_START + NUM_LOCATIONS;


// #define CM_MTHK_LOCATIONS_GOTO_START (CM_MTHK_LOCATIONS_START)
// #define CM_MTHK_LOCATIONS_GOTO_END (CM_MTHK_LOCATIONS_GOTO_START + NUM_LOCATIONS)
// #define CM_MTHK_LOCATIONS_SET_START (CM_MTHK_LOCATIONS_GOTO_END)
// #define CM_MTHK_LOCATIONS_SET_END (CM_MTHK_LOCATIONS_SET_START + NUM_LOCATIONS)


#define CM_LOCATION_HOTKEYS_DECL \
    Hotkey((uint16)0, "cm_goto_location_1", CM_MTHK_LOCATIONS_START), \
    Hotkey((uint16)0, "cm_goto_location_2", CM_MTHK_LOCATIONS_START + 1), \
    Hotkey((uint16)0, "cm_goto_location_3", CM_MTHK_LOCATIONS_START + 2), \
    Hotkey((uint16)0, "cm_goto_location_4", CM_MTHK_LOCATIONS_START + 3), \
    Hotkey((uint16)0, "cm_goto_location_5", CM_MTHK_LOCATIONS_START + 4), \
    Hotkey((uint16)0, "cm_goto_location_6", CM_MTHK_LOCATIONS_START + 5), \
    Hotkey((uint16)0, "cm_goto_location_7", CM_MTHK_LOCATIONS_START + 6), \
    Hotkey((uint16)0, "cm_goto_location_8", CM_MTHK_LOCATIONS_START + 7), \
    Hotkey((uint16)0, "cm_goto_location_9", CM_MTHK_LOCATIONS_START + 8), \
    Hotkey((uint16)0, "cm_set_location_1", CM_MTHK_LOCATIONS_START + 9), \
    Hotkey((uint16)0, "cm_set_location_2", CM_MTHK_LOCATIONS_START + 10), \
    Hotkey((uint16)0, "cm_set_location_3", CM_MTHK_LOCATIONS_START + 11), \
    Hotkey((uint16)0, "cm_set_location_4", CM_MTHK_LOCATIONS_START + 12), \
    Hotkey((uint16)0, "cm_set_location_5", CM_MTHK_LOCATIONS_START + 13), \
    Hotkey((uint16)0, "cm_set_location_6", CM_MTHK_LOCATIONS_START + 14), \
    Hotkey((uint16)0, "cm_set_location_7", CM_MTHK_LOCATIONS_START + 15), \
    Hotkey((uint16)0, "cm_set_location_8", CM_MTHK_LOCATIONS_START + 16), \
    Hotkey((uint16)0, "cm_set_location_9", CM_MTHK_LOCATIONS_START + 17)

EventState HandleToolbarHotkey(int hotkey);

} // namespace citymania

#endif  /* CITYMANIA_MINIMAP_HPP */
