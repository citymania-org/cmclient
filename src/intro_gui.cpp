/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file intro_gui.cpp The main menu GUI. */

#include "stdafx.h"
#include "error.h"
#include "gui.h"
#include "window_gui.h"
#include "window_func.h"
#include "textbuf_gui.h"
#include "help_gui.h"
#include "network/network.h"
#include "genworld.h"
#include "network/network_gui.h"
#include "network/network_content.h"
#include "network/network_survey.h"
#include "landscape_type.h"
#include "landscape.h"
#include "strings_func.h"
#include "fios.h"
#include "ai/ai_gui.hpp"
#include "game/game_gui.hpp"
#include "gfx_func.h"
#include "core/geometry_func.hpp"
#include "language.h"
#include "rev.h"
#include "highscore.h"
#include "signs_base.h"
#include "viewport_func.h"
#include "vehicle_base.h"
#include <regex>

#include "widgets/intro_widget.h"

#include "table/strings.h"
#include "table/sprites.h"

#include "citymania/cm_hotkeys.hpp"

#include "safeguards.h"


/**
 * A viewport command for the main menu background (intro game).
 */
struct IntroGameViewportCommand {
	/** Horizontal alignment value. */
	enum AlignmentH : uint8_t {
		LEFT,
		CENTRE,
		RIGHT,
	};
	/** Vertical alignment value. */
	enum AlignmentV : uint8_t {
		TOP,
		MIDDLE,
		BOTTOM,
	};

	int command_index = 0;               ///< Sequence number of the command (order they are performed in).
	Point position{ 0, 0 };              ///< Calculated world coordinate to position viewport top-left at.
	VehicleID vehicle = VehicleID::Invalid(); ///< Vehicle to follow, or VehicleID::Invalid() if not following a vehicle.
	uint delay = 0;                      ///< Delay until next command.
	int zoom_adjust = 0;                 ///< Adjustment to zoom level from base zoom level.
	bool pan_to_next = false;            ///< If true, do a smooth pan from this position to the next.
	AlignmentH align_h = CENTRE;         ///< Horizontal alignment.
	AlignmentV align_v = MIDDLE;         ///< Vertical alignment.

	/**
	 * Calculate effective position.
	 * This will update the position field if a vehicle is followed.
	 * @param vp Viewport to calculate position for.
	 * @return Calculated position in the viewport.
	 */
	Point PositionForViewport(const Viewport &vp)
	{
		if (this->vehicle != VehicleID::Invalid()) {
			const Vehicle *v = Vehicle::Get(this->vehicle);
			this->position = RemapCoords(v->x_pos, v->y_pos, v->z_pos);
		}

		Point p;
		switch (this->align_h) {
			case LEFT: p.x = this->position.x; break;
			case CENTRE: p.x = this->position.x - vp.virtual_width / 2; break;
			case RIGHT: p.x = this->position.x - vp.virtual_width; break;
		}
		switch (this->align_v) {
			case TOP: p.y = this->position.y; break;
			case MIDDLE: p.y = this->position.y - vp.virtual_height / 2; break;
			case BOTTOM: p.y = this->position.y - vp.virtual_height; break;
		}
		return p;
	}
};


struct SelectGameWindow : public Window {
	/** Vector of viewport commands parsed. */
	std::vector<IntroGameViewportCommand> intro_viewport_commands{};
	/** Index of currently active viewport command. */
	size_t cur_viewport_command_index = SIZE_MAX;
	/** Time spent (milliseconds) on current viewport command. */
	uint cur_viewport_command_time = 0;
	uint mouse_idle_time = 0;
	Point mouse_idle_pos{};

