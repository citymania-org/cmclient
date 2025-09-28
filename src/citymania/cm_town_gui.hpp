/** @file cm_town_gui.hpp CityMania town GUI. */

#ifndef CM_TOWN_GUI_HPP
#define CM_TOWN_GUI_HPP

#include "../town_type.h"  // Town
#include "../town.h"  // TownAction

namespace citymania {

enum TownWindowHotkeys : int32_t {
	CM_THK_SMALL_AD = 0x80,
	CM_THK_MEDIUM_AD,
	CM_THK_LARGE_AD,
	// CM_THK_ROAD_CONSTRUCTION,
	CM_THK_BUILD_STATUE,
	CM_THK_FUND_BUILDNGS,
};

bool TownExecuteAction(const Town *town, TownAction action);
void DrawExtraTownInfo(Rect &r, const Town *town, uint line, bool show_house_states_info);
void ShowCBTownWindow(TownID town);

} // namespace citymania

#endif /* CM_TOWN_GUI_HPP */
