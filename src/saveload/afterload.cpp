/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file afterload.cpp Code updating data after game load */

#include "../stdafx.h"
#include "../void_map.h"
#include "../signs_base.h"
#include "../depot_base.h"
#include "../fios.h"
#include "../gamelog_internal.h"
#include "../network/network.h"
#include "../network/network_func.h"
#include "../gfxinit.h"
#include "../viewport_func.h"
#include "../viewport_kdtree.h"
#include "../industry.h"
#include "../clear_map.h"
#include "../vehicle_func.h"
#include "../string_func.h"
#include "../strings_func.h"
#include "../window_func.h"
#include "../roadveh.h"
#include "../roadveh_cmd.h"
#include "../train.h"
#include "../station_base.h"
#include "../waypoint_base.h"
#include "../roadstop_base.h"
#include "../tunnelbridge_map.h"
#include "../pathfinder/yapf/yapf_cache.h"
#include "../elrail_func.h"
#include "../signs_func.h"
#include "../aircraft.h"
#include "../object_map.h"
#include "../object_base.h"
#include "../tree_map.h"
#include "../company_func.h"
#include "../road_cmd.h"
#include "../ai/ai.hpp"
#include "../script/script_gui.h"
#include "../game/game.hpp"
#include "../town.h"
#include "../economy_base.h"
#include "../animated_tile_map.h"
#include "../animated_tile_func.h"
#include "../subsidy_base.h"
#include "../subsidy_func.h"
#include "../newgrf.h"
#include "../newgrf_station.h"
#include "../engine_func.h"
#include "../rail_gui.h"
#include "../core/backup_type.hpp"
#include "../smallmap_gui.h"
#include "../news_func.h"
#include "../order_backup.h"
#include "../error.h"
#include "../disaster_vehicle.h"
#include "../ship.h"
#include "../water.h"
#include "../timer/timer.h"
#include "../timer/timer_game_calendar.h"
#include "../timer/timer_game_economy.h"
#include "../timer/timer_game_tick.h"
#include "../picker_func.h"

#include "../story_base.h"
#include "../game/game_text.hpp"
#include "../table/control_codes.h"

#include "../citymania/cm_highlight.hpp"
#include "../citymania/cm_minimap.hpp"

#include "saveload_internal.h"

#include <signal.h>

#include "table/strings.h"

#include "../safeguards.h"

extern Company *DoStartupNewCompany(bool is_ai, CompanyID company = CompanyID::Invalid());

/**
 * Makes a tile canal or water depending on the surroundings.
 *
 * Must only be used for converting old savegames. Use WaterClass now.
 *
 * This as for example docks and shipdepots do not store
 * whether the tile used to be canal or 'normal' water.
 * @param t the tile to change.
 * @param include_invalid_water_class Also consider WATER_CLASS_INVALID, i.e. industry tiles on land
 */
void SetWaterClassDependingOnSurroundings(Tile t, bool include_invalid_water_class)
{
	/* If the slope is not flat, we always assume 'land' (if allowed). Also for one-corner-raised-shores.
	 * Note: Wrt. autosloping under industry tiles this is the most fool-proof behaviour. */
	if (!IsTileFlat(t)) {
		if (include_invalid_water_class) {
			SetWaterClass(t, WATER_CLASS_INVALID);
			return;
		} else {
			SlErrorCorrupt("Invalid water class for dry tile");
		}
	}

	/* Mark tile dirty in all cases */
	MarkTileDirtyByTile(t);

	if (TileX(t) == 0 || TileY(t) == 0 || TileX(t) == Map::MaxX() - 1 || TileY(t) == Map::MaxY() - 1) {
		/* tiles at map borders are always WATER_CLASS_SEA */
		SetWaterClass(t, WATER_CLASS_SEA);
		return;
	}

	bool has_water = false;
	bool has_canal = false;
	bool has_river = false;

	for (DiagDirection dir = DIAGDIR_BEGIN; dir < DIAGDIR_END; dir++) {
		Tile neighbour = TileAddByDiagDir(t, dir);
		switch (GetTileType(neighbour)) {
			case MP_WATER:
				/* clear water and shipdepots have already a WaterClass associated */
				if (IsCoast(neighbour)) {
					has_water = true;
				} else if (!IsLock(neighbour)) {
					switch (GetWaterClass(neighbour)) {
						case WATER_CLASS_SEA:   has_water = true; break;
						case WATER_CLASS_CANAL: has_canal = true; break;
						case WATER_CLASS_RIVER: has_river = true; break;
						default: SlErrorCorrupt("Invalid water class for tile");
					}
				}
				break;

			case MP_RAILWAY:
				/* Shore or flooded halftile */
				has_water |= (GetRailGroundType(neighbour) == RAIL_GROUND_WATER);
				break;

			case MP_TREES:
				/* trees on shore */
				has_water |= (GB(neighbour.m2(), 4, 2) == TREE_GROUND_SHORE);
				break;

			default: break;
		}
	}

	if (!has_water && !has_canal && !has_river && include_invalid_water_class) {
		SetWaterClass(t, WATER_CLASS_INVALID);
		return;
	}

	if (has_river && !has_canal) {
		SetWaterClass(t, WATER_CLASS_RIVER);
	} else if (has_canal || !has_water) {
		SetWaterClass(t, WATER_CLASS_CANAL);
	} else {
		SetWaterClass(t, WATER_CLASS_SEA);
	}
}

static void ConvertTownOwner()
{
	for (auto tile : Map::Iterate()) {
		switch (GetTileType(tile)) {
			case MP_ROAD:
				if (GB(tile.m5(), 4, 2) == ROAD_TILE_CROSSING && HasBit(tile.m3(), 7)) {
					tile.m3() = OWNER_TOWN.base();
				}
				[[fallthrough]];

			case MP_TUNNELBRIDGE:
				if (tile.m1() & 0x80) SetTileOwner(tile, OWNER_TOWN);
				break;

			default: break;
		}
	}
}

/* since savegame version 4.1, exclusive transport rights are stored at towns */
static void UpdateExclusiveRights()
{
	for (Town *t : Town::Iterate()) {
		t->exclusivity = CompanyID::Invalid();
	}
}

static const uint8_t convert_currency[] = {
	 0,  1, 12,  8,  3,
	10, 14, 19,  4,  5,
	 9, 11, 13,  6, 17,
	16, 22, 21,  7, 15,
	18,  2, 20,
};

/* since savegame version 4.2 the currencies are arranged differently */
static void UpdateCurrencies()
{
	_settings_game.locale.currency = convert_currency[_settings_game.locale.currency];
}

/* Up to revision 1413 the invisible tiles at the southern border have not been
 * MP_VOID, even though they should have. This is fixed by this function
 */
static void UpdateVoidTiles()
{
	for (uint x = 0; x < Map::SizeX(); x++) MakeVoid(TileXY(x, Map::MaxY()));
	for (uint y = 0; y < Map::SizeY(); y++) MakeVoid(TileXY(Map::MaxX(), y));
}

static inline RailType UpdateRailType(RailType rt, RailType min)
{
	return rt >= min ? (RailType)(rt + 1): rt;
}

/**
 * Update the viewport coordinates of all signs.
 */
void UpdateAllVirtCoords()
{
	UpdateAllStationVirtCoords();
	UpdateAllSignVirtCoords();
	UpdateAllTownVirtCoords();
	UpdateAllTextEffectVirtCoords();
	RebuildViewportKdtree();
}

void ClearAllCachedNames()
{
	ClearAllStationCachedNames();
	ClearAllTownCachedNames();
	ClearAllIndustryCachedNames();
}

/**
 * Initialization of the windows and several kinds of caches.
 * This is not done directly in AfterLoadGame because these
 * functions require that all saveload conversions have been
 * done. As people tend to add savegame conversion stuff after
 * the initialization of the windows and caches quite some bugs
 * had been made.
 * Moving this out of there is both cleaner and less bug-prone.
 */
static void InitializeWindowsAndCaches()
{
	/* Initialize windows */
	ResetWindowSystem();
	SetupColoursAndInitialWindow();

	/* Update coordinates of the signs. */
	ClearAllCachedNames();
	UpdateAllVirtCoords();
	ResetViewportAfterLoadGame();

	for (Company *c : Company::Iterate()) {
		/* For each company, verify (while loading a scenario) that the inauguration date is the current year and set it
		 * accordingly if it is not the case.  No need to set it on companies that are not been used already,
		 * thus the MIN_YEAR (which is really nothing more than Zero, initialized value) test */
		if (_file_to_saveload.abstract_ftype == FT_SCENARIO && c->inaugurated_year != EconomyTime::MIN_YEAR) {
			c->inaugurated_year = TimerGameEconomy::year;
		}
	}

	/* Count number of objects per type */
	for (Object *o : Object::Iterate()) {
		Object::IncTypeCount(o->type);
	}

	/* Identify owners of persistent storage arrays */
	for (Industry *i : Industry::Iterate()) {
		if (i->psa != nullptr) {
			i->psa->feature = GSF_INDUSTRIES;
			i->psa->tile = i->location.tile;
		}
	}
	for (Station *s : Station::Iterate()) {
		if (s->airport.psa != nullptr) {
			s->airport.psa->feature = GSF_AIRPORTS;
			s->airport.psa->tile = s->airport.tile;
		}
	}
	for (Town *t : Town::Iterate()) {
		for (auto &it : t->psa_list) {
			it->feature = GSF_FAKE_TOWNS;
			it->tile = t->xy;
		}
	}
	for (RoadVehicle *rv : RoadVehicle::Iterate()) {
		if (rv->IsFrontEngine()) {
			rv->CargoChanged();
		}
	}

	RecomputePrices();

	GroupStatistics::UpdateAfterLoad();

	RebuildSubsidisedSourceAndDestinationCache();

	/* Towns have a noise controlled number of airports system
	 * So each airport's noise value must be added to the town->noise_reached value
	 * Reset each town's noise_reached value to '0' before. */
	UpdateAirportsNoise();

	CheckTrainsLengths();
	ShowNewGRFError();

	/* Rebuild the smallmap list of owners. */
	BuildOwnerLegend();
}

typedef void (CDECL *SignalHandlerPointer)(int);
static SignalHandlerPointer _prev_segfault = nullptr;
static SignalHandlerPointer _prev_abort    = nullptr;
static SignalHandlerPointer _prev_fpe      = nullptr;

static void CDECL HandleSavegameLoadCrash(int signum);

/**
 * Replaces signal handlers of SIGSEGV and SIGABRT
 * and stores pointers to original handlers in memory.
 */
static void SetSignalHandlers()
{
	_prev_segfault = signal(SIGSEGV, HandleSavegameLoadCrash);
	_prev_abort    = signal(SIGABRT, HandleSavegameLoadCrash);
	_prev_fpe      = signal(SIGFPE,  HandleSavegameLoadCrash);
}

/**
 * Resets signal handlers back to original handlers.
 */
static void ResetSignalHandlers()
{
	signal(SIGSEGV, _prev_segfault);
	signal(SIGABRT, _prev_abort);
	signal(SIGFPE,  _prev_fpe);
}

/** Was the saveload crash because of missing NewGRFs? */
static bool _saveload_crash_with_missing_newgrfs = false;

/**
 * Did loading the savegame cause a crash? If so,
 * were NewGRFs missing?
 * @return when the saveload crashed due to missing NewGRFs.
 */
bool SaveloadCrashWithMissingNewGRFs()
{
	return _saveload_crash_with_missing_newgrfs;
}

/**
 * Signal handler used to give a user a more useful report for crashes during
 * the savegame loading process; especially when there's problems with the
 * NewGRFs that are required by the savegame.
 * @param signum received signal
 */
static void CDECL HandleSavegameLoadCrash(int signum)
{
	ResetSignalHandlers();

	std::string message;
	message.reserve(1024);
	message += "Loading your savegame caused OpenTTD to crash.\n";

	_saveload_crash_with_missing_newgrfs = std::ranges::any_of(_grfconfig, [](const auto &c) { return c->flags.Test(GRFConfigFlag::Compatible) || c->status == GCS_NOT_FOUND; });

	if (_saveload_crash_with_missing_newgrfs) {
		message +=
			"This is most likely caused by a missing NewGRF or a NewGRF that\n"
			"has been loaded as replacement for a missing NewGRF. OpenTTD\n"
			"cannot easily determine whether a replacement NewGRF is of a newer\n"
			"or older version.\n"
			"It will load a NewGRF with the same GRF ID as the missing NewGRF.\n"
			"This means that if the author makes incompatible NewGRFs with the\n"
			"same GRF ID, OpenTTD cannot magically do the right thing. In most\n"
			"cases, OpenTTD will load the savegame and not crash, but this is an\n"
			"exception.\n"
			"Please load the savegame with the appropriate NewGRFs installed.\n"
			"The missing/compatible NewGRFs are:\n";

		for (const auto &c : _grfconfig) {
			if (c->flags.Test(GRFConfigFlag::Compatible)) {
				const GRFIdentifier &replaced = _gamelog.GetOverriddenIdentifier(*c);
				fmt::format_to(std::back_inserter(message), "NewGRF {:08X} (checksum {}) not found.\n  Loaded NewGRF \"{}\" (checksum {}) with same GRF ID instead.\n",
						std::byteswap(c->ident.grfid), FormatArrayAsHex(c->original_md5sum), c->filename, FormatArrayAsHex(replaced.md5sum));
			}
			if (c->status == GCS_NOT_FOUND) {
				fmt::format_to(std::back_inserter(message), "NewGRF {:08X} ({}) not found; checksum {}.\n",
						std::byteswap(c->ident.grfid), c->filename, FormatArrayAsHex(c->ident.md5sum));
			}
		}
	} else {
		message +=
			"This is probably caused by a corruption in the savegame.\n"
			"Please file a bug report and attach this savegame.\n";
	}

	ShowInfoI(message);

	SignalHandlerPointer call = nullptr;
	switch (signum) {
		case SIGSEGV: call = _prev_segfault; break;
		case SIGABRT: call = _prev_abort; break;
		case SIGFPE:  call = _prev_fpe; break;
		default: NOT_REACHED();
	}
	if (call != nullptr) call(signum);
}

/**
 * Tries to change owner of this rail tile to a valid owner. In very old versions it could happen that
 * a rail track had an invalid owner. When conversion isn't possible, track is removed.
 * @param t tile to update
 */
static void FixOwnerOfRailTrack(Tile t)
{
	assert(!Company::IsValidID(GetTileOwner(t)) && (IsLevelCrossingTile(t) || IsPlainRailTile(t)));

	/* remove leftover rail piece from crossing (from very old savegames) */
	Train *v = nullptr;
	for (Train *w : Train::Iterate()) {
		if (w->tile == TileIndex(t)) {
			v = w;
			break;
		}
	}

	if (v != nullptr) {
		/* when there is a train on crossing (it could happen in TTD), set owner of crossing to train owner */
		SetTileOwner(t, v->owner);
		return;
	}

	/* try to find any connected rail */
	for (DiagDirection dd = DIAGDIR_BEGIN; dd < DIAGDIR_END; dd++) {
		TileIndex tt{t + TileOffsByDiagDir(dd)};
		if (GetTileTrackStatus(t, TRANSPORT_RAIL, 0, dd) != 0 &&
				GetTileTrackStatus(tt, TRANSPORT_RAIL, 0, ReverseDiagDir(dd)) != 0 &&
				Company::IsValidID(GetTileOwner(tt))) {
			SetTileOwner(t, GetTileOwner(tt));
			return;
		}
	}

	if (IsLevelCrossingTile(t)) {
		/* else change the crossing to normal road (road vehicles won't care) */
		Owner road = GetRoadOwner(t, RTT_ROAD);
		Owner tram = GetRoadOwner(t, RTT_TRAM);
		RoadBits bits = GetCrossingRoadBits(t);
		bool hasroad = HasBit(t.m7(), 6);
		bool hastram = HasBit(t.m7(), 7);

		/* MakeRoadNormal */
		SetTileType(t, MP_ROAD);
		SetTileOwner(t, road);
		t.m3() = (hasroad ? bits : 0);
		t.m5() = (hastram ? bits : 0) | ROAD_TILE_NORMAL << 6;
		SB(t.m6(), 2, 4, 0);
		SetRoadOwner(t, RTT_TRAM, tram);
		return;
	}

	/* if it's not a crossing, make it clean land */
	MakeClear(t, CLEAR_GRASS, 0);
}

/**
 * Fixes inclination of a vehicle. Older OpenTTD versions didn't update the bits correctly.
 * @param v vehicle
 * @param dir vehicle's direction, or # INVALID_DIR if it can be ignored
 * @return inclination bits to set
 */
static uint FixVehicleInclination(Vehicle *v, Direction dir)
{
	/* Compute place where this vehicle entered the tile */
	int entry_x = v->x_pos;
	int entry_y = v->y_pos;
	switch (dir) {
		case DIR_NE: entry_x |= TILE_UNIT_MASK; break;
		case DIR_NW: entry_y |= TILE_UNIT_MASK; break;
		case DIR_SW: entry_x &= ~TILE_UNIT_MASK; break;
		case DIR_SE: entry_y &= ~TILE_UNIT_MASK; break;
		case INVALID_DIR: break;
		default: NOT_REACHED();
	}
	uint8_t entry_z = GetSlopePixelZ(entry_x, entry_y, true);

	/* Compute middle of the tile. */
	int middle_x = (v->x_pos & ~TILE_UNIT_MASK) + TILE_SIZE / 2;
	int middle_y = (v->y_pos & ~TILE_UNIT_MASK) + TILE_SIZE / 2;
	uint8_t middle_z = GetSlopePixelZ(middle_x, middle_y, true);

	/* middle_z == entry_z, no height change. */
	if (middle_z == entry_z) return 0;

	/* middle_z < entry_z, we are going downwards. */
	if (middle_z < entry_z) return 1U << GVF_GOINGDOWN_BIT;

	/* middle_z > entry_z, we are going upwards. */
	return 1U << GVF_GOINGUP_BIT;
}