	/**
	 * Find and parse all viewport command signs.
	 * Fills the intro_viewport_commands vector and deletes parsed signs from the world.
	 */
	void ReadIntroGameViewportCommands()
	{
		intro_viewport_commands.clear();

		/* Regular expression matching the commands: T, spaces, integer, spaces, flags, spaces, integer */
		const char *sign_langauge = "^T\\s*([0-9]+)\\s*([-+A-Z0-9]+)\\s*([0-9]+)";
		std::regex re(sign_langauge, std::regex_constants::icase);

		/* List of signs successfully parsed to delete afterwards. */
		std::vector<SignID> signs_to_delete;

		for (const Sign *sign : Sign::Iterate()) {
			std::smatch match;
			if (std::regex_search(sign->name, match, re)) {
				IntroGameViewportCommand vc;
				/* Sequence index from the first matching group. */
				vc.command_index = std::stoi(match[1].str());
				/* Sign coordinates for positioning. */
				vc.position = RemapCoords(sign->x, sign->y, sign->z);
				/* Delay from the third matching group. */
				vc.delay = std::stoi(match[3].str()) * 1000; // milliseconds

				/* Parse flags from second matching group. */
				enum IdType : uint8_t {
					ID_NONE, ID_VEHICLE
				} id_type = ID_NONE;
				for (char c : match[2].str()) {
					if (isdigit(c)) {
						if (id_type == ID_VEHICLE) {
							vc.vehicle = static_cast<VehicleID>(vc.vehicle.base() * 10 + (c - '0'));
						}
					} else {
						id_type = ID_NONE;
						switch (toupper(c)) {
							case '-': vc.zoom_adjust = +1; break;
							case '+': vc.zoom_adjust = -1; break;
							case 'T': vc.align_v = IntroGameViewportCommand::TOP; break;
							case 'M': vc.align_v = IntroGameViewportCommand::MIDDLE; break;
							case 'B': vc.align_v = IntroGameViewportCommand::BOTTOM; break;
							case 'L': vc.align_h = IntroGameViewportCommand::LEFT; break;
							case 'C': vc.align_h = IntroGameViewportCommand::CENTRE; break;
							case 'R': vc.align_h = IntroGameViewportCommand::RIGHT; break;
							case 'P': vc.pan_to_next = true; break;
							case 'V': id_type = ID_VEHICLE; vc.vehicle = VehicleID::Begin(); break;
						}
					}
				}

				/* Successfully parsed, store. */
				intro_viewport_commands.push_back(vc);
				signs_to_delete.push_back(sign->index);
			}
		}

		/* Sort the commands by sequence index. */
		std::sort(intro_viewport_commands.begin(), intro_viewport_commands.end(), [](const IntroGameViewportCommand &a, const IntroGameViewportCommand &b) { return a.command_index < b.command_index; });

		/* Delete all the consumed signs, from last ID to first ID. */
		std::sort(signs_to_delete.begin(), signs_to_delete.end(), [](SignID a, SignID b) { return a > b; });
		for (SignID sign_id : signs_to_delete) {
			delete Sign::Get(sign_id);
		}
	}

	SelectGameWindow(WindowDesc &desc) : Window(desc), mouse_idle_pos(_cursor.pos)
	{
		this->CreateNestedTree();
		this->FinishInitNested(0);
		this->OnInvalidateData();

		this->ReadIntroGameViewportCommands();
	}

