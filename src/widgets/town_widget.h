/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file town_widget.h Types related to the town widgets. */

#ifndef WIDGETS_TOWN_WIDGET_H
#define WIDGETS_TOWN_WIDGET_H

/** Widgets of the #TownDirectoryWindow class. */
enum TownDirectoryWidgets : WidgetID {
	WID_TD_CAPTION, ///< Caption of the window.
	WID_TD_SORT_ORDER,       ///< Direction of sort dropdown.
	WID_TD_SORT_CRITERIA,    ///< Criteria of sort dropdown.
	WID_TD_FILTER,           ///< Filter of name.
	WID_TD_LIST,             ///< List of towns.
	WID_TD_SCROLLBAR,        ///< Scrollbar for the town list.
	WID_TD_WORLD_POPULATION, ///< The world's population.
	TDW_CAPTION_TEXT,
};

/** Widgets of the #TownAuthorityWindow class. */
enum TownAuthorityWidgets : WidgetID {
	WID_TA_CAPTION,      ///< Caption of window.
	WID_TA_ZONE_BUTTON,  ///< Turn on/off showing local authority zone.
	WID_TA_RATING_INFO,  ///< Overview with ratings for each company.
	WID_TA_COMMAND_LIST, ///< List of commands for the player.
	WID_TA_SCROLLBAR,    ///< Scrollbar of the list of commands.
	WID_TA_ACTION_INFO,  ///< Additional information about the action.
	WID_TA_EXECUTE,      ///< Do-it button.
};

/** Widgets of the #TownViewWindow class. */
enum TownViewWidgets : WidgetID {
	WID_TV_CAPTION,        ///< Caption of window.
	WID_TV_VIEWPORT,       ///< View of the center of the town.
	WID_TV_INFO,           ///< General information about the town.
	WID_TV_CENTER_VIEW,    ///< Center the main view on this town.
	WID_TV_SHOW_AUTHORITY, ///< Show the town authority window.
	WID_TV_CHANGE_NAME,    ///< Change the name of this town.
	WID_TV_CATCHMENT,      ///< Toggle catchment area highlight.
	WID_TV_EXPAND,         ///< Expand this town (scenario editor only).
	WID_TV_DELETE,         ///< Delete this town (scenario editor only).
	WID_TV_CB,
};

/** Widgets of the #FoundTownWindow class. */
enum TownFoundingWidgets : WidgetID {
	WID_TF_NEW_TOWN,          ///< Create a new town.
	WID_TF_TOWN_ACTION_SEL, ///< Container of town action buttons.
	WID_TF_RANDOM_TOWN,       ///< Randomly place a town.
	WID_TF_MANY_RANDOM_TOWNS, ///< Randomly place many towns.
	WID_TF_LOAD_FROM_FILE,    ///< Load town data from file.
	WID_TF_EXPAND_ALL_TOWNS,  ///< Make all towns grow slightly.
	WID_TF_TOWN_NAME_EDITBOX, ///< Editor for the town name.
	WID_TF_TOWN_NAME_RANDOM,  ///< Generate a random town name.
	WID_TF_SIZE_SEL, ///< Container of town size buttons.
	WID_TF_SIZE_SMALL,        ///< Selection for a small town.
	WID_TF_SIZE_MEDIUM,       ///< Selection for a medium town.
	WID_TF_SIZE_LARGE,        ///< Selection for a large town.
	WID_TF_SIZE_RANDOM,       ///< Selection for a randomly sized town.
	WID_TF_CITY,              ///< Selection for the town's city state.
	WID_TF_ROAD_LAYOUT_SEL, ///< Container of town road layout buttons.
	WID_TF_LAYOUT_ORIGINAL,   ///< Selection for the original town layout.
	WID_TF_LAYOUT_BETTER,     ///< Selection for the better town layout.
	WID_TF_LAYOUT_GRID2,      ///< Selection for the 2x2 grid town layout.
	WID_TF_LAYOUT_GRID3,      ///< Selection for the 3x3 grid town layout.
	WID_TF_LAYOUT_RANDOM,     ///< Selection for a randomly chosen town layout.
};

/** Widgets of the #BuildHouseWindow class. */
enum BuildHouseWidgets : WidgetID {
	WID_BH_INFO, ///< Information panel of selected house.
	WID_BH_PROTECT_OFF, ///< Button to protect the next house built.
	WID_BH_PROTECT_ON, ///< Button to not protect the next house built.
};

// TODO move to cm folder
enum TownHK {
	HK_SADVERT,
	HK_MADVERT,
	HK_LADVERT,
	HK_ROADS,
	HK_STATUE,
	HK_FUND,
};

enum CBTownWidgets {
	WID_CB_CAPTION,        ///< Caption of window.
	WID_CB_CENTER_VIEW,
	WID_CB_ADVERT,
	WID_CB_ADVERT_REGULAR,
	WID_CB_FUND,
	WID_CB_FUND_REGULAR,
	WID_CB_POWERFUND,
	WID_CB_DETAILS,
	WID_CB_CARGO_NAME,
	WID_CB_CARGO_AMOUNT,
	WID_CB_CARGO_REQ,
	WID_CB_CARGO_PREVIOUS,
	WID_CB_CARGO_STORE,
	WID_CB_CARGO_STORE_PCT,
	WID_CB_CARGO_FROM,
	WID_CB_TOWN_VIEW,
	WID_CB_SHOW_AUTHORITY,
	WID_CB_SELECT_REQUIREMENTS,
	WID_CB_GOALS,
};

#endif /* WIDGETS_TOWN_WIDGET_H */