/**
 * Check whether the ground vehicles are at the correct Z-coordinate. When they
 * are not, this will cause all kinds of problems later on as the vehicle might
 * not get onto bridges and so on.
 */
static void CheckGroundVehiclesAtCorrectZ()
{
	for (Vehicle *v : Vehicle::Iterate()) {
		if (v->IsGroundVehicle()) {
			/*
			 * Either the vehicle is not actually on the given tile, i.e. it is
			 * in the wormhole of a bridge or a tunnel, or the Z-coordinate must
			 * be the same as when it would be recalculated right now.
			 */
			assert(v->tile != TileVirtXY(v->x_pos, v->y_pos) || v->z_pos == GetSlopePixelZ(v->x_pos, v->y_pos, true));
		}
	}
}

/**
 * Checks for the possibility that a bridge may be on this tile
 * These are in fact all the tile types on which a bridge can be found
 * @param t The tile to analyze
 * @return True if a bridge might have been present prior to savegame 194.
 */
static inline bool MayHaveBridgeAbove(Tile t)
{
	return IsTileType(t, MP_CLEAR) || IsTileType(t, MP_RAILWAY) || IsTileType(t, MP_ROAD) ||
			IsTileType(t, MP_WATER) || IsTileType(t, MP_TUNNELBRIDGE) || IsTileType(t, MP_OBJECT);
}

extern std::shared_ptr<GameStrings> _current_gamestrings_data;

void AfterLoadFindBTProCBInfo() {
	if (_current_gamestrings_data == nullptr) return;

	std::string encoded_prefix;
	StringBuilder builder(encoded_prefix);
	builder.PutUtf8(SCC_ENCODED);
	for (const auto &ls : _current_gamestrings_data->raw_strings) {
		int string_id = 0;
		for (const auto &s : ls.lines) {
			if (s.empty() || s[0] == ';' || s[0] == '#' || s[0] == ' ' || s[0] == '\0') continue;
			if (strncmp(s.c_str(), "STR_TOWN_CLAIMED_CARGOS", strlen("STR_TOWN_CLAIMED_CARGOS")) == 0 ||
					strncmp(s.c_str(), "STR_TOWN_CARGOS_NEEDED_CB", strlen("STR_TOWN_CARGOS_NEEDED_CB")) == 0) {
				bool with_decay = (strncmp(s.c_str(), "STR_TOWN_CLAIMED_CARGOS_DECAY",
					strlen("STR_TOWN_CLAIMED_CARGOS_DECAY")) == 0);
				auto str_prefix = encoded_prefix + fmt::format("{:X}:", string_id);
				auto pn = str_prefix.size();
				for (StoryPageElement *se : StoryPageElement::Iterate()) {
					if (se->text.string.compare(0, pn, str_prefix, 0, pn) != 0) continue;
					uint req, cargomask, from, decay=0;
					if (with_decay) {
						sscanf(se->text.string.c_str() + pn, "%X:%X:%X:%X", &req, &cargomask, &from, &decay);
				 	} else {
						sscanf(se->text.string.c_str() + pn, "%X:%X:%X", &req, &cargomask, &from);
					}
					uint cargo_id = FindFirstBit(cargomask);
					CB_SetCB(true);
					CB_SetRequirements(cargo_id, req, from, decay);
				}
			}
			string_id++;
		}
	}
}

/**
 * Start the scripts.
 */
static void StartScripts()
{
	/* Script debug window requires AIs to be started before trying to start GameScript. */

	/* Start the AIs. */
	for (const Company *c : Company::Iterate()) {
		if (Company::IsValidAiID(c->index)) AI::StartNew(c->index);
	}

	/* Start the GameScript. */
	Game::StartNew();

	ShowScriptDebugWindowIfScriptError();
}

/**
 * Perform a (large) amount of savegame conversion *magic* in order to
 * load older savegames and to fill the caches for various purposes.
 * @return True iff conversion went without a problem.
 */