	void OnRealtimeTick(uint delta_ms) override
	{
		/* Move the main game viewport according to intro viewport commands. */

		if (intro_viewport_commands.empty()) return;

		bool suppress_panning = true;
		if (this->mouse_idle_pos.x != _cursor.pos.x || this->mouse_idle_pos.y != _cursor.pos.y) {
			this->mouse_idle_pos = _cursor.pos;
			this->mouse_idle_time = 2000;
		} else if (this->mouse_idle_time > delta_ms) {
			this->mouse_idle_time -= delta_ms;
		} else {
			this->mouse_idle_time = 0;
			suppress_panning = false;
		}

		/* Determine whether to move to the next command or stay at current. */
		bool changed_command = false;
		if (this->cur_viewport_command_index >= intro_viewport_commands.size()) {
			/* Reached last, rotate back to start of the list. */
			this->cur_viewport_command_index = 0;
			changed_command = true;
		} else {
			/* Check if current command has elapsed and switch to next. */
			this->cur_viewport_command_time += delta_ms;
			if (this->cur_viewport_command_time >= intro_viewport_commands[this->cur_viewport_command_index].delay) {
				this->cur_viewport_command_index = (this->cur_viewport_command_index + 1) % intro_viewport_commands.size();
				this->cur_viewport_command_time = 0;
				changed_command = true;
			}
		}

		IntroGameViewportCommand &vc = intro_viewport_commands[this->cur_viewport_command_index];
		Window *mw = GetMainWindow();

		/* Early exit if the current command hasn't elapsed and isn't animated. */
		if (!changed_command && !vc.pan_to_next && vc.vehicle == VehicleID::Invalid()) return;

		/* Suppress panning commands, while user interacts with GUIs. */
		if (!changed_command && suppress_panning) return;

		/* Reset the zoom level. */
		if (changed_command) FixTitleGameZoom(vc.zoom_adjust);

		/* Calculate current command position (updates followed vehicle coordinates). */
		Point pos = vc.PositionForViewport(*mw->viewport);

		/* Calculate panning (linear interpolation between current and next command position). */
		if (vc.pan_to_next) {
			size_t next_command_index = (this->cur_viewport_command_index + 1) % intro_viewport_commands.size();
			IntroGameViewportCommand &nvc = intro_viewport_commands[next_command_index];
			Point pos2 = nvc.PositionForViewport(*mw->viewport);
			const double t = this->cur_viewport_command_time / (double)vc.delay;
			pos.x = pos.x + (int)(t * (pos2.x - pos.x));
			pos.y = pos.y + (int)(t * (pos2.y - pos.y));
		}

		/* Update the viewport position. */
		mw->viewport->dest_scrollpos_x = mw->viewport->scrollpos_x = pos.x;
		mw->viewport->dest_scrollpos_y = mw->viewport->scrollpos_y = pos.y;
		UpdateViewportPosition(mw, delta_ms);
		mw->SetDirty(); // Required during panning, otherwise logo graphics disappears

		/* If there is only one command, we just executed it and don't need to do any more */
		if (intro_viewport_commands.size() == 1 && vc.vehicle == VehicleID::Invalid()) intro_viewport_commands.clear();
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;
		this->SetWidgetLoweredState(WID_SGI_TEMPERATE_LANDSCAPE, _settings_newgame.game_creation.landscape == LandscapeType::Temperate);
		this->SetWidgetLoweredState(WID_SGI_ARCTIC_LANDSCAPE,    _settings_newgame.game_creation.landscape == LandscapeType::Arctic);
		this->SetWidgetLoweredState(WID_SGI_TROPIC_LANDSCAPE,    _settings_newgame.game_creation.landscape == LandscapeType::Tropic);
		this->SetWidgetLoweredState(WID_SGI_TOYLAND_LANDSCAPE,   _settings_newgame.game_creation.landscape == LandscapeType::Toyland);
	}

	void OnInit() override
	{
		bool missing_sprites = _missing_extra_graphics > 0 && !IsReleasedVersion();
		this->GetWidget<NWidgetStacked>(WID_SGI_BASESET_SELECTION)->SetDisplayedPlane(missing_sprites ? 0 : SZSP_NONE);

		bool missing_lang = _current_language->missing >= _settings_client.gui.missing_strings_threshold && !IsReleasedVersion();
		this->GetWidget<NWidgetStacked>(WID_SGI_TRANSLATION_SELECTION)->SetDisplayedPlane(missing_lang ? 0 : SZSP_NONE);
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_SGI_BASESET:
				DrawStringMultiLine(r, GetString(STR_INTRO_BASESET, _missing_extra_graphics), TC_FROMSTRING, SA_CENTER);
				break;

			case WID_SGI_TRANSLATION:
				DrawStringMultiLine(r, GetString(STR_INTRO_TRANSLATION, _current_language->missing), TC_FROMSTRING, SA_CENTER);
				break;
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_SGI_TEMPERATE_LANDSCAPE: case WID_SGI_ARCTIC_LANDSCAPE:
			case WID_SGI_TROPIC_LANDSCAPE: case WID_SGI_TOYLAND_LANDSCAPE:
				size.width += WidgetDimensions::scaled.fullbevel.Horizontal();
				size.height += WidgetDimensions::scaled.fullbevel.Vertical();
				break;
		}
	}

	void OnResize() override
	{
		bool changed = false;

		if (NWidgetResizeBase *wid = this->GetWidget<NWidgetResizeBase>(WID_SGI_BASESET); wid != nullptr && wid->current_x > 0) {
			changed |= wid->UpdateMultilineWidgetSize(GetString(STR_INTRO_BASESET, _missing_extra_graphics), 3);
		}

		if (NWidgetResizeBase *wid = this->GetWidget<NWidgetResizeBase>(WID_SGI_TRANSLATION); wid != nullptr && wid->current_x > 0) {
			changed |= wid->UpdateMultilineWidgetSize(GetString(STR_INTRO_TRANSLATION, _current_language->missing), 3);
		}

		if (changed) this->ReInit(0, 0, this->flags.Test(WindowFlag::Centred));
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		/* Do not create a network server when you (just) have closed one of the game
		 * creation/load windows for the network server. */
		if (IsInsideMM(widget, WID_SGI_GENERATE_GAME, WID_SGI_EDIT_SCENARIO + 1)) _is_network_server = false;

		switch (widget) {
			case WID_SGI_GENERATE_GAME:
				if (citymania::_fn_mod) {
					StartNewGameWithoutGUI(GENERATE_NEW_SEED);
				} else {
					ShowGenerateLandscape();
				}
				break;

			case WID_SGI_LOAD_GAME:      ShowSaveLoadDialog(FT_SAVEGAME, SLO_LOAD); break;
			case WID_SGI_PLAY_SCENARIO:  ShowSaveLoadDialog(FT_SCENARIO, SLO_LOAD); break;
			case WID_SGI_PLAY_HEIGHTMAP: ShowSaveLoadDialog(FT_HEIGHTMAP,SLO_LOAD); break;
			case WID_SGI_EDIT_SCENARIO:  StartScenarioEditor(); break;

			case WID_SGI_PLAY_NETWORK:
				if (!_network_available) {
					ShowErrorMessage(GetEncodedString(STR_NETWORK_ERROR_NOTAVAILABLE), {}, WL_ERROR);
				} else {
					ShowNetworkGameWindow();
				}
				break;

			case WID_SGI_TEMPERATE_LANDSCAPE: case WID_SGI_ARCTIC_LANDSCAPE:
			case WID_SGI_TROPIC_LANDSCAPE: case WID_SGI_TOYLAND_LANDSCAPE:
				SetNewLandscapeType(LandscapeType(widget - WID_SGI_TEMPERATE_LANDSCAPE));
				break;

			case WID_SGI_OPTIONS:         ShowGameOptions(); break;
			case WID_SGI_HIGHSCORE:       ShowHighscoreTable(); break;
			case WID_SGI_HELP:            ShowHelpWindow(); break;
			case WID_SGI_SETTINGS_OPTIONS:ShowGameSettings(); break;
			case WID_SGI_GRF_SETTINGS:    ShowNewGRFSettings(true, true, false, _grfconfig_newgame); break;
			case WID_SGI_CONTENT_DOWNLOAD:
				if (!_network_available) {
					ShowErrorMessage(GetEncodedString(STR_NETWORK_ERROR_NOTAVAILABLE), {}, WL_ERROR);
				} else {
					ShowNetworkContentListWindow();
				}
				break;
			case WID_SGI_AI_SETTINGS:     ShowAIConfigWindow(); break;
			case WID_SGI_GS_SETTINGS:     ShowGSConfigWindow(); break;
			case WID_SGI_EXIT:            HandleExitGameRequest(); break;
		}
	}
};