bool AfterLoadGame()
{
	SetSignalHandlers();

	extern TileIndex _cur_tileloop_tile; // From landscape.cpp.
	/* The LFSR used in RunTileLoop iteration cannot have a zeroed state, make it non-zeroed. */
	if (_cur_tileloop_tile == 0) _cur_tileloop_tile = TileIndex{1};

	if (IsSavegameVersionBefore(SLV_98)) _gamelog.Oldver();

	_gamelog.TestRevision();
	_gamelog.TestMode();

	RebuildTownKdtree();
	RebuildStationKdtree();
	/* This needs to be done even before conversion, because some conversions will destroy objects
	 * that otherwise won't exist in the tree. */
	RebuildViewportKdtree();

	if (IsSavegameVersionBefore(SLV_98)) _gamelog.GRFAddList(_grfconfig);

	if (IsSavegameVersionBefore(SLV_119)) {
		_pause_mode = (_pause_mode.base() == 2) ? PauseMode::Normal : PauseModes{};
	} else if (_network_dedicated && _pause_mode.Test(PauseMode::Error)) {
		Debug(net, 0, "The loading savegame was paused due to an error state");
		Debug(net, 0, "  This savegame cannot be used for multiplayer");
		/* Restore the signals */
		ResetSignalHandlers();
		return false;
	} else if (!_networking || _network_server) {
		/* If we are in singleplayer mode, i.e. not networking, and loading the
		 * savegame or we are loading the savegame as network server we do
		 * not want to be bothered by being paused because of the automatic
		 * reason of a network server, e.g. joining clients or too few
		 * active clients. Note that resetting these values for a network
		 * client are very bad because then the client is going to execute
		 * the game loop when the server is not, i.e. it desyncs. */
		_pause_mode.Reset({PauseMode::ActiveClients, PauseMode::Join});
	}

	/* In very old versions, size of train stations was stored differently.
	 * They had swapped width and height if station was built along the Y axis.
	 * TTO and TTD used 3 bits for width/height, while OpenTTD used 4.
	 * Because the data stored by TTDPatch are unusable for rail stations > 7x7,
	 * recompute the width and height. Doing this unconditionally for all old
	 * savegames simplifies the code. */
	if (IsSavegameVersionBefore(SLV_2)) {
		for (Station *st : Station::Iterate()) {
			st->train_station.w = st->train_station.h = 0;
		}
		for (auto t : Map::Iterate()) {
			if (!IsTileType(t, MP_STATION)) continue;
			if (t.m5() > 7) continue; // is it a rail station tile?
			Station *st = Station::Get(t.m2());
			assert(st->train_station.tile != 0);
			int dx = TileX(t) - TileX(st->train_station.tile);
			int dy = TileY(t) - TileY(st->train_station.tile);
			assert(dx >= 0 && dy >= 0);
			st->train_station.w = std::max<uint>(st->train_station.w, dx + 1);
			st->train_station.h = std::max<uint>(st->train_station.h, dy + 1);
		}
	}

	if (IsSavegameVersionBefore(SLV_194)) {
		_settings_game.construction.map_height_limit = 15;

		/* In old savegame versions, the heightlevel was coded in bits 0..3 of the type field */
		for (auto t : Map::Iterate()) {
			t.height() = GB(t.type(), 0, 4);
			SB(t.type(), 0, 2, GB(t.m6(), 0, 2));
			SB(t.m6(), 0, 2, 0);
			if (MayHaveBridgeAbove(t)) {
				SB(t.type(), 2, 2, GB(t.m6(), 6, 2));
				SB(t.m6(), 6, 2, 0);
			} else {
				SB(t.type(), 2, 2, 0);
			}
		}
	}

	/* in version 2.1 of the savegame, town owner was unified. */
	if (IsSavegameVersionBefore(SLV_2, 1)) ConvertTownOwner();

	/* from version 4.1 of the savegame, exclusive rights are stored at towns */
	if (IsSavegameVersionBefore(SLV_4, 1)) UpdateExclusiveRights();

	/* from version 4.2 of the savegame, currencies are in a different order */
	if (IsSavegameVersionBefore(SLV_4, 2)) UpdateCurrencies();

	/* In old version there seems to be a problem that water is owned by
	 * OWNER_NONE, not OWNER_WATER.. I can't replicate it for the current
	 * (4.3) version, so I just check when versions are older, and then
	 * walk through the whole map.. */
	if (IsSavegameVersionBefore(SLV_4, 3)) {
		for (const auto t : Map::Iterate()) {
			if (IsTileType(t, MP_WATER) && GetTileOwner(t) >= MAX_COMPANIES) {
				SetTileOwner(t, OWNER_WATER);
			}
		}
	}

	if (IsSavegameVersionBefore(SLV_84)) {
		for (Company *c : Company::Iterate()) {
			c->name = CopyFromOldName(c->name_1);
			if (!c->name.empty()) c->name_1 = STR_SV_UNNAMED;
			c->president_name = CopyFromOldName(c->president_name_1);
			if (!c->president_name.empty()) c->president_name_1 = SPECSTR_PRESIDENT_NAME;
		}

		for (Station *st : Station::Iterate()) {
			st->name = CopyFromOldName(st->string_id);
			/* generating new name would be too much work for little effect, use the station name fallback */
			if (!st->name.empty()) st->string_id = STR_SV_STNAME_FALLBACK;
		}

		for (Town *t : Town::Iterate()) {
			t->name = CopyFromOldName(t->townnametype);
			if (!t->name.empty()) t->townnametype = SPECSTR_TOWNNAME_START + _settings_game.game_creation.town_name;
		}
	}

	/* From this point the old names array is cleared. */
	ResetOldNames();

	if (IsSavegameVersionBefore(SLV_106)) {
		/* no station is determined by 'tile == INVALID_TILE' now (instead of '0') */
		for (Station *st : Station::Iterate()) {
			if (st->airport.tile       == 0) st->airport.tile = INVALID_TILE;
			if (st->train_station.tile == 0) st->train_station.tile   = INVALID_TILE;
		}

		/* the same applies to Company::location_of_HQ */
		for (Company *c : Company::Iterate()) {
			if (c->location_of_HQ == 0 || (IsSavegameVersionBefore(SLV_4) && c->location_of_HQ == 0xFFFF)) {
				c->location_of_HQ = INVALID_TILE;
			}
		}
	}

	/* convert road side to my format. */
	if (_settings_game.vehicle.road_side) _settings_game.vehicle.road_side = 1;

	/* Check if all NewGRFs are present, we are very strict in MP mode */
	GRFListCompatibility gcf_res = IsGoodGRFConfigList(_grfconfig);
	for (const auto &c : _grfconfig) {
		if (c->status == GCS_NOT_FOUND) {
			_gamelog.GRFRemove(c->ident.grfid);
		} else if (c->flags.Test(GRFConfigFlag::Compatible)) {
			_gamelog.GRFCompatible(c->ident);
		}
	}

	if (_networking && gcf_res != GLC_ALL_GOOD) {
		SetSaveLoadError(STR_NETWORK_ERROR_CLIENT_NEWGRF_MISMATCH);
		/* Restore the signals */
		ResetSignalHandlers();
		return false;
	}

	switch (gcf_res) {
		case GLC_COMPATIBLE: ShowErrorMessage(GetEncodedString(STR_NEWGRF_COMPATIBLE_LOAD_WARNING), {}, WL_CRITICAL); break;
		case GLC_NOT_FOUND:  ShowErrorMessage(GetEncodedString(STR_NEWGRF_DISABLED_WARNING), {}, WL_CRITICAL); _pause_mode = PauseMode::Error; break;
		default: break;
	}

	/* The value of TimerGameCalendar::date_fract got divided, so make sure that old games are converted correctly. */
	if (IsSavegameVersionBefore(SLV_11, 1) || (IsSavegameVersionBefore(SLV_147) && TimerGameCalendar::date_fract > Ticks::DAY_TICKS)) TimerGameCalendar::date_fract /= 885;

	/* Update current year
	 * must be done before loading sprites as some newgrfs check it */
	TimerGameCalendar::SetDate(TimerGameCalendar::date, TimerGameCalendar::date_fract);

	/* Only new games can use wallclock units. */
	if (IsSavegameVersionBefore(SLV_ECONOMY_MODE_TIMEKEEPING_UNITS)) _settings_game.economy.timekeeping_units = TKU_CALENDAR;

	/* Update economy year. If we don't have a separate economy date saved, follow the calendar date. */
	if (IsSavegameVersionBefore(SLV_ECONOMY_DATE)) {
		TimerGameEconomy::SetDate(TimerGameEconomy::Date{TimerGameCalendar::date.base()}, TimerGameCalendar::date_fract);
	} else {
		TimerGameEconomy::SetDate(TimerGameEconomy::date, TimerGameEconomy::date_fract);
	}

	/*
	 * Force the old behaviour for compatibility reasons with old savegames. As new
	 * settings can only be loaded from new savegames loading old savegames with new
	 * versions of OpenTTD will normally initialize settings newer than the savegame
	 * version with "new game" defaults which the player can define to their liking.
	 * For some settings we override that to keep the behaviour the same as when the
	 * game was saved.
	 *
	 * Note that there is no non-stop in here. This is because the setting could have
	 * either value in TTDPatch. To convert it properly the user has to make sure the
	 * right value has been chosen in the settings. Otherwise we will be converting
	 * it incorrectly in half of the times without a means to correct that.
	 */
	if (IsSavegameVersionBefore(SLV_4, 2)) _settings_game.station.modified_catchment = false;
	if (IsSavegameVersionBefore(SLV_6, 1)) _settings_game.pf.forbid_90_deg = false;
	if (IsSavegameVersionBefore(SLV_21))   _settings_game.vehicle.train_acceleration_model = 0;
	if (IsSavegameVersionBefore(SLV_90))   _settings_game.vehicle.plane_speed = 4;
	if (IsSavegameVersionBefore(SLV_95))   _settings_game.vehicle.dynamic_engines = false;
	if (IsSavegameVersionBefore(SLV_96))   _settings_game.economy.station_noise_level = false;
	if (IsSavegameVersionBefore(SLV_133)) {
		_settings_game.vehicle.train_slope_steepness = 3;
	}
	if (IsSavegameVersionBefore(SLV_134))  _settings_game.economy.feeder_payment_share = 75;
	if (IsSavegameVersionBefore(SLV_138))  _settings_game.vehicle.plane_crashes = 2;
	if (IsSavegameVersionBefore(SLV_139)) {
		_settings_game.vehicle.roadveh_acceleration_model = 0;
		_settings_game.vehicle.roadveh_slope_steepness = 7;
	}
	if (IsSavegameVersionBefore(SLV_143))  _settings_game.economy.allow_town_level_crossings = true;
	if (IsSavegameVersionBefore(SLV_159)) {
		_settings_game.vehicle.max_train_length = 50;
		_settings_game.construction.max_bridge_length = 64;
		_settings_game.construction.max_tunnel_length = 64;
	}
	if (IsSavegameVersionBefore(SLV_166))  _settings_game.economy.infrastructure_maintenance = false;
	if (IsSavegameVersionBefore(SLV_183)) {
		_settings_game.linkgraph.distribution_pax = DT_MANUAL;
		_settings_game.linkgraph.distribution_mail = DT_MANUAL;
		_settings_game.linkgraph.distribution_armoured = DT_MANUAL;
		_settings_game.linkgraph.distribution_default = DT_MANUAL;
	}

	if (IsSavegameVersionBefore(SLV_ENDING_YEAR)) {
		_settings_game.game_creation.ending_year = CalendarTime::DEF_END_YEAR;
	}

	/* Convert linkgraph update settings from days to seconds. */
	if (IsSavegameVersionBefore(SLV_LINKGRAPH_SECONDS)) {
		_settings_game.linkgraph.recalc_interval *= CalendarTime::SECONDS_PER_DAY;
		_settings_game.linkgraph.recalc_time     *= CalendarTime::SECONDS_PER_DAY;
	}

	/* Load the sprites */
	GfxLoadSprites();
	LoadStringWidthTable();

	/* Copy temporary data to Engine pool */
	CopyTempEngineData();

	/* Connect front and rear engines of multiheaded trains and converts
	 * subtype to the new format */
	if (IsSavegameVersionBefore(SLV_17, 1)) ConvertOldMultiheadToNew();

	/* Connect front and rear engines of multiheaded trains */
	ConnectMultiheadedTrains();

	/* Fix the CargoPackets *and* fix the caches of CargoLists.
	 * If this isn't done before Stations and especially Vehicles are
	 * running their AfterLoad we might get in trouble. In the case of
	 * vehicles we could give the wrong (cached) count of items in a
	 * vehicle which causes different results when getting their caches
	 * filled; and that could eventually lead to desyncs. */
	CargoPacket::AfterLoad();

	/* Update all vehicles: Phase 1 */
	AfterLoadVehiclesPhase1(true);

	/* make sure there is a town in the game */
	if (_game_mode == GM_NORMAL && Town::GetNumItems() == 0) {
		SetSaveLoadError(STR_ERROR_NO_TOWN_IN_SCENARIO);
		/* Restore the signals */
		ResetSignalHandlers();
		return false;
	}

	/* The void tiles on the southern border used to belong to a wrong class (pre 4.3).
	 * This problem appears in savegame version 21 too, see r3455. But after loading the
	 * savegame and saving again, the buggy map array could be converted to new savegame
	 * version. It didn't show up before r12070. */
	if (IsSavegameVersionBefore(SLV_87)) UpdateVoidTiles();

	/* Fix the cache for cargo payments. */
	for (CargoPayment *cp : CargoPayment::Iterate()) {
		cp->front->cargo_payment = cp;
		cp->current_station = cp->front->last_station_visited;
	}


	if (IsSavegameVersionBefore(SLV_WATER_TILE_TYPE)) {
		/* Prior to SLV_WATER_TILE_TYPE, the water tile type was stored differently from the enumeration. This has to be
		 * converted before SLV_72 and SLV_82 conversions which use GetWaterTileType. */
		static constexpr uint8_t WBL_COAST_FLAG = 0; ///< Flag for coast.

		for (auto t : Map::Iterate()) {
			if (!IsTileType(t, MP_WATER)) continue;

			switch (GB(t.m5(), 4, 4)) {
				case 0x0: /* Previously WBL_TYPE_NORMAL, Clear water or coast. */
					SetWaterTileType(t, HasBit(t.m5(), WBL_COAST_FLAG) ? WATER_TILE_COAST : WATER_TILE_CLEAR);
					break;

				case 0x1: SetWaterTileType(t, WATER_TILE_LOCK); break; /* Previously WBL_TYPE_LOCK */
				case 0x8: SetWaterTileType(t, WATER_TILE_DEPOT); break; /* Previously WBL_TYPE_DEPOT */
				default: SetWaterTileType(t, WATER_TILE_CLEAR); break; /* Shouldn't happen... */
			}
		}
	}

	if (IsSavegameVersionBefore(SLV_72)) {
		/* Locks in very old savegames had OWNER_WATER as owner */
		for (auto t : Map::Iterate()) {
			switch (GetTileType(t)) {
				default: break;

				case MP_WATER:
					if (GetWaterTileType(t) == WATER_TILE_LOCK && GetTileOwner(t) == OWNER_WATER) SetTileOwner(t, OWNER_NONE);
					break;

				case MP_STATION: {
					if (HasBit(t.m6(), 3)) SetBit(t.m6(), 2);
					StationGfx gfx = GetStationGfx(t);
					StationType st;
					if (       IsInsideMM(gfx,   0,   8)) { // Rail station
						st = StationType::Rail;
						SetStationGfx(t, gfx - 0);
					} else if (IsInsideMM(gfx,   8,  67)) { // Airport
						st = StationType::Airport;
						SetStationGfx(t, gfx - 8);
					} else if (IsInsideMM(gfx,  67,  71)) { // Truck
						st = StationType::Truck;
						SetStationGfx(t, gfx - 67);
					} else if (IsInsideMM(gfx,  71,  75)) { // Bus
						st = StationType::Bus;
						SetStationGfx(t, gfx - 71);
					} else if (gfx == 75) {                 // Oil rig
						st = StationType::Oilrig;
						SetStationGfx(t, gfx - 75);
					} else if (IsInsideMM(gfx,  76,  82)) { // Dock
						st = StationType::Dock;
						SetStationGfx(t, gfx - 76);
					} else if (gfx == 82) {                 // Buoy
						st = StationType::Buoy;
						SetStationGfx(t, gfx - 82);
					} else if (IsInsideMM(gfx,  83, 168)) { // Extended airport
						st = StationType::Airport;
						SetStationGfx(t, gfx - 83 + 67 - 8);
					} else if (IsInsideMM(gfx, 168, 170)) { // Drive through truck
						st = StationType::Truck;
						SetStationGfx(t, gfx - 168 + GFX_TRUCK_BUS_DRIVETHROUGH_OFFSET);
					} else if (IsInsideMM(gfx, 170, 172)) { // Drive through bus
						st = StationType::Bus;
						SetStationGfx(t, gfx - 170 + GFX_TRUCK_BUS_DRIVETHROUGH_OFFSET);
					} else {
						/* Restore the signals */
						ResetSignalHandlers();
						return false;
					}
					SB(t.m6(), 3, 3, to_underlying(st));
					break;
				}
			}
		}
	}

	if (IsSavegameVersionBefore(SLV_INCREASE_STATION_TYPE_FIELD_SIZE)) {
		/* Expansion of station type field in m6 */
		for (auto t : Map::Iterate()) {
			if (IsTileType(t, MP_STATION)) {
				ClrBit(t.m6(), 6);
			}
		}
	}

	for (const auto t : Map::Iterate()) {
		switch (GetTileType(t)) {
			case MP_STATION: {
				BaseStation *bst = BaseStation::GetByTile(t);

				/* Sanity check */
				if (!IsBuoy(t) && bst->owner != GetTileOwner(t)) SlErrorCorrupt("Wrong owner for station tile");

				/* Set up station spread */
				bst->rect.BeforeAddTile(t, StationRect::ADD_FORCE);

				/* Waypoints don't have road stops/oil rigs in the old format */
				if (!Station::IsExpected(bst)) break;
				Station *st = Station::From(bst);

				switch (GetStationType(t)) {
					case StationType::Truck:
					case StationType::Bus:
						if (IsSavegameVersionBefore(SLV_6)) {
							/* Before version 5 you could not have more than 250 stations.
							 * Version 6 adds large maps, so you could only place 253*253
							 * road stops on a map (no freeform edges) = 64009. So, yes
							 * someone could in theory create such a full map to trigger
							 * this assertion, it's safe to assume that's only something
							 * theoretical and does not happen in normal games. */
							assert(RoadStop::CanAllocateItem());

							/* From this version on there can be multiple road stops of the
							 * same type per station. Convert the existing stops to the new
							 * internal data structure. */
							RoadStop *rs = new RoadStop(t);

							RoadStop **head =
								IsTruckStop(t) ? &st->truck_stops : &st->bus_stops;
							*head = rs;
						}
						break;

					case StationType::Oilrig: {
						/* The internal encoding of oil rigs was changed twice.
						 * It was 3 (till 2.2) and later 5 (till 5.1).
						 * DeleteOilRig asserts on the correct type, and
						 * setting it unconditionally does not hurt.
						 */
						Station::GetByTile(t)->airport.type = AT_OILRIG;

						/* Very old savegames sometimes have phantom oil rigs, i.e.
						 * an oil rig which got shut down, but not completely removed from
						 * the map
						 */
						TileIndex t1 = TileAddXY(t, 0, 1);
						if (!IsTileType(t1, MP_INDUSTRY) || GetIndustryGfx(t1) != GFX_OILRIG_1) {
							DeleteOilRig(t);
						}
						break;
					}

					default: break;
				}
				break;
			}

			default: break;
		}
	}

	/* In version 6.1 we put the town index in the map-array. To do this, we need
	 *  to use m2 (16bit big), so we need to clean m2, and that is where this is
	 *  all about ;) */
	if (IsSavegameVersionBefore(SLV_6, 1)) {
		for (auto t : Map::Iterate()) {
			switch (GetTileType(t)) {
				case MP_HOUSE:
					t.m4() = t.m2();
					SetTownIndex(t, CalcClosestTownFromTile(t)->index);
					break;

				case MP_ROAD:
					t.m4() |= (t.m2() << 4);
					if ((GB(t.m5(), 4, 2) == ROAD_TILE_CROSSING ? (Owner)t.m3() : GetTileOwner(t)) == OWNER_TOWN) {
						SetTownIndex(t, CalcClosestTownFromTile(t)->index);
					} else {
						SetTownIndex(t, TownID::Begin());
					}
					break;

				default: break;
			}
		}
	}

	/* Force the freeform edges to false for old savegames. */
	if (IsSavegameVersionBefore(SLV_111)) {
		_settings_game.construction.freeform_edges = false;
	}

	/* From version 9.0, we update the max passengers of a town (was sometimes negative
	 *  before that. */
	if (IsSavegameVersionBefore(SLV_9)) {
		for (Town *t : Town::Iterate()) UpdateTownMaxPass(t);
	}

	/* From version 16.0, we included autorenew on engines, which are now saved, but
	 *  of course, we do need to initialize them for older savegames. */
	if (IsSavegameVersionBefore(SLV_16)) {
		for (Company *c : Company::Iterate()) {
			c->engine_renew_list            = nullptr;
			c->settings.engine_renew        = false;
			c->settings.engine_renew_months = 6;
			c->settings.engine_renew_money  = 100000;
		}

		/* When loading a game, _local_company is not yet set to the correct value.
		 * However, in a dedicated server we are a spectator, so nothing needs to
		 * happen. In case we are not a dedicated server, the local company always
		 * becomes the first available company, unless we are in the scenario editor
		 * where all the companies are 'invalid'.
		 */
		Company *c = Company::GetIfValid(GetFirstPlayableCompanyID());
		if (!_network_dedicated && c != nullptr) {
			c->settings = _settings_client.company;
		}
	}

	if (IsSavegameVersionBefore(SLV_48)) {
		for (auto t : Map::Iterate()) {
			switch (GetTileType(t)) {
				case MP_RAILWAY:
					if (IsPlainRail(t)) {
						/* Swap ground type and signal type for plain rail tiles, so the
						 * ground type uses the same bits as for depots and waypoints. */
						uint tmp = GB(t.m4(), 0, 4);
						SB(t.m4(), 0, 4, GB(t.m2(), 0, 4));
						SB(t.m2(), 0, 4, tmp);
					} else if (HasBit(t.m5(), 2)) {
						/* Split waypoint and depot rail type and remove the subtype. */
						ClrBit(t.m5(), 2);
						ClrBit(t.m5(), 6);
					}
					break;

				case MP_ROAD:
					/* Swap m3 and m4, so the track type for rail crossings is the
					 * same as for normal rail. */
					std::swap(t.m3(), t.m4());
					break;

				default: break;
			}
		}
	}

	if (IsSavegameVersionBefore(SLV_61)) {
		/* Added the RoadType */
		bool old_bridge = IsSavegameVersionBefore(SLV_42);
		for (auto t : Map::Iterate()) {
			switch (GetTileType(t)) {
				case MP_ROAD:
					SB(t.m5(), 6, 2, GB(t.m5(), 4, 2));
					switch (GetRoadTileType(t)) {
						default: SlErrorCorrupt("Invalid road tile type");
						case ROAD_TILE_NORMAL:
							SB(t.m4(), 0, 4, GB(t.m5(), 0, 4));
							SB(t.m4(), 4, 4, 0);
							SB(t.m6(), 2, 4, 0);
							break;
						case ROAD_TILE_CROSSING:
							SB(t.m4(), 5, 2, GB(t.m5(), 2, 2));
							break;
						case ROAD_TILE_DEPOT:    break;
					}
					SB(t.m7(), 6, 2, 1); // Set pre-NRT road type bits for conversion later.
					break;

				case MP_STATION:
					if (IsStationRoadStop(t)) SB(t.m7(), 6, 2, 1);
					break;

				case MP_TUNNELBRIDGE:
					/* Middle part of "old" bridges */
					if (old_bridge && IsBridge(t) && HasBit(t.m5(), 6)) break;
					if (((old_bridge && IsBridge(t)) ? (TransportType)GB(t.m5(), 1, 2) : GetTunnelBridgeTransportType(t)) == TRANSPORT_ROAD) {
						SB(t.m7(), 6, 2, 1); // Set pre-NRT road type bits for conversion later.
					}
					break;

				default: break;
			}
		}
	}

	if (IsSavegameVersionBefore(SLV_114)) {
		bool fix_roadtypes = !IsSavegameVersionBefore(SLV_61);
		bool old_bridge = IsSavegameVersionBefore(SLV_42);

		for (auto t : Map::Iterate()) {
			switch (GetTileType(t)) {
				case MP_ROAD:
					if (fix_roadtypes) SB(t.m7(), 6, 2, GB(t.m7(), 5, 3));
					SB(t.m7(), 5, 1, GB(t.m3(), 7, 1)); // snow/desert
					switch (GetRoadTileType(t)) {
						default: SlErrorCorrupt("Invalid road tile type");
						case ROAD_TILE_NORMAL:
							SB(t.m7(), 0, 4, GB(t.m3(), 0, 4));  // road works
							SB(t.m6(), 3, 3, GB(t.m3(), 4, 3));  // ground
							SB(t.m3(), 0, 4, GB(t.m4(), 4, 4));   // tram bits
							SB(t.m3(), 4, 4, GB(t.m5(), 0, 4));   // tram owner
							SB(t.m5(), 0, 4, GB(t.m4(), 0, 4));   // road bits
							break;

						case ROAD_TILE_CROSSING:
							SB(t.m7(), 0, 5, GB(t.m4(), 0, 5));  // road owner
							SB(t.m6(), 3, 3, GB(t.m3(), 4, 3));  // ground
							SB(t.m3(), 4, 4, GB(t.m5(), 0, 4));   // tram owner
							SB(t.m5(), 0, 1, GB(t.m4(), 6, 1));   // road axis
							SB(t.m5(), 5, 1, GB(t.m4(), 5, 1));   // crossing state
							break;

						case ROAD_TILE_DEPOT:
							break;
					}
					if (!IsRoadDepot(t) && !HasTownOwnedRoad(t)) {
						const Town *town = CalcClosestTownFromTile(t);
						if (town != nullptr) SetTownIndex(t, town->index);
					}
					t.m4() = 0;
					break;

				case MP_STATION:
					if (!IsStationRoadStop(t)) break;

					if (fix_roadtypes) SB(t.m7(), 6, 2, GB(t.m3(), 0, 3));
					SB(t.m7(), 0, 5, (HasBit(t.m6(), 2) ? OWNER_TOWN : GetTileOwner(t)).base());
					SB(t.m3(), 4, 4, t.m1());
					t.m4() = 0;
					break;

				case MP_TUNNELBRIDGE:
					if (old_bridge && IsBridge(t) && HasBit(t.m5(), 6)) break;
					if (((old_bridge && IsBridge(t)) ? (TransportType)GB(t.m5(), 1, 2) : GetTunnelBridgeTransportType(t)) == TRANSPORT_ROAD) {
						if (fix_roadtypes) SB(t.m7(), 6, 2, GB(t.m3(), 0, 3));

						Owner o = GetTileOwner(t);
						SB(t.m7(), 0, 5, o.base()); // road owner
						SB(t.m3(), 4, 4, (o == OWNER_NONE ? OWNER_TOWN : o).base()); // tram owner
					}
					SB(t.m6(), 2, 4, GB(t.m2(), 4, 4)); // bridge type
					SB(t.m7(), 5, 1, GB(t.m4(), 7, 1)); // snow/desert

					t.m2() = 0;
					t.m4() = 0;
					break;

				default: break;
			}
		}
	}

	/* Railtype moved from m3 to m8 in version SLV_EXTEND_RAILTYPES. */
	if (IsSavegameVersionBefore(SLV_EXTEND_RAILTYPES)) {
		for (auto t : Map::Iterate()) {
			switch (GetTileType(t)) {
				case MP_RAILWAY:
					SetRailType(t, (RailType)GB(t.m3(), 0, 4));
					break;

				case MP_ROAD:
					if (IsLevelCrossing(t)) {
						SetRailType(t, (RailType)GB(t.m3(), 0, 4));
					}
					break;

				case MP_STATION:
					if (HasStationRail(t)) {
						SetRailType(t, (RailType)GB(t.m3(), 0, 4));
					}
					break;

				case MP_TUNNELBRIDGE:
					if (GetTunnelBridgeTransportType(t) == TRANSPORT_RAIL) {
						SetRailType(t, (RailType)GB(t.m3(), 0, 4));
					}
					break;

				default:
					break;
			}
		}
	}

	if (IsSavegameVersionBefore(SLV_42)) {
		for (auto t : Map::Iterate()) {
			if (MayHaveBridgeAbove(t)) ClearBridgeMiddle(t);
			if (IsBridgeTile(t)) {
				if (HasBit(t.m5(), 6)) { // middle part
					Axis axis = (Axis)GB(t.m5(), 0, 1);

					if (HasBit(t.m5(), 5)) { // transport route under bridge?
						if (GB(t.m5(), 3, 2) == TRANSPORT_RAIL) {
							MakeRailNormal(
								t,
								GetTileOwner(t),
								axis == AXIS_X ? TRACK_BIT_Y : TRACK_BIT_X,
								GetRailType(t)
							);
						} else {
							TownID town = IsTileOwner(t, OWNER_TOWN) ? ClosestTownFromTile(t, UINT_MAX)->index : TownID::Begin();

							/* MakeRoadNormal */
							SetTileType(t, MP_ROAD);
							t.m2() = town.base();
							t.m3() = 0;
							t.m5() = (axis == AXIS_X ? ROAD_Y : ROAD_X) | ROAD_TILE_NORMAL << 6;
							SB(t.m6(), 2, 4, 0);
							t.m7() = 1 << 6;
							SetRoadOwner(t, RTT_TRAM, OWNER_NONE);
						}
					} else {
						if (GB(t.m5(), 3, 2) == 0) {
							MakeClear(t, CLEAR_GRASS, 3);
						} else {
							if (!IsTileFlat(t)) {
								MakeShore(t);
							} else {
								if (GetTileOwner(t) == OWNER_WATER) {
									MakeSea(t);
								} else {
									MakeCanal(t, GetTileOwner(t), Random());
								}
							}
						}
					}
					SetBridgeMiddle(t, axis);
				} else { // ramp
					Axis axis = (Axis)GB(t.m5(), 0, 1);
					uint north_south = GB(t.m5(), 5, 1);
					DiagDirection dir = ReverseDiagDir(XYNSToDiagDir(axis, north_south));
					TransportType type = (TransportType)GB(t.m5(), 1, 2);

					t.m5() = 1 << 7 | type << 2 | dir;
				}
			}
		}

		for (Vehicle *v : Vehicle::Iterate()) {
			if (!v->IsGroundVehicle()) continue;
			if (IsBridgeTile(v->tile)) {
				DiagDirection dir = GetTunnelBridgeDirection(v->tile);

				if (dir != DirToDiagDir(v->direction)) continue;
				switch (dir) {
					default: SlErrorCorrupt("Invalid vehicle direction");
					case DIAGDIR_NE: if ((v->x_pos & 0xF) !=  0)            continue; break;
					case DIAGDIR_SE: if ((v->y_pos & 0xF) != TILE_SIZE - 1) continue; break;
					case DIAGDIR_SW: if ((v->x_pos & 0xF) != TILE_SIZE - 1) continue; break;
					case DIAGDIR_NW: if ((v->y_pos & 0xF) !=  0)            continue; break;
				}
			} else if (v->z_pos > GetTileMaxPixelZ(TileVirtXY(v->x_pos, v->y_pos))) {
				v->tile = GetNorthernBridgeEnd(v->tile);
				v->UpdatePosition();
			} else {
				continue;
			}
			if (v->type == VEH_TRAIN) {
				Train::From(v)->track = TRACK_BIT_WORMHOLE;
			} else {
				RoadVehicle::From(v)->state = RVSB_WORMHOLE;
			}
		}
	}

	if (IsSavegameVersionBefore(SLV_ROAD_TYPES)) {
		/* Add road subtypes */
		for (auto t : Map::Iterate()) {
			bool has_road = false;
			switch (GetTileType(t)) {
				case MP_ROAD:
					has_road = true;
					break;
				case MP_STATION:
					has_road = IsAnyRoadStop(t);
					break;
				case MP_TUNNELBRIDGE:
					has_road = GetTunnelBridgeTransportType(t) == TRANSPORT_ROAD;
					break;
				default:
					break;
			}

			if (has_road) {
				RoadType road_rt = HasBit(t.m7(), 6) ? ROADTYPE_ROAD : INVALID_ROADTYPE;
				RoadType tram_rt = HasBit(t.m7(), 7) ? ROADTYPE_TRAM : INVALID_ROADTYPE;

				assert(road_rt != INVALID_ROADTYPE || tram_rt != INVALID_ROADTYPE);
				SetRoadTypes(t, road_rt, tram_rt);
				SB(t.m7(), 6, 2, 0); // Clear pre-NRT road type bits.
			}
		}
	}

	/* Elrails got added in rev 24 */
	if (IsSavegameVersionBefore(SLV_24)) {
		RailType min_rail = RAILTYPE_ELECTRIC;

		for (Train *v : Train::Iterate()) {
			RailType rt = RailVehInfo(v->engine_type)->railtype;

			v->railtype = rt;
			if (rt == RAILTYPE_ELECTRIC) min_rail = RAILTYPE_RAIL;
		}

		/* .. so we convert the entire map from normal to elrail (so maintain "fairness") */
		for (const auto t : Map::Iterate()) {
			switch (GetTileType(t)) {
				case MP_RAILWAY:
					SetRailType(t, UpdateRailType(GetRailType(t), min_rail));
					break;

				case MP_ROAD:
					if (IsLevelCrossing(t)) {
						SetRailType(t, UpdateRailType(GetRailType(t), min_rail));
					}
					break;

				case MP_STATION:
					if (HasStationRail(t)) {
						SetRailType(t, UpdateRailType(GetRailType(t), min_rail));
					}
					break;

				case MP_TUNNELBRIDGE:
					if (GetTunnelBridgeTransportType(t) == TRANSPORT_RAIL) {
						SetRailType(t, UpdateRailType(GetRailType(t), min_rail));
					}
					break;

				default:
					break;
			}
		}
	}

	/* In version 16.1 of the savegame a company can decide if trains, which get
	 * replaced, shall keep their old length. In all prior versions, just default
	 * to false */
	if (IsSavegameVersionBefore(SLV_16, 1)) {
		for (Company *c : Company::Iterate()) c->settings.renew_keep_length = false;
	}

	if (IsSavegameVersionBefore(SLV_123)) {
		/* Waypoints became subclasses of stations ... */
		MoveWaypointsToBaseStations();
		/* ... and buoys were moved to waypoints. */
		MoveBuoysToWaypoints();
	}

	/* From version 15, we moved a semaphore bit from bit 2 to bit 3 in m4, making
	 *  room for PBS. Now in version 21 move it back :P. */
	if (IsSavegameVersionBefore(SLV_21) && !IsSavegameVersionBefore(SLV_15)) {
		for (auto t : Map::Iterate()) {
			switch (GetTileType(t)) {
				case MP_RAILWAY:
					if (HasSignals(t)) {
						/* Original signal type/variant was stored in m4 but since saveload
						 * version 48 they are in m2. The bits has been already moved to m2
						 * (see the code somewhere above) so don't use m4, use m2 instead. */

						/* convert PBS signals to combo-signals */
						if (HasBit(t.m2(), 2)) SB(t.m2(), 0, 2, SIGTYPE_COMBO);

						/* move the signal variant back */
						SB(t.m2(), 2, 1, HasBit(t.m2(), 3) ? SIG_SEMAPHORE : SIG_ELECTRIC);
						ClrBit(t.m2(), 3);
					}

					/* Clear PBS reservation on track */
					if (!IsRailDepotTile(t)) {
						SB(t.m4(), 4, 4, 0);
					} else {
						ClrBit(t.m3(), 6);
					}
					break;

				case MP_STATION: // Clear PBS reservation on station
					ClrBit(t.m3(), 6);
					break;

				default: break;
			}
		}
	}

	if (IsSavegameVersionBefore(SLV_25)) {
		/* Remove obsolete VS_WAIT_FOR_SLOT state from road vehicles. */
		static constexpr VehStates OLD_VS_WAIT_FOR_SLOT{0x40};
		for (RoadVehicle *rv : RoadVehicle::Iterate()) {
			rv->vehstatus.Reset(OLD_VS_WAIT_FOR_SLOT);
		}
	}

	if (IsSavegameVersionBefore(SLV_26)) {
		for (Station *st : Station::Iterate()) {
			st->last_vehicle_type = VEH_INVALID;
		}
	}

	YapfNotifyTrackLayoutChange(INVALID_TILE, INVALID_TRACK);

	if (IsSavegameVersionBefore(SLV_34)) {
		for (Company *c : Company::Iterate()) ResetCompanyLivery(c);
	}

	for (Company *c : Company::Iterate()) {
		c->avail_railtypes = GetCompanyRailTypes(c->index);
		c->avail_roadtypes = GetCompanyRoadTypes(c->index);
	}

	AfterLoadStations();

	/* Time starts at 0 instead of 1920.
	 * Account for this in older games by adding an offset */
	if (IsSavegameVersionBefore(SLV_31)) {
		TimerGameCalendar::date += CalendarTime::DAYS_TILL_ORIGINAL_BASE_YEAR;
		TimerGameCalendar::year += CalendarTime::ORIGINAL_BASE_YEAR;
		TimerGameEconomy::date += EconomyTime::DAYS_TILL_ORIGINAL_BASE_YEAR;
		TimerGameEconomy::year += EconomyTime::ORIGINAL_BASE_YEAR;

		for (Station *st : Station::Iterate())   st->build_date      += CalendarTime::DAYS_TILL_ORIGINAL_BASE_YEAR;
		for (Waypoint *wp : Waypoint::Iterate()) wp->build_date      += CalendarTime::DAYS_TILL_ORIGINAL_BASE_YEAR;
		for (Engine *e : Engine::Iterate())      e->intro_date       += CalendarTime::DAYS_TILL_ORIGINAL_BASE_YEAR;
		for (Company *c : Company::Iterate())    c->inaugurated_year += EconomyTime::ORIGINAL_BASE_YEAR;
		for (Industry *i : Industry::Iterate())  i->last_prod_year   += EconomyTime::ORIGINAL_BASE_YEAR;

		for (Vehicle *v : Vehicle::Iterate()) {
			v->date_of_last_service += EconomyTime::DAYS_TILL_ORIGINAL_BASE_YEAR;
			v->build_year += CalendarTime::ORIGINAL_BASE_YEAR;
		}
	}

	/* From 32 on we save the industry who made the farmland.
	 *  To give this prettiness to old savegames, we remove all farmfields and
	 *  plant new ones. */
	if (IsSavegameVersionBefore(SLV_32)) {
		for (const auto t : Map::Iterate()) {
			if (IsTileType(t, MP_CLEAR) && IsClearGround(t, CLEAR_FIELDS)) {
				/* remove fields */
				MakeClear(t, CLEAR_GRASS, 3);
			}
		}

		for (Industry *i : Industry::Iterate()) {
			uint j;

			if (GetIndustrySpec(i->type)->behaviour.Test(IndustryBehaviour::PlantOnBuild)) {
				for (j = 0; j != 50; j++) PlantRandomFarmField(i);
			}
		}
	}

	/* Setting no refit flags to all orders in savegames from before refit in orders were added */
	if (IsSavegameVersionBefore(SLV_36)) {
		for (Order *order : Order::Iterate()) {
			order->SetRefit(CARGO_NO_REFIT);
		}

		for (Vehicle *v : Vehicle::Iterate()) {
			v->current_order.SetRefit(CARGO_NO_REFIT);
		}
	}

	/* from version 38 we have optional elrails, since we cannot know the
	 * preference of a user, let elrails enabled; it can be disabled manually */
	if (IsSavegameVersionBefore(SLV_38)) _settings_game.vehicle.disable_elrails = false;
	/* do the same as when elrails were enabled/disabled manually just now */
	UpdateDisableElrailSettingState(_settings_game.vehicle.disable_elrails, false);
	InitializeRailGUI();

	/* From version 53, the map array was changed for house tiles to allow
	 * space for newhouses grf features. A new byte, m7, was also added. */
	if (IsSavegameVersionBefore(SLV_53)) {
		for (auto t : Map::Iterate()) {
			if (IsTileType(t, MP_HOUSE)) {
				if (GB(t.m3(), 6, 2) != TOWN_HOUSE_COMPLETED) {
					/* Move the construction stage from m3[7..6] to m5[5..4].
					 * The construction counter does not have to move. */
					SB(t.m5(), 3, 2, GB(t.m3(), 6, 2));
					SB(t.m3(), 6, 2, 0);

					/* The "house is completed" bit is now in m6[2]. */
					SetHouseCompleted(t, false);
				} else {
					/* The "lift has destination" bit has been moved from
					 * m5[7] to m7[0]. */
					AssignBit(t.m7(), 0, HasBit(t.m5(), 7));
					ClrBit(t.m5(), 7);

					/* The "lift is moving" bit has been removed, as it does
					 * the same job as the "lift has destination" bit. */
					ClrBit(t.m1(), 7);

					/* The position of the lift goes from m1[7..0] to m6[7..2],
					 * making m1 totally free, now. The lift position does not
					 * have to be a full byte since the maximum value is 36. */
					SetLiftPosition(t, GB(t.m1(), 0, 6 ));

					t.m1() = 0;
					t.m3() = 0;
					SetHouseCompleted(t, true);
				}
			}
		}
	}

	if (IsSavegameVersionBefore(SLV_INCREASE_HOUSE_LIMIT)) {
		for (auto t : Map::Iterate()) {
			if (IsTileType(t, MP_HOUSE)) {
				/* House type is moved from m4 + m3[6] to m8. */
				SetHouseType(t, t.m4() | (GB(t.m3(), 6, 1) << 8));
				t.m4() = 0;
				ClrBit(t.m3(), 6);
			}
		}
	}

	if (IsSavegameVersionBefore(SLV_PROTECT_PLACED_HOUSES)) {
		for (auto t : Map::Iterate()) {
			if (IsTileType(t, MP_HOUSE)) {
				/* We now store house protection status in the map. Set this based on the house spec flags. */
				const HouseSpec *hs = HouseSpec::Get(GetHouseType(t));
				SetHouseProtected(t, hs->extra_flags.Test(HouseExtraFlag::BuildingIsProtected));
			}
		}
	}

	/* Check and update house and town values */
	UpdateHousesAndTowns();

	if (IsSavegameVersionBefore(SLV_43)) {
		for (auto t : Map::Iterate()) {
			if (IsTileType(t, MP_INDUSTRY)) {
				switch (GetIndustryGfx(t)) {
					case GFX_POWERPLANT_SPARKS:
						t.m3() = GB(t.m1(), 2, 5);
						break;

					case GFX_OILWELL_ANIMATED_1:
					case GFX_OILWELL_ANIMATED_2:
					case GFX_OILWELL_ANIMATED_3:
						t.m3() = GB(t.m1(), 0, 2);
						break;

					case GFX_COAL_MINE_TOWER_ANIMATED:
					case GFX_COPPER_MINE_TOWER_ANIMATED:
					case GFX_GOLD_MINE_TOWER_ANIMATED:
						 t.m3() = t.m1();
						 break;

					default: // No animation states to change
						break;
				}
			}
		}
	}

	if (IsSavegameVersionBefore(SLV_45)) {
		/* Originally just the fact that some cargo had been paid for was
		 * stored to stop people cheating and cashing in several times. This
		 * wasn't enough though as it was cleared when the vehicle started
		 * loading again, even if it didn't actually load anything, so now the
		 * amount that has been paid is stored. */
		for (Vehicle *v : Vehicle::Iterate()) {
			v->vehicle_flags.Reset(VehicleFlag{2});
		}
	}

	/* Buoys do now store the owner of the previous water tile, which can never
	 * be OWNER_NONE. So replace OWNER_NONE with OWNER_WATER. */
	if (IsSavegameVersionBefore(SLV_46)) {
		for (Waypoint *wp : Waypoint::Iterate()) {
			if (wp->facilities.Test(StationFacility::Dock) && IsTileOwner(wp->xy, OWNER_NONE) && TileHeight(wp->xy) == 0) SetTileOwner(wp->xy, OWNER_WATER);
		}
	}

	if (IsSavegameVersionBefore(SLV_50)) {
		/* Aircraft units changed from 8 mph to 1 km-ish/h */
		for (Aircraft *v : Aircraft::Iterate()) {
			if (v->subtype <= AIR_AIRCRAFT) {
				const AircraftVehicleInfo *avi = AircraftVehInfo(v->engine_type);
				v->cur_speed *= 128;
				v->cur_speed /= 10;
				v->acceleration = avi->acceleration;
			}
		}
	}

	if (IsSavegameVersionBefore(SLV_49)) for (Company *c : Company::Iterate()) c->face = ConvertFromOldCompanyManagerFace(c->face);

	if (IsSavegameVersionBefore(SLV_52)) {
		for (auto t : Map::Iterate()) {
			if (IsTileType(t, MP_OBJECT) && t.m5() == OBJECT_STATUE) {
				t.m2() = CalcClosestTownFromTile(t)->index.base();
			}
		}
	}

	/* A setting containing the proportion of towns that grow twice as
	 * fast was added in version 54. From version 56 this is now saved in the
	 * town as cities can be built specifically in the scenario editor. */
	if (IsSavegameVersionBefore(SLV_56)) {
		for (Town *t : Town::Iterate()) {
			if (_settings_game.economy.larger_towns != 0 && (t->index % _settings_game.economy.larger_towns) == 0) {
				t->larger_town = true;
			}
		}
	}

	if (IsSavegameVersionBefore(SLV_57)) {
		/* Added a FIFO queue of vehicles loading at stations */
		for (Vehicle *v : Vehicle::Iterate()) {
			if ((v->type != VEH_TRAIN || Train::From(v)->IsFrontEngine()) &&  // for all locs
					!v->vehstatus.Any({VehState::Stopped, VehState::Crashed}) && // not stopped or crashed
					v->current_order.IsType(OT_LOADING)) {         // loading
				Station::Get(v->last_station_visited)->loading_vehicles.push_back(v);

				/* The loading finished flag is *only* set when actually completely
				 * finished. Because the vehicle is loading, it is not finished. */
				v->vehicle_flags.Reset(VehicleFlag::LoadingFinished);
			}
		}
	} else if (IsSavegameVersionBefore(SLV_59)) {
		/* For some reason non-loading vehicles could be in the station's loading vehicle list */

		for (Station *st : Station::Iterate()) {
			for (auto iter = st->loading_vehicles.begin(); iter != st->loading_vehicles.end(); /* nothing */) {
				Vehicle *v = *iter;
				if (!v->current_order.IsType(OT_LOADING)) {
					iter = st->loading_vehicles.erase(iter);
				} else {
					++iter;
				}
			}
		}
	}

	if (IsSavegameVersionBefore(SLV_58)) {
		/* Setting difficulty industry_density other than zero get bumped to +1
		 * since a new option (very low at position 1) has been added */
		if (_settings_game.difficulty.industry_density > 0) {
			_settings_game.difficulty.industry_density++;
		}

		/* Same goes for number of towns, although no test is needed, just an increment */
		_settings_game.difficulty.number_towns++;
	}

	if (IsSavegameVersionBefore(SLV_64)) {
		/* Since now we allow different signal types and variants on a single tile.
		 * Move signal states to m4 to make room and clone the signal type/variant. */
		for (auto t : Map::Iterate()) {
			if (IsTileType(t, MP_RAILWAY) && HasSignals(t)) {
				/* move signal states */
				SetSignalStates(t, GB(t.m2(), 4, 4));
				SB(t.m2(), 4, 4, 0);
				/* clone signal type and variant */
				SB(t.m2(), 4, 3, GB(t.m2(), 0, 3));
			}
		}
	}

	if (IsSavegameVersionBefore(SLV_69)) {
		/* In some old savegames a bit was cleared when it should not be cleared */
		for (RoadVehicle *rv : RoadVehicle::Iterate()) {
			if (rv->state == 250 || rv->state == 251) {
				SetBit(rv->state, 2);
			}
		}
	}

	if (IsSavegameVersionBefore(SLV_70)) {
		/* Added variables to support newindustries */
		for (Industry *i : Industry::Iterate()) i->founder = OWNER_NONE;
	}

	/* From version 82, old style canals (above sealevel (0), WATER owner) are no longer supported.
	    Replace the owner for those by OWNER_NONE. */
	if (IsSavegameVersionBefore(SLV_82)) {
		for (const auto t : Map::Iterate()) {
			if (IsTileType(t, MP_WATER) &&
					GetWaterTileType(t) == WATER_TILE_CLEAR &&
					GetTileOwner(t) == OWNER_WATER &&
					TileHeight(t) != 0) {
				SetTileOwner(t, OWNER_NONE);
			}
		}
	}

	/*
	 * Add the 'previous' owner to the ship depots so we can reset it with
	 * the correct values when it gets destroyed. This prevents that
	 * someone can remove canals owned by somebody else and it prevents
	 * making floods using the removal of ship depots.
	 */
	if (IsSavegameVersionBefore(SLV_83)) {
		for (auto t : Map::Iterate()) {
			if (IsShipDepotTile(t)) {
				t.m4() = (TileHeight(t) == 0 ? OWNER_WATER : OWNER_NONE).base();
			}
		}
	}

	if (IsSavegameVersionBefore(SLV_74)) {
		for (Station *st : Station::Iterate()) {
			for (GoodsEntry &ge : st->goods) {
				ge.last_speed = 0;
				if (ge.HasData() && ge.GetData().cargo.AvailableCount() != 0) ge.status.Set(GoodsEntry::State::Rating);
			}
		}
	}

	/* At version 78, industry cargo types can be changed, and are stored with the industry. For older save versions
	 * copy the IndustrySpec's cargo types over to the Industry. */
	if (IsSavegameVersionBefore(SLV_78)) {
		for (Industry *i : Industry::Iterate()) {
			const IndustrySpec *indsp = GetIndustrySpec(i->type);
			for (uint j = 0; j < std::size(i->produced); j++) {
				i->produced[j].cargo = indsp->produced_cargo[j];
			}
			for (uint j = 0; j < std::size(i->accepted); j++) {
				i->accepted[j].cargo = indsp->accepts_cargo[j];
			}
		}
	}

	/* Industry cargo slots were fixed size before (and including) SLV_VEHICLE_ECONOMY_AGE (either 2/3 or 16/16),
	 * after this they are dynamic. Trim excess slots. */
	if (IsSavegameVersionBeforeOrAt(SLV_VEHICLE_ECONOMY_AGE)) {
		for (Industry *i : Industry::Iterate()) {
			TrimIndustryAcceptedProduced(i);
		}
	}

	/* Before version 81, the density of grass was always stored as zero, and
	 * grassy trees were always drawn fully grassy. Furthermore, trees on rough
	 * land used to have zero density, now they have full density. Therefore,
	 * make all grassy/rough land trees have a density of 3. */
	if (IsSavegameVersionBefore(SLV_81)) {
		for (auto t : Map::Iterate()) {
			if (GetTileType(t) == MP_TREES) {
				TreeGround ground_type = (TreeGround)GB(t.m2(), 4, 2);
				if (ground_type != TREE_GROUND_SNOW_DESERT) SB(t.m2(), 6, 2, 3);
			}
		}
	}


	if (IsSavegameVersionBefore(SLV_93)) {
		/* Rework of orders. */
		for (Order *order : Order::Iterate()) order->ConvertFromOldSavegame();

		for (Vehicle *v : Vehicle::Iterate()) {
			if (v->orders != nullptr && v->orders->GetFirstOrder() != nullptr && v->orders->GetFirstOrder()->IsType(OT_NOTHING)) {
				v->orders->FreeChain();
				v->orders = nullptr;
			}

			v->current_order.ConvertFromOldSavegame();
			if (v->type == VEH_ROAD && v->IsPrimaryVehicle() && v->FirstShared() == v) {
				for (Order *order : v->Orders()) order->SetNonStopType(ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS);
			}
		}
	} else if (IsSavegameVersionBefore(SLV_94)) {
		/* Unload and transfer are now mutual exclusive. */
		for (Order *order : Order::Iterate()) {
			if ((order->GetUnloadType() & (OUFB_UNLOAD | OUFB_TRANSFER)) == (OUFB_UNLOAD | OUFB_TRANSFER)) {
				order->SetUnloadType(OUFB_TRANSFER);
				order->SetLoadType(OLFB_NO_LOAD);
			}
		}

		for (Vehicle *v : Vehicle::Iterate()) {
			if ((v->current_order.GetUnloadType() & (OUFB_UNLOAD | OUFB_TRANSFER)) == (OUFB_UNLOAD | OUFB_TRANSFER)) {
				v->current_order.SetUnloadType(OUFB_TRANSFER);
				v->current_order.SetLoadType(OLFB_NO_LOAD);
			}
		}
	} else if (IsSavegameVersionBefore(SLV_DEPOT_UNBUNCHING)) {
		/* OrderDepotActionFlags were moved, instead of starting at bit 4 they now start at bit 3. */
		for (Order *order : Order::Iterate()) {
			if (!order->IsType(OT_GOTO_DEPOT)) continue;
			order->SetDepotActionType((OrderDepotActionFlags)(order->GetDepotActionType() >> 1));
		}

		for (Vehicle *v : Vehicle::Iterate()) {
			if (!v->current_order.IsType(OT_GOTO_DEPOT)) continue;
			v->current_order.SetDepotActionType((OrderDepotActionFlags)(v->current_order.GetDepotActionType() >> 1));
		}
	}

	/* The water class was moved/unified. */
	if (IsSavegameVersionBefore(SLV_146)) {
		for (auto t : Map::Iterate()) {
			switch (GetTileType(t)) {
				case MP_STATION:
					switch (GetStationType(t)) {
						case StationType::Oilrig:
						case StationType::Dock:
						case StationType::Buoy:
							SetWaterClass(t, (WaterClass)GB(t.m3(), 0, 2));
							SB(t.m3(), 0, 2, 0);
							break;

						default:
							SetWaterClass(t, WATER_CLASS_INVALID);
							break;
					}
					break;

				case MP_WATER:
					SetWaterClass(t, (WaterClass)GB(t.m3(), 0, 2));
					SB(t.m3(), 0, 2, 0);
					break;

				case MP_OBJECT:
					SetWaterClass(t, WATER_CLASS_INVALID);
					break;

				default:
					/* No water class. */
					break;
			}
		}
	}

	if (IsSavegameVersionBefore(SLV_86)) {
		for (auto t : Map::Iterate()) {
			/* Move river flag and update canals to use water class */
			if (IsTileType(t, MP_WATER)) {
				if (GetWaterClass(t) != WATER_CLASS_RIVER) {
					if (IsWater(t)) {
						Owner o = GetTileOwner(t);
						if (o == OWNER_WATER) {
							MakeSea(t);
						} else {
							MakeCanal(t, o, Random());
						}
					} else if (IsShipDepot(t)) {
						Owner o = (Owner)t.m4(); // Original water owner
						SetWaterClass(t, o == OWNER_WATER ? WATER_CLASS_SEA : WATER_CLASS_CANAL);
					}
				}
			}
		}

		/* Update locks, depots, docks and buoys to have a water class based
		 * on its neighbouring tiles. Done after river and canal updates to
		 * ensure neighbours are correct. */
		for (const auto t : Map::Iterate()) {
			if (!IsTileFlat(t)) continue;

			if (IsTileType(t, MP_WATER) && IsLock(t)) SetWaterClassDependingOnSurroundings(t, false);
			if (IsTileType(t, MP_STATION) && (IsDock(t) || IsBuoy(t))) SetWaterClassDependingOnSurroundings(t, false);
		}
	}

	if (IsSavegameVersionBefore(SLV_87)) {
		for (const auto t : Map::Iterate()) {
			/* skip oil rigs at borders! */
			if ((IsTileType(t, MP_WATER) || IsBuoyTile(t)) &&
					(TileX(t) == 0 || TileY(t) == 0 || TileX(t) == Map::MaxX() - 1 || TileY(t) == Map::MaxY() - 1)) {
				/* Some version 86 savegames have wrong water class at map borders (under buoy, or after removing buoy).
				 * This conversion has to be done before buoys with invalid owner are removed. */
				SetWaterClass(t, WATER_CLASS_SEA);
			}

			if (IsBuoyTile(t) || IsDriveThroughStopTile(t) || IsTileType(t, MP_WATER)) {
				Owner o = GetTileOwner(t);
				if (o < MAX_COMPANIES && !Company::IsValidID(o)) {
					Backup<CompanyID> cur_company(_current_company, o);
					ChangeTileOwner(t, o, INVALID_OWNER);
					cur_company.Restore();
				}
				if (IsBuoyTile(t)) {
					/* reset buoy owner to OWNER_NONE in the station struct
					 * (even if it is owned by active company) */
					Waypoint::GetByTile(t)->owner = OWNER_NONE;
				}
			} else if (IsTileType(t, MP_ROAD)) {
				/* works for all RoadTileType */
				for (RoadTramType rtt : _roadtramtypes) {
					/* update even non-existing road types to update tile owner too */
					Owner o = GetRoadOwner(t, rtt);
					if (o < MAX_COMPANIES && !Company::IsValidID(o)) SetRoadOwner(t, rtt, OWNER_NONE);
				}
				if (IsLevelCrossing(t)) {
					if (!Company::IsValidID(GetTileOwner(t))) FixOwnerOfRailTrack(t);
				}
			} else if (IsPlainRailTile(t)) {
				if (!Company::IsValidID(GetTileOwner(t))) FixOwnerOfRailTrack(t);
			}
		}
	}

	if (IsSavegameVersionBefore(SLV_88)) {
		/* Profits are now with 8 bit fract */
		for (Vehicle *v : Vehicle::Iterate()) {
			v->profit_this_year <<= 8;
			v->profit_last_year <<= 8;
			v->running_ticks = 0;
		}
	}

	if (IsSavegameVersionBefore(SLV_91)) {
		/* Increase HouseAnimationFrame from 5 to 7 bits */
		for (auto t : Map::Iterate()) {
			if (IsTileType(t, MP_HOUSE) && GetHouseType(t) >= NEW_HOUSE_OFFSET) {
				SB(t.m6(), 2, 6, GB(t.m6(), 3, 5));
				SB(t.m3(), 5, 1, 0);
			}
		}
	}

	if (IsSavegameVersionBefore(SLV_62)) {
		GroupStatistics::UpdateAfterLoad(); // Ensure statistics pool is initialised before trying to delete vehicles
		/* Remove all trams from savegames without tram support.
		 * There would be trams without tram track under causing crashes sooner or later. */
		for (RoadVehicle *v : RoadVehicle::Iterate()) {
			if (v->First() == v && EngInfo(v->engine_type)->misc_flags.Test(EngineMiscFlag::RoadIsTram)) {
				ShowErrorMessage(GetEncodedString(STR_WARNING_LOADGAME_REMOVED_TRAMS), {}, WL_CRITICAL);
				delete v;
			}
		}
	}

	if (IsSavegameVersionBefore(SLV_99)) {
		for (auto t : Map::Iterate()) {
			/* Set newly introduced WaterClass of industry tiles */
			if (IsTileType(t, MP_STATION) && IsOilRig(t)) {
				SetWaterClassDependingOnSurroundings(t, true);
			}
			if (IsTileType(t, MP_INDUSTRY)) {
				if (GetIndustrySpec(GetIndustryType(t))->behaviour.Test(IndustryBehaviour::BuiltOnWater)) {
					SetWaterClassDependingOnSurroundings(t, true);
				} else {
					SetWaterClass(t, WATER_CLASS_INVALID);
				}
			}

			/* Replace "house construction year" with "house age" */
			if (IsTileType(t, MP_HOUSE) && IsHouseCompleted(t)) {
				t.m5() = ClampTo<uint8_t>(TimerGameCalendar::year - (CalendarTime::ORIGINAL_BASE_YEAR + t.m5()));
			}
		}
	}

	/* Move the signal variant back up one bit for PBS. We don't convert the old PBS
	 * format here, as an old layout wouldn't work properly anyway. To be safe, we
	 * clear any possible PBS reservations as well. */
	if (IsSavegameVersionBefore(SLV_100)) {
		for (auto t : Map::Iterate()) {
			switch (GetTileType(t)) {
				case MP_RAILWAY:
					if (HasSignals(t)) {
						/* move the signal variant */
						SetSignalVariant(t, TRACK_UPPER, HasBit(t.m2(), 2) ? SIG_SEMAPHORE : SIG_ELECTRIC);
						SetSignalVariant(t, TRACK_LOWER, HasBit(t.m2(), 6) ? SIG_SEMAPHORE : SIG_ELECTRIC);
						ClrBit(t.m2(), 2);
						ClrBit(t.m2(), 6);
					}

					/* Clear PBS reservation on track */
					if (IsRailDepot(t)) {
						SetDepotReservation(t, false);
					} else {
						SetTrackReservation(t, TRACK_BIT_NONE);
					}
					break;

				case MP_ROAD: // Clear PBS reservation on crossing
					if (IsLevelCrossing(t)) SetCrossingReservation(t, false);
					break;

				case MP_STATION: // Clear PBS reservation on station
					if (HasStationRail(t)) SetRailStationReservation(t, false);
					break;

				case MP_TUNNELBRIDGE: // Clear PBS reservation on tunnels/bridges
					if (GetTunnelBridgeTransportType(t) == TRANSPORT_RAIL) SetTunnelBridgeReservation(t, false);
					break;

				default: break;
			}
		}
	}

	/* Reserve all tracks trains are currently on. */
	if (IsSavegameVersionBefore(SLV_101)) {
		for (const Train *t : Train::Iterate()) {
			if (t->First() == t) t->ReserveTrackUnderConsist();
		}
	}

	if (IsSavegameVersionBefore(SLV_103)) {
		/* Non-town-owned roads now store the closest town */
		UpdateNearestTownForRoadTiles(false);

		/* signs with invalid owner left from older savegames */
		for (Sign *si : Sign::Iterate()) {
			if (si->owner != OWNER_NONE && !Company::IsValidID(si->owner)) si->owner = OWNER_NONE;
		}

		/* Station can get named based on an industry type, but the current ones
		 * are not, so mark them as if they are not named by an industry. */
		for (Station *st : Station::Iterate()) {
			st->indtype = IT_INVALID;
		}
	}

	if (IsSavegameVersionBefore(SLV_104)) {
		for (Aircraft *a : Aircraft::Iterate()) {
			/* Set engine_type of shadow and rotor */
			if (!a->IsNormalAircraft()) {
				a->engine_type = a->First()->engine_type;
			}
		}

		/* More companies ... */
		for (Company *c : Company::Iterate()) {
			if (c->bankrupt_asked.base() == 0xFF) c->bankrupt_asked.Set();
		}

		for (Engine *e : Engine::Iterate()) {
			if (e->company_avail.base() == 0xFF) e->company_avail.Set();
		}

		for (Town *t : Town::Iterate()) {
			if (t->have_ratings.base() == 0xFF) t->have_ratings.Set();
			for (uint i = 8; i != MAX_COMPANIES; i++) t->ratings[i] = RATING_INITIAL;
		}
	}

	if (IsSavegameVersionBefore(SLV_112)) {
		for (auto t : Map::Iterate()) {
			/* Check for HQ bit being set, instead of using map accessor,
			 * since we've already changed it code-wise */
			if (IsTileType(t, MP_OBJECT) && HasBit(t.m5(), 7)) {
				/* Move size and part identification of HQ out of the m5 attribute,
				 * on new locations */
				t.m3() = GB(t.m5(), 0, 5);
				t.m5() = OBJECT_HQ;
			}
		}
	}
	if (IsSavegameVersionBefore(SLV_144)) {
		for (auto t : Map::Iterate()) {
			if (!IsTileType(t, MP_OBJECT)) continue;

			/* Reordering/generalisation of the object bits. */
			ObjectType type = t.m5();
			SB(t.m6(), 2, 4, type == OBJECT_HQ ? GB(t.m3(), 2, 3) : 0);
			t.m3() = type == OBJECT_HQ ? GB(t.m3(), 1, 1) | GB(t.m3(), 0, 1) << 4 : 0;

			/* Make sure those bits are clear as well! */
			t.m4() = 0;
			t.m7() = 0;
		}
	}

	if (IsSavegameVersionBefore(SLV_147) && Object::GetNumItems() == 0) {
		/* Make real objects for object tiles. */
		for (auto t : Map::Iterate()) {
			if (!IsTileType(t, MP_OBJECT)) continue;

			if (Town::GetNumItems() == 0) {
				/* No towns, so remove all objects! */
				DoClearSquare(t);
			} else {
				uint offset = t.m3();

				/* Also move the animation state. */
				t.m3() = GB(t.m6(), 2, 4);
				SB(t.m6(), 2, 4, 0);

				if (offset == 0) {
					/* No offset, so make the object. */
					ObjectType type = t.m5();
					int size = type == OBJECT_HQ ? 2 : 1;

					if (!Object::CanAllocateItem()) {
						/* Nice... you managed to place 64k lighthouses and
						 * antennae on the map... boohoo. */
						SlError(STR_ERROR_TOO_MANY_OBJECTS);
					}

					Object *o = new Object();
					o->location.tile = (TileIndex)t;
					o->location.w    = size;
					o->location.h    = size;
					o->build_date    = TimerGameCalendar::date;
					o->town          = type == OBJECT_STATUE ? Town::Get(t.m2()) : CalcClosestTownFromTile(t, UINT_MAX);
					t.m2() = o->index.base();
					Object::IncTypeCount(type);
				} else {
					/* We're at an offset, so get the ID from our "root". */
					Tile northern_tile = t - TileXY(GB(offset, 0, 4), GB(offset, 4, 4));
					assert(IsTileType(northern_tile, MP_OBJECT));
					t.m2() = northern_tile.m2();
				}
			}
		}
	}

	if (IsSavegameVersionBefore(SLV_113)) {
		/* allow_town_roads is added, set it if town_layout wasn't TL_NO_ROADS */
		if (_settings_game.economy.town_layout == 0) { // was TL_NO_ROADS
			_settings_game.economy.allow_town_roads = false;
			_settings_game.economy.town_layout = TL_BETTER_ROADS;
		} else {
			_settings_game.economy.allow_town_roads = true;
			_settings_game.economy.town_layout = static_cast<TownLayout>(_settings_game.economy.town_layout - 1);
		}

		/* Initialize layout of all towns. Older versions were using different
		 * generator for random town layout, use it if needed. */
		for (Town *t : Town::Iterate()) {
			if (_settings_game.economy.town_layout != TL_RANDOM) {
				t->layout = _settings_game.economy.town_layout;
				continue;
			}

			/* Use old layout randomizer code */
			uint8_t layout = TileHash(TileX(t->xy), TileY(t->xy)) % 6;
			switch (layout) {
				default: break;
				case 5: layout = 1; break;
				case 0: layout = 2; break;
			}
			t->layout = static_cast<TownLayout>(layout - 1);
		}
	}

	if (IsSavegameVersionBefore(SLV_114)) {
		/* There could be (deleted) stations with invalid owner, set owner to OWNER NONE.
		 * The conversion affects oil rigs and buoys too, but it doesn't matter as
		 * they have st->owner == OWNER_NONE already. */
		for (Station *st : Station::Iterate()) {
			if (!Company::IsValidID(st->owner)) st->owner = OWNER_NONE;
		}
	}

	/* Trains could now stop in a specific location. */
	if (IsSavegameVersionBefore(SLV_117)) {
		for (Order *o : Order::Iterate()) {
			if (o->IsType(OT_GOTO_STATION)) o->SetStopLocation(OSL_PLATFORM_FAR_END);
		}
	}

	if (IsSavegameVersionBefore(SLV_120)) {
		extern VehicleDefaultSettings _old_vds;
		for (Company *c : Company::Iterate()) {
			c->settings.vehicle = _old_vds;
		}
	}

	if (IsSavegameVersionBefore(SLV_121)) {
		/* Delete small ufos heading for non-existing vehicles */
		for (DisasterVehicle *v : DisasterVehicle::Iterate()) {
			if (v->subtype == 2 /* ST_SMALL_UFO */ && v->state != 0) {
				const Vehicle *u = Vehicle::GetIfValid(v->dest_tile.base());
				if (u == nullptr || u->type != VEH_ROAD || !RoadVehicle::From(u)->IsFrontEngine()) {
					delete v;
				}
			}
		}

		/* We didn't store cargo payment yet, so make them for vehicles that are
		 * currently at a station and loading/unloading. If they don't get any
		 * payment anymore they just removed in the next load/unload cycle.
		 * However, some 0.7 versions might have cargo payment. For those we just
		 * add cargopayment for the vehicles that don't have it.
		 */
		for (Station *st : Station::Iterate()) {
			for (Vehicle *v : st->loading_vehicles) {
				/* There are always as many CargoPayments as Vehicles. We need to make the
				 * assert() in Pool::GetNew() happy by calling CanAllocateItem(). */
				static_assert(CargoPaymentPool::MAX_SIZE == VehiclePool::MAX_SIZE);
				assert(CargoPayment::CanAllocateItem());
				if (v->cargo_payment == nullptr) v->cargo_payment = new CargoPayment(v);
			}
		}
	}

	if (IsSavegameVersionBefore(SLV_122)) {
		/* Animated tiles would sometimes not be actually animated or
		 * in case of old savegames duplicate. */

		extern std::vector<TileIndex> _animated_tiles;

		for (auto tile = _animated_tiles.begin(); tile < _animated_tiles.end(); /* Nothing */) {
			/* Remove if tile is not animated */
			bool remove = !MayAnimateTile(*tile);

			/* and remove if duplicate */
			for (auto j = _animated_tiles.begin(); !remove && j < tile; j++) {
				remove = *tile == *j;
			}

			if (remove) {
				tile = _animated_tiles.erase(tile);
			} else {
				tile++;
			}
		}
	}

	if (IsSavegameVersionBefore(SLV_NONFLOODING_WATER_TILES)) {
		for (auto t : Map::Iterate()) {
			if (!IsTileType(t, MP_WATER)) continue;
			SetNonFloodingWaterTile(t, false);
		}
	}

	if (IsSavegameVersionBefore(SLV_ANIMATED_TILE_STATE_IN_MAP)) {
		/* Animated tile state is stored in the map array, allowing
		 * quicker addition and deletion of animated tiles. */

		extern std::vector<TileIndex> _animated_tiles;

		for (auto t : Map::Iterate()) {
			/* Ensure there is no spurious animated tile state. */
			if (MayAnimateTile(t)) SetAnimatedTileState(t, AnimatedTileState::None);
		}

		/* Set animated flag for all valid animated tiles. */
		for (const TileIndex &tile : _animated_tiles) {
			if (tile != INVALID_TILE) SetAnimatedTileState(tile, AnimatedTileState::Animated);
		}
	}

	if (IsSavegameVersionBefore(SLV_124) && !IsSavegameVersionBefore(SLV_1)) {
		/* The train station tile area was added, but for really old (TTDPatch) it's already valid. */
		for (Waypoint *wp : Waypoint::Iterate()) {
			if (wp->facilities.Test(StationFacility::Train)) {
				wp->train_station.tile = wp->xy;
				wp->train_station.w = 1;
				wp->train_station.h = 1;
			} else {
				wp->train_station.tile = INVALID_TILE;
				wp->train_station.w = 0;
				wp->train_station.h = 0;
			}
		}
	}

	if (IsSavegameVersionBefore(SLV_125)) {
		/* Convert old subsidies */
		for (Subsidy *s : Subsidy::Iterate()) {
			if (s->remaining < 12) {
				/* Converting nonawarded subsidy */
				s->remaining = 12 - s->remaining; // convert "age" to "remaining"
				s->awarded = CompanyID::Invalid(); // not awarded to anyone
				const CargoSpec *cs = CargoSpec::Get(s->cargo_type);
				switch (cs->town_acceptance_effect) {
					case TAE_PASSENGERS:
					case TAE_MAIL:
						/* Town -> Town */
						s->src.type = s->dst.type = SourceType::Town;
						if (Town::IsValidID(s->src.ToTownID()) && Town::IsValidID(s->dst.ToTownID())) continue;
						break;
					case TAE_GOODS:
					case TAE_FOOD:
						/* Industry -> Town */
						s->src.type = SourceType::Industry;
						s->dst.type = SourceType::Town;
						if (Industry::IsValidID(s->src.ToIndustryID()) && Town::IsValidID(s->dst.ToTownID())) continue;
						break;
					default:
						/* Industry -> Industry */
						s->src.type = s->dst.type = SourceType::Industry;
						if (Industry::IsValidID(s->src.ToIndustryID()) && Industry::IsValidID(s->dst.ToIndustryID())) continue;
						break;
				}
			} else {
				/* Do our best for awarded subsidies. The original source or destination industry
				 * can't be determined anymore for awarded subsidies, so invalidate them.
				 * Town -> Town subsidies are converted using simple heuristic */
				s->remaining = 24 - s->remaining; // convert "age of awarded subsidy" to "remaining"
				const CargoSpec *cs = CargoSpec::Get(s->cargo_type);
				switch (cs->town_acceptance_effect) {
					case TAE_PASSENGERS:
					case TAE_MAIL: {
						/* Town -> Town */
						const Station *ss = Station::GetIfValid(s->src.id);
						const Station *sd = Station::GetIfValid(s->dst.id);
						if (ss != nullptr && sd != nullptr && ss->owner == sd->owner &&
								Company::IsValidID(ss->owner)) {
							s->src.type = s->dst.type = SourceType::Town;
							s->src.SetIndex(ss->town->index);
							s->dst.SetIndex(sd->town->index);
							s->awarded = ss->owner;
							continue;
						}
						break;
					}
					default:
						break;
				}
			}
			/* Awarded non-town subsidy or invalid source/destination, invalidate */
			delete s;
		}
	}

	if (IsSavegameVersionBefore(SLV_126)) {
		/* Recompute inflation based on old unround loan limit
		 * Note: Max loan is 500000. With an inflation of 4% across 170 years
		 *       that results in a max loan of about 0.7 * 2^31.
		 *       So taking the 16 bit fractional part into account there are plenty of bits left
		 *       for unmodified savegames ...
		 */
		uint64_t aimed_inflation = (_economy.old_max_loan_unround << 16 | _economy.old_max_loan_unround_fract) / _settings_game.difficulty.max_loan;

		/* ... well, just clamp it then. */
		if (aimed_inflation > MAX_INFLATION) aimed_inflation = MAX_INFLATION;

		/* Simulate the inflation, so we also get the payment inflation */
		while (_economy.inflation_prices < aimed_inflation) {
			if (AddInflation(false)) break;
		}
	}

	if (IsSavegameVersionBefore(SLV_128)) {
		for (const Depot *d : Depot::Iterate()) {
			Tile tile = d->xy;
			/* At some point, invalid depots were saved into the game (possibly those removed in the past?)
			 * Remove them here, so they don't cause issues further down the line */
			if (!IsDepotTile(tile)) {
				Debug(sl, 0, "Removing invalid depot {} at {}, {}", d->index, TileX(d->xy), TileY(d->xy));
				delete d;
				d = nullptr;
				continue;
			}
			tile.m2() = d->index.base();
			if (IsTileType(tile, MP_WATER)) Tile(GetOtherShipDepotTile(tile)).m2() = d->index.base();
		}
	}

	/* The behaviour of force_proceed has been changed. Now
	 * it counts signals instead of some random time out. */
	if (IsSavegameVersionBefore(SLV_131)) {
		for (Train *t : Train::Iterate()) {
			if (t->force_proceed != TFP_NONE) {
				t->force_proceed = TFP_STUCK;
			}
		}
	}

	/* The bits for the tree ground and tree density have
	 * been swapped (m2 bits 7..6 and 5..4. */
	if (IsSavegameVersionBefore(SLV_135)) {
		for (auto t : Map::Iterate()) {
			if (IsTileType(t, MP_CLEAR)) {
				if (GetClearGround(t) == CLEAR_SNOW) { // CLEAR_SNOW becomes CLEAR_GRASS with IsSnowTile() set.
					SetClearGroundDensity(t, CLEAR_GRASS, GetClearDensity(t));
					SetBit(t.m3(), 4);
				} else {
					ClrBit(t.m3(), 4);
				}
			}
			if (IsTileType(t, MP_TREES)) {
				uint density = GB(t.m2(), 6, 2);
				uint ground = GB(t.m2(), 4, 2);
				t.m2() = ground << 6 | density << 4;
			}
		}
	}

	/* Wait counter and load/unload ticks got split. */
	if (IsSavegameVersionBefore(SLV_136)) {
		for (Aircraft *a : Aircraft::Iterate()) {
			a->turn_counter = a->current_order.IsType(OT_LOADING) ? 0 : a->load_unload_ticks;
		}

		for (Train *t : Train::Iterate()) {
			t->wait_counter = t->current_order.IsType(OT_LOADING) ? 0 : t->load_unload_ticks;
		}
	}

	/* Airport tile animation uses animation frame instead of other graphics id */
	if (IsSavegameVersionBefore(SLV_137)) {
		struct AirportTileConversion {
			uint8_t old_start;
			uint8_t num_frames;
		};
		static const AirportTileConversion atcs[] = {
			{31,  12}, // APT_RADAR_GRASS_FENCE_SW
			{50,   4}, // APT_GRASS_FENCE_NE_FLAG
			{62,   2}, // 1 unused tile
			{66,  12}, // APT_RADAR_FENCE_SW
			{78,  12}, // APT_RADAR_FENCE_NE
			{101, 10}, // 9 unused tiles
			{111,  8}, // 7 unused tiles
			{119, 15}, // 14 unused tiles (radar)
			{140,  4}, // APT_GRASS_FENCE_NE_FLAG_2
		};
		for (const auto t : Map::Iterate()) {
			if (IsAirportTile(t)) {
				StationGfx old_gfx = GetStationGfx(t);
				uint8_t offset = 0;
				for (const auto &atc : atcs) {
					if (old_gfx < atc.old_start) {
						SetStationGfx(t, old_gfx - offset);
						break;
					}
					if (old_gfx < atc.old_start + atc.num_frames) {
						SetAnimationFrame(t, old_gfx - atc.old_start);
						SetStationGfx(t, atc.old_start - offset);
						break;
					}
					offset += atc.num_frames - 1;
				}
			}
		}
	}

	/* Oilrig was moved from id 15 to 9. */
	if (IsSavegameVersionBefore(SLV_139)) {
		for (Station *st : Station::Iterate()) {
			if (st->airport.tile != INVALID_TILE && st->airport.type == 15) {
				st->airport.type = AT_OILRIG;
			}
		}
	}

	if (IsSavegameVersionBefore(SLV_140)) {
		for (Station *st : Station::Iterate()) {
			if (st->airport.tile != INVALID_TILE) {
				st->airport.w = st->airport.GetSpec()->size_x;
				st->airport.h = st->airport.GetSpec()->size_y;
			}
		}
	}

	if (IsSavegameVersionBefore(SLV_141)) {
		for (const auto t : Map::Iterate()) {
			/* Reset tropic zone for VOID tiles, they shall not have any. */
			if (IsTileType(t, MP_VOID)) SetTropicZone(t, TROPICZONE_NORMAL);
		}

		/* We need to properly number/name the depots.
		 * The first step is making sure none of the depots uses the
		 * 'default' names, after that we can assign the names. */
		for (Depot *d : Depot::Iterate()) d->town_cn = UINT16_MAX;

		for (Depot *d : Depot::Iterate()) MakeDefaultName(d);
	}

	if (IsSavegameVersionBefore(SLV_142)) {
		for (Depot *d : Depot::Iterate()) d->build_date = TimerGameCalendar::date;
	}

	if (IsSavegameVersionBefore(SLV_145)) {
		for (Station *st : Station::Iterate()) {
			if (st->facilities.Test(StationFacility::Airport)) st->airport.rotation = DIR_N;
		}
	}

	/* In old versions it was possible to remove an airport while a plane was
	 * taking off or landing. This gives all kind of problems when building
	 * another airport in the same station so we don't allow that anymore.
	 * For old savegames with such aircraft we just throw them in the air and
	 * treat the aircraft like they were flying already. */
	if (IsSavegameVersionBefore(SLV_146)) {
		for (Aircraft *v : Aircraft::Iterate()) {
			if (!v->IsNormalAircraft()) continue;
			Station *st = GetTargetAirportIfValid(v);
			if (st == nullptr && v->state != FLYING) {
				v->state = FLYING;
				UpdateAircraftCache(v);
				AircraftNextAirportPos_and_Order(v);
				/* get aircraft back on running altitude */
				if (!v->vehstatus.Test(VehState::Crashed)) {
					GetAircraftFlightLevelBounds(v, &v->z_pos, nullptr);
					SetAircraftPosition(v, v->x_pos, v->y_pos, GetAircraftFlightLevel(v));
				}
			}
		}
	}

	/* Move the animation frame to the same location (m7) for all objects. */
	if (IsSavegameVersionBefore(SLV_147)) {
		for (auto t : Map::Iterate()) {
			switch (GetTileType(t)) {
				case MP_HOUSE:
					if (GetHouseType(t) >= NEW_HOUSE_OFFSET) {
						uint per_proc = t.m7();
						t.m7() = GB(t.m6(), 2, 6) | (GB(t.m3(), 5, 1) << 6);
						SB(t.m3(), 5, 1, 0);
						SB(t.m6(), 2, 6, std::min(per_proc, 63U));
					}
					break;

				case MP_INDUSTRY: {
					uint rand = t.m7();
					t.m7() = t.m3();
					t.m3() = rand;
					break;
				}

				case MP_OBJECT:
					t.m7() = t.m3();
					t.m3() = 0;
					break;

				default:
					/* For stations/airports it's already at m7 */
					break;
			}
		}
	}

	/* Add (random) colour to all objects. */
	if (IsSavegameVersionBefore(SLV_148)) {
		for (Object *o : Object::Iterate()) {
			Owner owner = GetTileOwner(o->location.tile);
			o->colour = (owner == OWNER_NONE) ? static_cast<Colours>(GB(Random(), 0, 4)) : Company::Get(owner)->livery[0].colour1;
		}
	}

	if (IsSavegameVersionBefore(SLV_149)) {
		for (const auto t : Map::Iterate()) {
			if (!IsTileType(t, MP_STATION)) continue;
			if (!IsBuoy(t) && !IsOilRig(t) && !(IsDock(t) && IsTileFlat(t))) {
				SetWaterClass(t, WATER_CLASS_INVALID);
			}
		}

		/* Waypoints with custom name may have a non-unique town_cn,
		 * renumber those. First set all affected waypoints to the
		 * highest possible number to get them numbered in the
		 * order they have in the pool. */
		for (Waypoint *wp : Waypoint::Iterate()) {
			if (!wp->name.empty()) wp->town_cn = UINT16_MAX;
		}

		for (Waypoint *wp : Waypoint::Iterate()) {
			if (!wp->name.empty()) MakeDefaultName(wp);
		}
	}

	if (IsSavegameVersionBefore(SLV_152)) {
		_industry_builder.Reset(); // Initialize industry build data.

		/* The moment vehicles go from hidden to visible changed. This means
		 * that vehicles don't always get visible anymore causing things to
		 * get messed up just after loading the savegame. This fixes that. */
		for (Vehicle *v : Vehicle::Iterate()) {
			/* Not all vehicle types can be inside a tunnel. Furthermore,
			 * testing IsTunnelTile() for invalid tiles causes a crash. */
			if (!v->IsGroundVehicle()) continue;

			/* Is the vehicle in a tunnel? */
			if (!IsTunnelTile(v->tile)) continue;

			/* Is the vehicle actually at a tunnel entrance/exit? */
			TileIndex vtile = TileVirtXY(v->x_pos, v->y_pos);
			if (!IsTunnelTile(vtile)) continue;

			/* Are we actually in this tunnel? Or maybe a lower tunnel? */
			if (GetSlopePixelZ(v->x_pos, v->y_pos, true) != v->z_pos) continue;

			/* What way are we going? */
			const DiagDirection dir = GetTunnelBridgeDirection(vtile);
			const DiagDirection vdir = DirToDiagDir(v->direction);

			/* Have we passed the visibility "switch" state already? */
			uint8_t pos = (DiagDirToAxis(vdir) == AXIS_X ? v->x_pos : v->y_pos) & TILE_UNIT_MASK;
			uint8_t frame = (vdir == DIAGDIR_NE || vdir == DIAGDIR_NW) ? TILE_SIZE - 1 - pos : pos;
			extern const uint8_t _tunnel_visibility_frame[DIAGDIR_END];

			/* Should the vehicle be hidden or not? */
			bool hidden;
			if (dir == vdir) { // Entering tunnel
				hidden = frame >= _tunnel_visibility_frame[dir];
				v->tile = vtile;
			} else if (dir == ReverseDiagDir(vdir)) { // Leaving tunnel
				hidden = frame < TILE_SIZE - _tunnel_visibility_frame[dir];
				/* v->tile changes at the moment when the vehicle leaves the tunnel. */
				v->tile = hidden ? GetOtherTunnelBridgeEnd(vtile) : vtile;
			} else {
				/* We could get here in two cases:
				 * - for road vehicles, it is reversing at the end of the tunnel
				 * - it is crashed in the tunnel entry (both train or RV destroyed by UFO)
				 * Whatever case it is, do not change anything and use the old values.
				 * Especially changing RV's state would break its reversing in the middle. */
				continue;
			}

			if (hidden) {
				v->vehstatus.Set(VehState::Hidden);

				switch (v->type) {
					case VEH_TRAIN: Train::From(v)->track       = TRACK_BIT_WORMHOLE; break;
					case VEH_ROAD:  RoadVehicle::From(v)->state = RVSB_WORMHOLE;      break;
					default: NOT_REACHED();
				}
			} else {
				v->vehstatus.Reset(VehState::Hidden);

				switch (v->type) {
					case VEH_TRAIN: Train::From(v)->track       = DiagDirToDiagTrackBits(vdir); break;
					case VEH_ROAD:  RoadVehicle::From(v)->state = DiagDirToDiagTrackdir(vdir); RoadVehicle::From(v)->frame = frame; break;
					default: NOT_REACHED();
				}
			}
		}
	}

	if (IsSavegameVersionBefore(SLV_153)) {
		for (RoadVehicle *rv : RoadVehicle::Iterate()) {
			if (rv->state == RVSB_IN_DEPOT || rv->state == RVSB_WORMHOLE) continue;

			bool loading = rv->current_order.IsType(OT_LOADING) || rv->current_order.IsType(OT_LEAVESTATION);
			if (HasBit(rv->state, RVS_IN_ROAD_STOP)) {
				extern const uint8_t _road_stop_stop_frame[];
				SB(rv->state, RVS_ENTERED_STOP, 1, loading || rv->frame > _road_stop_stop_frame[rv->state - RVSB_IN_ROAD_STOP + (_settings_game.vehicle.road_side << RVS_DRIVE_SIDE)]);
			} else if (HasBit(rv->state, RVS_IN_DT_ROAD_STOP)) {
				SB(rv->state, RVS_ENTERED_STOP, 1, loading || rv->frame > RVC_DRIVE_THROUGH_STOP_FRAME);
			}
		}
	}

	if (IsSavegameVersionBefore(SLV_156)) {
		/* The train's pathfinder lost flag got moved. */
		for (Train *t : Train::Iterate()) {
			if (!HasBit(t->flags, 5)) continue;

			ClrBit(t->flags, 5);
			t->vehicle_flags.Set(VehicleFlag::PathfinderLost);
		}

		/* Introduced terraform/clear limits. */
		for (Company *c : Company::Iterate()) {
			c->terraform_limit = _settings_game.construction.terraform_frame_burst << 16;
			c->clear_limit     = _settings_game.construction.clear_frame_burst << 16;
		}
	}


	if (IsSavegameVersionBefore(SLV_CONSISTENT_PARTIAL_Z)) {
		/*
		 * The logic of GetPartialPixelZ has been changed, so the resulting Zs on
		 * the map are consistent. This requires that the Z position of some
		 * vehicles is updated to reflect this new situation.
		 *
		 * This needs to be before SLV_158, because that performs asserts using
		 * GetSlopePixelZ which internally uses GetPartialPixelZ.
		 */
		for (Vehicle *v : Vehicle::Iterate()) {
			if (v->IsGroundVehicle() && TileVirtXY(v->x_pos, v->y_pos) == v->tile) {
				/* Vehicle is on the ground, and not in a wormhole. */
				v->z_pos = GetSlopePixelZ(v->x_pos, v->y_pos, true);
			}
		}
	}

	if (IsSavegameVersionBefore(SLV_158)) {
		for (Vehicle *v : Vehicle::Iterate()) {
			switch (v->type) {
				case VEH_TRAIN: {
					Train *t = Train::From(v);

					/* Clear old GOINGUP / GOINGDOWN flags.
					 * It was changed in savegame version 139, but savegame
					 * version 158 doesn't use these bits, so it doesn't hurt
					 * to clear them unconditionally. */
					ClrBit(t->flags, 1);
					ClrBit(t->flags, 2);

					/* Clear both bits first. */
					ClrBit(t->gv_flags, GVF_GOINGUP_BIT);
					ClrBit(t->gv_flags, GVF_GOINGDOWN_BIT);

					/* Crashed vehicles can't be going up/down. */
					if (t->vehstatus.Test(VehState::Crashed)) break;

					/* Only X/Y tracks can be sloped. */
					if (t->track != TRACK_BIT_X && t->track != TRACK_BIT_Y) break;

					t->gv_flags |= FixVehicleInclination(t, t->direction);
					break;
				}
				case VEH_ROAD: {
					RoadVehicle *rv = RoadVehicle::From(v);
					ClrBit(rv->gv_flags, GVF_GOINGUP_BIT);
					ClrBit(rv->gv_flags, GVF_GOINGDOWN_BIT);

					/* Crashed vehicles can't be going up/down. */
					if (rv->vehstatus.Test(VehState::Crashed)) break;

					if (rv->state == RVSB_IN_DEPOT || rv->state == RVSB_WORMHOLE) break;

					TrackStatus ts = GetTileTrackStatus(rv->tile, TRANSPORT_ROAD, GetRoadTramType(rv->roadtype));
					TrackBits trackbits = TrackStatusToTrackBits(ts);

					/* Only X/Y tracks can be sloped. */
					if (trackbits != TRACK_BIT_X && trackbits != TRACK_BIT_Y) break;

					Direction dir = rv->direction;

					/* Test if we are reversing. */
					Axis a = trackbits == TRACK_BIT_X ? AXIS_X : AXIS_Y;
					if (AxisToDirection(a) != dir &&
							AxisToDirection(a) != ReverseDir(dir)) {
						/* When reversing, the road vehicle is on the edge of the tile,
						 * so it can be safely compared to the middle of the tile. */
						dir = INVALID_DIR;
					}

					rv->gv_flags |= FixVehicleInclination(rv, dir);
					break;
				}
				case VEH_SHIP:
					break;

				default:
					continue;
			}

			if (IsBridgeTile(v->tile) && TileVirtXY(v->x_pos, v->y_pos) == v->tile) {
				/* In old versions, z_pos was 1 unit lower on bridge heads.
				 * However, this invalid state could be converted to new savegames
				 * by loading and saving the game in a new version. */
				v->z_pos = GetSlopePixelZ(v->x_pos, v->y_pos, true);
				DiagDirection dir = GetTunnelBridgeDirection(v->tile);
				if (v->type == VEH_TRAIN && !v->vehstatus.Test(VehState::Crashed) &&
						v->direction != DiagDirToDir(dir)) {
					/* If the train has left the bridge, it shouldn't have
					 * track == TRACK_BIT_WORMHOLE - this could happen
					 * when the train was reversed while on the last "tick"
					 * on the ramp before leaving the ramp to the bridge. */
					Train::From(v)->track = DiagDirToDiagTrackBits(dir);
				}
			}

			/* If the vehicle is really above v->tile (not in a wormhole),
			 * it should have set v->z_pos correctly. */
			assert(v->tile != TileVirtXY(v->x_pos, v->y_pos) || v->z_pos == GetSlopePixelZ(v->x_pos, v->y_pos, true));
		}

		/* Fill Vehicle::cur_real_order_index */
		for (Vehicle *v : Vehicle::Iterate()) {
			if (!v->IsPrimaryVehicle()) continue;

			/* Older versions are less strict with indices being in range and fix them on the fly */
			if (v->cur_implicit_order_index >= v->GetNumOrders()) v->cur_implicit_order_index = 0;

			v->cur_real_order_index = v->cur_implicit_order_index;
			v->UpdateRealOrderIndex();
		}
	}

	if (IsSavegameVersionBefore(SLV_159)) {
		/* If the savegame is old (before version 100), then the value of 255
		 * for these settings did not mean "disabled". As such everything
		 * before then did reverse.
		 * To simplify stuff we disable all turning around or we do not
		 * disable anything at all. So, if some reversing was disabled we
		 * will keep reversing disabled, otherwise it'll be turned on. */
		_settings_game.pf.reverse_at_signals = IsSavegameVersionBefore(SLV_100) || (_settings_game.pf.wait_oneway_signal != 255 && _settings_game.pf.wait_twoway_signal != 255 && _settings_game.pf.wait_for_pbs_path != 255);

		for (Train *t : Train::Iterate()) {
			_settings_game.vehicle.max_train_length = std::max<uint8_t>(_settings_game.vehicle.max_train_length, CeilDiv(t->gcache.cached_total_length, TILE_SIZE));
		}
	}

	if (IsSavegameVersionBefore(SLV_160)) {
		/* Setting difficulty industry_density other than zero get bumped to +1
		 * since a new option (minimal at position 1) has been added */
		if (_settings_game.difficulty.industry_density > 0) {
			_settings_game.difficulty.industry_density++;
		}
	}

	if (IsSavegameVersionBefore(SLV_161)) {
		/* Before savegame version 161, persistent storages were not stored in a pool. */

		if (!IsSavegameVersionBefore(SLV_76)) {
			for (Industry *ind : Industry::Iterate()) {
				assert(ind->psa != nullptr);

				/* Check if the old storage was empty. */
				bool is_empty = true;
				for (uint i = 0; i < sizeof(ind->psa->storage); i++) {
					if (ind->psa->GetValue(i) != 0) {
						is_empty = false;
						break;
					}
				}

				if (!is_empty) {
					ind->psa->grfid = _industry_mngr.GetGRFID(ind->type);
				} else {
					delete ind->psa;
					ind->psa = nullptr;
				}
			}
		}

		if (!IsSavegameVersionBefore(SLV_145)) {
			for (Station *st : Station::Iterate()) {
				if (!st->facilities.Test(StationFacility::Airport)) continue;
				assert(st->airport.psa != nullptr);

				/* Check if the old storage was empty. */
				bool is_empty = true;
				for (uint i = 0; i < sizeof(st->airport.psa->storage); i++) {
					if (st->airport.psa->GetValue(i) != 0) {
						is_empty = false;
						break;
					}
				}

				if (!is_empty) {
					st->airport.psa->grfid = _airport_mngr.GetGRFID(st->airport.type);
				} else {
					delete st->airport.psa;
					st->airport.psa = nullptr;

				}
			}
		}
	}

	/* This triggers only when old snow_lines were copied into the snow_line_height. */
	if (IsSavegameVersionBefore(SLV_164) && _settings_game.game_creation.snow_line_height >= MIN_SNOWLINE_HEIGHT * TILE_HEIGHT) {
		_settings_game.game_creation.snow_line_height /= TILE_HEIGHT;
	}

	if (IsSavegameVersionBefore(SLV_164) && !IsSavegameVersionBefore(SLV_32)) {
		/* We store 4 fences in the field tiles instead of only SE and SW. */
		for (auto t : Map::Iterate()) {
			if (!IsTileType(t, MP_CLEAR) && !IsTileType(t, MP_TREES)) continue;
			if (IsTileType(t, MP_CLEAR) && IsClearGround(t, CLEAR_FIELDS)) continue;
			uint fence = GB(t.m4(), 5, 3);
			if (fence != 0 && IsTileType(TileAddXY(t, 1, 0), MP_CLEAR) && IsClearGround(TileAddXY(t, 1, 0), CLEAR_FIELDS)) {
				SetFence(TileAddXY(t, 1, 0), DIAGDIR_NE, fence);
			}
			fence = GB(t.m4(), 2, 3);
			if (fence != 0 && IsTileType(TileAddXY(t, 0, 1), MP_CLEAR) && IsClearGround(TileAddXY(t, 0, 1), CLEAR_FIELDS)) {
				SetFence(TileAddXY(t, 0, 1), DIAGDIR_NW, fence);
			}
			SB(t.m4(), 2, 3, 0);
			SB(t.m4(), 5, 3, 0);
		}
	}

	if (IsSavegameVersionBefore(SLV_165)) {
		for (Town *t : Town::Iterate()) {
			/* Set the default cargo requirement for town growth */
			switch (_settings_game.game_creation.landscape) {
				case LandscapeType::Arctic:
					if (FindFirstCargoWithTownAcceptanceEffect(TAE_FOOD) != nullptr) t->goal[TAE_FOOD] = TOWN_GROWTH_WINTER;
					break;

				case LandscapeType::Tropic:
					if (FindFirstCargoWithTownAcceptanceEffect(TAE_FOOD) != nullptr) t->goal[TAE_FOOD] = TOWN_GROWTH_DESERT;
					if (FindFirstCargoWithTownAcceptanceEffect(TAE_WATER) != nullptr) t->goal[TAE_WATER] = TOWN_GROWTH_DESERT;
					break;

				default:
					break;
			}
		}
	}

	if (IsSavegameVersionBefore(SLV_165)) {
		/* Adjust zoom level to account for new levels */
		_saved_scrollpos_zoom = static_cast<ZoomLevel>(_saved_scrollpos_zoom + ZOOM_BASE_SHIFT);
		_saved_scrollpos_x *= ZOOM_BASE;
		_saved_scrollpos_y *= ZOOM_BASE;
	}

	/* When any NewGRF has been changed the availability of some vehicles might
	 * have been changed too. e->company_avail must be set to 0 in that case
	 * which is done by StartupEngines(). */
	if (gcf_res != GLC_ALL_GOOD) StartupEngines();

	/* The road owner of standard road stops was not properly accounted for. */
	if (IsSavegameVersionBefore(SLV_172)) {
		for (const auto t : Map::Iterate()) {
			if (!IsBayRoadStopTile(t)) continue;
			Owner o = GetTileOwner(t);
			SetRoadOwner(t, RTT_ROAD, o);
			SetRoadOwner(t, RTT_TRAM, o);
		}
	}

	if (IsSavegameVersionBefore(SLV_175)) {
		/* Introduced tree planting limit. */
		for (Company *c : Company::Iterate()) c->tree_limit = _settings_game.construction.tree_frame_burst << 16;
	}

	if (IsSavegameVersionBefore(SLV_177)) {
		/* Fix too high inflation rates */
		if (_economy.inflation_prices > MAX_INFLATION) _economy.inflation_prices = MAX_INFLATION;
		if (_economy.inflation_payment > MAX_INFLATION) _economy.inflation_payment = MAX_INFLATION;

		/* We have to convert the quarters of bankruptcy into months of bankruptcy */
		for (Company *c : Company::Iterate()) {
			c->months_of_bankruptcy = 3 * c->months_of_bankruptcy;
		}
	}

	if (IsSavegameVersionBefore(SLV_182)) {
		/* Aircraft acceleration variable was bonkers */
		for (Aircraft *v : Aircraft::Iterate()) {
			if (v->subtype <= AIR_AIRCRAFT) {
				const AircraftVehicleInfo *avi = AircraftVehInfo(v->engine_type);
				v->acceleration = avi->acceleration;
			}
		}

		/* Blocked tiles could be reserved due to a bug, which causes
		 * other places to assert upon e.g. station reconstruction. */
		for (const auto t : Map::Iterate()) {
			if (HasStationTileRail(t) && IsStationTileBlocked(t)) {
				SetRailStationReservation(t, false);
			}
		}
	}

	if (IsSavegameVersionBefore(SLV_184)) {
		/* The global units configuration is split up in multiple configurations. */
		extern uint8_t _old_units;
		_settings_game.locale.units_velocity = Clamp(_old_units, 0, 2);
		_settings_game.locale.units_power    = Clamp(_old_units, 0, 2);
		_settings_game.locale.units_weight   = Clamp(_old_units, 1, 2);
		_settings_game.locale.units_volume   = Clamp(_old_units, 1, 2);
		_settings_game.locale.units_force    = 2;
		_settings_game.locale.units_height   = Clamp(_old_units, 0, 2);
	}

	if (IsSavegameVersionBefore(SLV_VELOCITY_NAUTICAL)) {
		/* Match nautical velocity with land velocity units. */
		_settings_game.locale.units_velocity_nautical = _settings_game.locale.units_velocity;
	}

	if (IsSavegameVersionBefore(SLV_186)) {
		/* Move ObjectType from map to pool */
		for (auto t : Map::Iterate()) {
			if (IsTileType(t, MP_OBJECT)) {
				Object *o = Object::Get(t.m2());
				o->type = t.m5();
				t.m5() = 0; // zero upper bits of (now bigger) ObjectID
			}
		}
	}

	/* Beyond this point, tile types which can be accessed by vehicles must be in a valid state. */

	/* Update all vehicles: Phase 2 */
	AfterLoadVehiclesPhase2(true);

	/* The center of train vehicles was changed, fix up spacing. */
	if (IsSavegameVersionBefore(SLV_164)) FixupTrainLengths();

	/* In version 2.2 of the savegame, we have new airports, so status of all aircraft is reset.
	 * This has to be called after all map array updates */
	if (IsSavegameVersionBefore(SLV_2, 2)) UpdateOldAircraft();

	if (IsSavegameVersionBefore(SLV_188)) {
		/* Fix articulated road vehicles.
		 * Some curves were shorter than other curves.
		 * Now they have the same length, but that means that trailing articulated parts will
		 * take longer to go through the curve than the parts in front which already left the courve.
		 * So, make articulated parts catch up. */
		bool roadside = _settings_game.vehicle.road_side == 1;
		std::vector<uint> skip_frames;
		for (RoadVehicle *v : RoadVehicle::Iterate()) {
			if (!v->IsFrontEngine()) continue;
			skip_frames.clear();
			TileIndex prev_tile = v->tile;
			uint prev_tile_skip = 0;
			uint cur_skip = 0;
			for (RoadVehicle *u = v; u != nullptr; u = u->Next()) {
				if (u->tile != prev_tile) {
					prev_tile_skip = cur_skip;
					prev_tile = u->tile;
				} else {
					cur_skip = prev_tile_skip;
				}

				uint &this_skip = skip_frames.emplace_back(prev_tile_skip);

				/* The following 3 curves now take longer than before */
				switch (u->state) {
					case 2:
						cur_skip++;
						if (u->frame <= (roadside ? 9 : 5)) this_skip = cur_skip;
						break;

					case 4:
						cur_skip++;
						if (u->frame <= (roadside ? 5 : 9)) this_skip = cur_skip;
						break;

					case 5:
						cur_skip++;
						if (u->frame <= (roadside ? 4 : 2)) this_skip = cur_skip;
						break;

					default:
						break;
				}
			}
			while (cur_skip > skip_frames[0]) {
				RoadVehicle *u = v;
				RoadVehicle *prev = nullptr;
				for (uint sf : skip_frames) {
					if (sf >= cur_skip) IndividualRoadVehicleController(u, prev);

					prev = u;
					u = u->Next();
				}
				cur_skip--;
			}
		}
	}

	if (IsSavegameVersionBefore(SLV_190)) {
		for (Order *order : Order::Iterate()) {
			order->SetTravelTimetabled(order->GetTravelTime() > 0);
			order->SetWaitTimetabled(order->GetWaitTime() > 0);
		}
		for (OrderList *orderlist : OrderList::Iterate()) {
			orderlist->RecalculateTimetableDuration();
		}
	}

	/*
	 * Only keep order-backups for network clients (and when replaying).
	 * If we are a network server or not networking, then we just loaded a previously
	 * saved-by-server savegame. There are no clients with a backup, so clear it.
	 * Furthermore before savegame version SLV_192 the actual content was always corrupt.
	 */
	if (!_networking || _network_server || IsSavegameVersionBefore(SLV_192)) {
#ifndef DEBUG_DUMP_COMMANDS
		/* Note: We cannot use CleanPool since that skips part of the destructor
		 * and then leaks un-reachable Orders in the order pool. */
		for (OrderBackup *ob : OrderBackup::Iterate()) {
			delete ob;
		}
#endif
	}

	if (IsSavegameVersionBefore(SLV_198)) {
		/* Convert towns growth_rate and grow_counter to ticks */
		for (Town *t : Town::Iterate()) {
			/* 0x8000 = TOWN_GROWTH_RATE_CUSTOM previously */
			if (t->growth_rate & 0x8000) SetBit(t->flags, TOWN_CUSTOM_GROWTH);
			if (t->growth_rate != TOWN_GROWTH_RATE_NONE) {
				t->growth_rate = TownTicksToGameTicks(t->growth_rate & ~0x8000);
			}
			/* Add t->index % TOWN_GROWTH_TICKS to spread growth across ticks. */
			t->grow_counter = TownTicksToGameTicks(t->grow_counter) + t->index % Ticks::TOWN_GROWTH_TICKS;
		}
	}

	if (IsSavegameVersionBefore(SLV_EXTEND_INDUSTRY_CARGO_SLOTS)) {
		/* Make sure added industry cargo slots are cleared */
		for (Industry *i : Industry::Iterate()) {
			/* Make sure last_cargo_accepted_at is copied to elements for every valid input cargo.
			 * The loading routine should put the original singular value into the first array element. */
			for (auto &a : i->accepted) {
				if (IsValidCargoType(a.cargo)) {
					a.last_accepted = i->GetAccepted(0).last_accepted;
				} else {
					a.last_accepted = EconomyTime::MIN_DATE;
				}
			}
		}
	}

	if (IsSavegameVersionBefore(SLV_SHIPS_STOP_IN_LOCKS)) {
		/* Move ships from lock slope to upper or lower position. */
		for (Ship *s : Ship::Iterate()) {
			/* Suitable tile? */
			if (!IsTileType(s->tile, MP_WATER) || !IsLock(s->tile) || GetLockPart(s->tile) != LOCK_PART_MIDDLE) continue;

			/* We don't need to adjust position when at the tile centre */
			int x = s->x_pos & 0xF;
			int y = s->y_pos & 0xF;
			if (x == 8 && y == 8) continue;

			/* Test if ship is on the second half of the tile */
			bool second_half;
			DiagDirection shipdiagdir = DirToDiagDir(s->direction);
			switch (shipdiagdir) {
				default: NOT_REACHED();
				case DIAGDIR_NE: second_half = x < 8; break;
				case DIAGDIR_NW: second_half = y < 8; break;
				case DIAGDIR_SW: second_half = x > 8; break;
				case DIAGDIR_SE: second_half = y > 8; break;
			}

			DiagDirection slopediagdir = GetInclinedSlopeDirection(GetTileSlope(s->tile));

			/* Heading up slope == passed half way */
			if ((shipdiagdir == slopediagdir) == second_half) {
				/* On top half of lock */
				s->z_pos = GetTileMaxZ(s->tile) * (int)TILE_HEIGHT;
			} else {
				/* On lower half of lock */
				s->z_pos = GetTileZ(s->tile) * (int)TILE_HEIGHT;
			}
		}
	}

	if (IsSavegameVersionBefore(SLV_TOWN_CARGOGEN)) {
		/* Ensure the original cargo generation mode is used */
		_settings_game.economy.town_cargogen_mode = TCGM_ORIGINAL;
	}

	if (IsSavegameVersionBefore(SLV_SERVE_NEUTRAL_INDUSTRIES)) {
		/* Ensure the original neutral industry/station behaviour is used */
		_settings_game.station.serve_neutral_industries = true;

		/* Link oil rigs to their industry and back. */
		for (Station *st : Station::Iterate()) {
			if (IsTileType(st->xy, MP_STATION) && IsOilRig(st->xy)) {
				/* Industry tile is always adjacent during construction by TileDiffXY(0, 1) */
				st->industry = Industry::GetByTile(st->xy + TileDiffXY(0, 1));
				st->industry->neutral_station = st;
			}
		}
	} else {
		/* Link neutral station back to industry, as this is not saved. */
		for (Industry *ind : Industry::Iterate()) if (ind->neutral_station != nullptr) ind->neutral_station->industry = ind;
	}

	if (IsSavegameVersionBefore(SLV_TREES_WATER_CLASS)) {
		/* Update water class for trees. */
		for (const auto t : Map::Iterate()) {
			if (IsTileType(t, MP_TREES)) SetWaterClass(t, GetTreeGround(t) == TREE_GROUND_SHORE ? WATER_CLASS_SEA : WATER_CLASS_INVALID);
		}
	}

	/* Update structures for multitile docks */
	if (IsSavegameVersionBefore(SLV_MULTITILE_DOCKS)) {
		for (const auto t : Map::Iterate()) {
			/* Clear docking tile flag from relevant tiles as it
			 * was not previously cleared. */
			if (IsTileType(t, MP_WATER) || IsTileType(t, MP_RAILWAY) || IsTileType(t, MP_STATION) || IsTileType(t, MP_TUNNELBRIDGE)) {
				SetDockingTile(t, false);
			}
			/* Add docks and oilrigs to Station::ship_station. */
			if (IsTileType(t, MP_STATION)) {
				if (IsDock(t) || IsOilRig(t)) Station::GetByTile(t)->ship_station.Add(t);
			}
		}
	}

	if (IsSavegameVersionBefore(SLV_REPAIR_OBJECT_DOCKING_TILES)) {
		/* Placing objects on docking tiles was not updating adjacent station's docking tiles. */
		for (Station *st : Station::Iterate()) {
			if (st->ship_station.tile != INVALID_TILE) UpdateStationDockingTiles(st);
		}
	}

	/* Make sure all industries exclusive supplier/consumer set correctly. */
	if (IsSavegameVersionBefore(SLV_GS_INDUSTRY_CONTROL)) {
		for (Industry *i : Industry::Iterate()) {
			i->exclusive_supplier = INVALID_OWNER;
			i->exclusive_consumer = INVALID_OWNER;
		}
	}

	if (IsSavegameVersionBefore(SLV_GROUP_REPLACE_WAGON_REMOVAL)) {
		/* Propagate wagon removal flag for compatibility */
		/* Temporary bitmask of company wagon removal setting */
		CompanyMask wagon_removal{};
		for (const Company *c : Company::Iterate()) {
			if (c->settings.renew_keep_length) wagon_removal.Set(c->index);
		}
		for (Group *g : Group::Iterate()) {
			if (g->flags.Any()) {
				/* Convert old replace_protection value to flag. */
				g->flags = GroupFlag::ReplaceProtection;
			}
			if (wagon_removal.Test(g->owner)) g->flags.Set(GroupFlag::ReplaceWagonRemoval);
		}
	}

	/* Use current order time to approximate last loading time */
	if (IsSavegameVersionBefore(SLV_LAST_LOADING_TICK)) {
		for (Vehicle *v : Vehicle::Iterate()) {
			v->last_loading_tick = std::max(TimerGameTick::counter, static_cast<uint64_t>(v->current_order_time)) - v->current_order_time;
		}
	}

	/* Road stops is 'only' updating some caches, but they are needed for PF calls in SLV_MULTITRACK_LEVEL_CROSSINGS teleporting. */
	AfterLoadRoadStops();

	/* Road vehicles stopped on multitrack level crossings need teleporting to a depot
	 * to avoid crashing into the side of the train they're waiting for. */
	if (IsSavegameVersionBefore(SLV_MULTITRACK_LEVEL_CROSSINGS)) {
		/* Teleport road vehicles to the nearest depot. */
		for (RoadVehicle *rv : RoadVehicle::Iterate()) {
			/* Ignore trailers of articulated vehicles. */
			if (rv->IsArticulatedPart()) continue;

			/* Ignore moving vehicles. */
			if (rv->cur_speed > 0) continue;

			/* Ignore crashed vehicles. */
			if (rv->vehstatus.Test(VehState::Crashed)) continue;

			/* Ignore vehicles not on level crossings. */
			TileIndex cur_tile = rv->tile;
			if (!IsLevelCrossingTile(cur_tile)) continue;

			ClosestDepot closest_depot = rv->FindClosestDepot();

			/* Try to find a depot with a distance limit of 512 tiles (Manhattan distance). */
			if (closest_depot.found && DistanceManhattan(rv->tile, closest_depot.location) < 512u) {
				/* Teleport all parts of articulated vehicles. */
				for (RoadVehicle *u = rv; u != nullptr; u = u->Next()) {
					u->tile = closest_depot.location;
					int x = TileX(closest_depot.location) * TILE_SIZE + TILE_SIZE / 2;
					int y = TileY(closest_depot.location) * TILE_SIZE + TILE_SIZE / 2;
					u->x_pos = x;
					u->y_pos = y;
					u->z_pos = GetSlopePixelZ(x, y, true);

					u->vehstatus.Set(VehState::Hidden);
					u->state = RVSB_IN_DEPOT;
					u->UpdatePosition();
				}
				RoadVehLeaveDepot(rv, false);
			}
		}

		if (IsSavegameVersionBeforeOrAt(SLV_MULTITRACK_LEVEL_CROSSINGS)) {
			/* Reset unused tree counters to reduce the savegame size. */
			for (auto t : Map::Iterate()) {
				if (IsTileType(t, MP_TREES)) {
					SB(t.m2(), 0, 4, 0);
				}
			}
		}

		/* Refresh all level crossings to bar adjacent crossing tiles, if needed. */
		for (const auto tile : Map::Iterate()) {
			if (IsLevelCrossingTile(tile)) UpdateLevelCrossing(tile, false);
		}
	}

	/* Compute station catchment areas. This is needed here in case UpdateStationAcceptance is called below. */
	Station::RecomputeCatchmentForAll();

	/* Station acceptance is some kind of cache */
	if (IsSavegameVersionBefore(SLV_127)) {
		for (Station *st : Station::Iterate()) UpdateStationAcceptance(st, false);
	}

	if (IsSavegameVersionBefore(SLV_SAVEGAME_ID)) {
		GenerateSavegameId();
	}

	if (IsSavegameVersionBefore(SLV_AI_START_DATE)) {
		/* For older savegames, we don't now the actual interval; so set it to the newgame value. */
		_settings_game.difficulty.competitors_interval = _settings_newgame.difficulty.competitors_interval;

		/* We did load the "period" of the timer, but not the fired/elapsed. We can deduce that here. */
		extern TimeoutTimer<TimerGameTick> _new_competitor_timeout;
		_new_competitor_timeout.storage.elapsed = 0;
		_new_competitor_timeout.fired = _new_competitor_timeout.period.value == 0;
	}

	if (IsSavegameVersionBefore(SLV_NEWGRF_LAST_SERVICE)) {
		/* Set service date provided to NewGRF. */
		for (Vehicle *v : Vehicle::Iterate()) {
			v->date_of_last_service_newgrf = TimerGameCalendar::Date{v->date_of_last_service.base()};
		}
	}

	if (IsSavegameVersionBefore(SLV_SHIP_ACCELERATION)) {
		/* NewGRF acceleration information was added to ships. */
		for (Ship *s : Ship::Iterate()) {
			if (s->acceleration == 0) s->acceleration = ShipVehInfo(s->engine_type)->acceleration;
		}
	}

	if (IsSavegameVersionBefore(SLV_MAX_LOAN_FOR_COMPANY)) {
		for (Company *c : Company::Iterate()) {
			c->max_loan = COMPANY_MAX_LOAN_DEFAULT;
		}
	}

	if (IsSavegameVersionBefore(SLV_SCRIPT_RANDOMIZER)) {
		ScriptObject::InitializeRandomizers();
	}

	if (IsSavegameVersionBefore(SLV_COMPANY_INAUGURATED_PERIOD_V2)) {
		for (Company *c : Company::Iterate()) {
			c->inaugurated_year_calendar = _settings_game.game_creation.starting_year;
		}
	}

	if (!IsSavegameVersionBefore(SLV_INCREASE_HOUSE_LIMIT) && IsSavegameVersionBeforeOrAt(SLV_SCRIPT_SAVE_INSTANCES)) {
		/* Between these two versions (actually from f8b1e303 to 77236258) EngineFlags had an off-by-one. Depending
		 * on when the save was started, this may or may not affect existing engines. Here we try to detect invalid flags
		 * and reset them to what they should be. */
		for (Engine *e : Engine::Iterate()) {
			if (e->flags.Test(EngineFlag::Available)) continue;
			if (e->flags.Test(EngineFlag{2}) || (e->flags.Test(EngineFlag::ExclusivePreview) && e->preview_asked.None())) {
				e->flags = EngineFlags(e->flags.base() >> 1U);
			}
		}
	}

	for (Company *c : Company::Iterate()) {
		UpdateCompanyLiveries(c);
	}

	/* Update free group numbers data for each company, required regardless of savegame version. */
	for (Group *g : Group::Iterate()) {
		Company *c = Company::Get(g->owner);
		if (IsSavegameVersionBefore(SLV_GROUP_NUMBERS)) {
			/* Use the index as group number when converting old savegames. */
			g->number = c->freegroups.UseID(g->index.base());
		} else {
			c->freegroups.UseID(g->number);
		}
	}

	AfterLoadLabelMaps();
	AfterLoadCompanyStats();
	AfterLoadStoryBook();

	_gamelog.PrintDebug(1);

	InitializeWindowsAndCaches();
	/* Restore the signals */
	ResetSignalHandlers();

	AfterLoadLinkGraphs();
	AfterLoadFindBTProCBInfo();
	citymania::InitializeZoningMap();
	citymania::minimap_init_industries();

	if ((!_networking || _network_server ) && _settings_client.gui.cm_pause_after_load) _pause_mode = PauseMode::Normal;

	CheckGroundVehiclesAtCorrectZ();

	/* Start the scripts. This MUST happen after everything else except
	 * starting a new company. */
	StartScripts();

	/* If Load Scenario / New (Scenario) Game is used,
	 *  a company does not exist yet. So create one here.
	 * 1 exception: network-games. Those can have 0 companies
	 *   But this exception is not true for non-dedicated network servers! */
	if (!_networking || (_networking && _network_server && !_network_dedicated)) {
		CompanyID first_human_company = GetFirstPlayableCompanyID();
		if (!Company::IsValidID(first_human_company)) {
			Company *c = DoStartupNewCompany(false, first_human_company);
			c->settings = _settings_client.company;
		}
	}

	return true;
}