static constexpr NWidgetPart _nested_select_game_widgets[] = {
	NWidget(WWT_CAPTION, COLOUR_BROWN), SetStringTip(STR_INTRO_CAPTION),
	NWidget(WWT_PANEL, COLOUR_BROWN),
		NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_wide, 0), SetPadding(WidgetDimensions::unscaled.sparse),

			NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_sparse, 0),
				/* 'New Game' and 'Load Game' buttons */
				NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
					NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, WID_SGI_GENERATE_GAME), SetStringTip(STR_INTRO_NEW_GAME, STR_INTRO_TOOLTIP_NEW_GAME), SetFill(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, WID_SGI_LOAD_GAME), SetStringTip(STR_INTRO_LOAD_GAME, STR_INTRO_TOOLTIP_LOAD_GAME), SetFill(1, 0),
				EndContainer(),

				/* 'Play Scenario' and 'Play Heightmap' buttons */
				NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
					NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, WID_SGI_PLAY_SCENARIO), SetStringTip(STR_INTRO_PLAY_SCENARIO, STR_INTRO_TOOLTIP_PLAY_SCENARIO), SetFill(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, WID_SGI_PLAY_HEIGHTMAP), SetStringTip(STR_INTRO_PLAY_HEIGHTMAP, STR_INTRO_TOOLTIP_PLAY_HEIGHTMAP), SetFill(1, 0),
				EndContainer(),

				/* 'Scenario Editor' and 'Multiplayer' buttons */
				NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
					NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, WID_SGI_EDIT_SCENARIO), SetStringTip(STR_INTRO_SCENARIO_EDITOR, STR_INTRO_TOOLTIP_SCENARIO_EDITOR), SetFill(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, WID_SGI_PLAY_NETWORK), SetStringTip(STR_INTRO_MULTIPLAYER, STR_INTRO_TOOLTIP_MULTIPLAYER), SetFill(1, 0),
				EndContainer(),
			EndContainer(),

			/* Climate selection buttons */
			NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0), SetPIPRatio(1, 1, 1),
				NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, WID_SGI_TEMPERATE_LANDSCAPE), SetSpriteTip(SPR_SELECT_TEMPERATE, STR_INTRO_TOOLTIP_TEMPERATE),
				NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, WID_SGI_ARCTIC_LANDSCAPE), SetSpriteTip(SPR_SELECT_SUB_ARCTIC, STR_INTRO_TOOLTIP_SUB_ARCTIC_LANDSCAPE),
				NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, WID_SGI_TROPIC_LANDSCAPE), SetSpriteTip(SPR_SELECT_SUB_TROPICAL, STR_INTRO_TOOLTIP_SUB_TROPICAL_LANDSCAPE),
				NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, WID_SGI_TOYLAND_LANDSCAPE), SetSpriteTip(SPR_SELECT_TOYLAND, STR_INTRO_TOOLTIP_TOYLAND_LANDSCAPE),
			EndContainer(),

			NWidget(NWID_SELECTION, INVALID_COLOUR, WID_SGI_BASESET_SELECTION),
				NWidget(NWID_VERTICAL),
					NWidget(WWT_EMPTY, INVALID_COLOUR, WID_SGI_BASESET), SetFill(1, 0),
				EndContainer(),
			EndContainer(),

			NWidget(NWID_SELECTION, INVALID_COLOUR, WID_SGI_TRANSLATION_SELECTION),
				NWidget(NWID_VERTICAL),
					NWidget(WWT_EMPTY, INVALID_COLOUR, WID_SGI_TRANSLATION), SetFill(1, 0),
				EndContainer(),
			EndContainer(),

			NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_sparse, 0),
				/* 'Game Options' and 'Settings' buttons */
				NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
					NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, WID_SGI_OPTIONS), SetStringTip(STR_INTRO_GAME_OPTIONS, STR_INTRO_TOOLTIP_GAME_OPTIONS), SetFill(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, WID_SGI_SETTINGS_OPTIONS), SetStringTip(STR_INTRO_CONFIG_SETTINGS_TREE, STR_INTRO_TOOLTIP_CONFIG_SETTINGS_TREE), SetFill(1, 0),
				EndContainer(),

				/* 'AI Settings' and 'Game Script Settings' buttons */
				NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
					NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, WID_SGI_AI_SETTINGS), SetStringTip(STR_INTRO_AI_SETTINGS, STR_INTRO_TOOLTIP_AI_SETTINGS), SetFill(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, WID_SGI_GS_SETTINGS), SetStringTip(STR_INTRO_GAMESCRIPT_SETTINGS, STR_INTRO_TOOLTIP_GAMESCRIPT_SETTINGS), SetFill(1, 0),
				EndContainer(),

				/* 'Check Online Content' and 'NewGRF Settings' buttons */
				NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
					NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, WID_SGI_CONTENT_DOWNLOAD), SetStringTip(STR_INTRO_ONLINE_CONTENT, STR_INTRO_TOOLTIP_ONLINE_CONTENT), SetFill(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, WID_SGI_GRF_SETTINGS), SetStringTip(STR_INTRO_NEWGRF_SETTINGS, STR_INTRO_TOOLTIP_NEWGRF_SETTINGS), SetFill(1, 0),
				EndContainer(),
			EndContainer(),

			/* 'Help and Manuals' and 'Highscore Table' buttons */
			NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
				NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, WID_SGI_HELP), SetStringTip(STR_INTRO_HELP, STR_INTRO_TOOLTIP_HELP), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, WID_SGI_HIGHSCORE), SetStringTip(STR_INTRO_HIGHSCORE, STR_INTRO_TOOLTIP_HIGHSCORE), SetFill(1, 0),
			EndContainer(),

			/* 'Exit' button */
			NWidget(NWID_HORIZONTAL), SetPIPRatio(1, 0, 1),
				NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, WID_SGI_EXIT), SetMinimalSize(128, 0), SetStringTip(STR_INTRO_QUIT, STR_INTRO_TOOLTIP_QUIT),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _select_game_desc(
	WDP_CENTER, nullptr, 0, 0,
	WC_SELECT_GAME, WC_NONE,
	WindowDefaultFlag::NoClose,
	_nested_select_game_widgets
);

void ShowSelectGameWindow()
{
	new SelectGameWindow(_select_game_desc);
}

static void AskExitGameCallback(Window *, bool confirmed)
{
	if (confirmed) {
		_survey.Transmit(NetworkSurveyHandler::Reason::EXIT, true);
		_exit_game = true;
	}
}

void AskExitGame()
{
	ShowQuery(
		GetEncodedString(STR_QUIT_CAPTION),
		GetEncodedString(STR_QUIT_ARE_YOU_SURE_YOU_WANT_TO_EXIT_OPENTTD),
		nullptr,
		AskExitGameCallback,
		true
	);
}


static void AskExitToGameMenuCallback(Window *, bool confirmed)
{
	if (confirmed) {
		_switch_mode = SM_MENU;
		ClearErrorMessages();
	}
}

void AskExitToGameMenu()
{
	ShowQuery(
		GetEncodedString(STR_ABANDON_GAME_CAPTION),
		GetEncodedString((_game_mode != GM_EDITOR) ? STR_ABANDON_GAME_QUERY : STR_ABANDON_SCENARIO_QUERY),
		nullptr,
		AskExitToGameMenuCallback,
		true
	);
}