/**
 * Reload all NewGRF files during a running game. This is a cut-down
 * version of AfterLoadGame().
 * XXX - We need to reset the vehicle position hash because with a non-empty
 * hash AfterLoadVehicles() will loop infinitely. We need AfterLoadVehicles()
 * to recalculate vehicle data as some NewGRF vehicle sets could have been
 * removed or added and changed statistics
 */
void ReloadNewGRFData()
{
	/* reload grf data */
	GfxLoadSprites();
	LoadStringWidthTable();
	RecomputePrices();
	/* reload vehicles */
	ResetVehicleHash();
	AfterLoadLabelMaps();
	AfterLoadVehiclesPhase1(false);
	AfterLoadVehiclesPhase2(false);
	StartupEngines();
	GroupStatistics::UpdateAfterLoad();
	/* update station graphics */
	AfterLoadStations();
	/* Update company statistics. */
	AfterLoadCompanyStats();
	/* Check and update house and town values */
	UpdateHousesAndTowns();
	/* Delete news referring to no longer existing entities */
	DeleteInvalidEngineNews();
	/* Update livery selection windows */
	for (CompanyID i = CompanyID::Begin(); i < MAX_COMPANIES; ++i) InvalidateWindowData(WC_COMPANY_COLOUR, i);
	/* Update company infrastructure counts. */
	InvalidateWindowClassesData(WC_COMPANY_INFRASTRUCTURE);
	InvalidateWindowClassesData(WC_BUILD_TOOLBAR);
	InvalidateAllPickerWindows();
	/* redraw the whole screen */
	MarkWholeScreenDirty();
	CheckTrainsLengths();
}
