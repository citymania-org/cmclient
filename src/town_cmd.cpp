/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file town_cmd.cpp Handling of town tiles. */

#include "stdafx.h"
#include "road.h"
#include "road_internal.h" /* Cleaning up road bits */
#include "road_cmd.h"
#include "landscape.h"
#include "viewport_func.h"
#include "viewport_kdtree.h"
#include "command_func.h"
#include "company_func.h"
#include "industry.h"
#include "station_base.h"
#include "waypoint_base.h"
#include "station_kdtree.h"
#include "company_base.h"
#include "news_func.h"
#include "error.h"
#include "object.h"
#include "genworld.h"
#include "newgrf_debug.h"
#include "newgrf_house.h"
#include "newgrf_text.h"
#include "autoslope.h"
#include "tunnelbridge_map.h"
#include "strings_func.h"
#include "window_func.h"
#include "string_func.h"
#include "newgrf_cargo.h"
#include "cheat_type.h"
#include "animated_tile_func.h"
#include "subsidy_func.h"
#include "core/pool_func.hpp"
#include "town.h"
#include "town_kdtree.h"
#include "townname_func.h"
#include "core/random_func.hpp"
#include "core/backup_type.hpp"
#include "depot_base.h"
#include "object_map.h"
#include "object_base.h"
#include "ai/ai.hpp"
#include "game/game.hpp"
#include "town_cmd.h"
#include "landscape_cmd.h"
#include "road_cmd.h"
#include "terraform_cmd.h"
#include "tunnelbridge_cmd.h"
#include "timer/timer.h"
#include "timer/timer_game_calendar.h"
#include "timer/timer_game_economy.h"
#include "timer/timer_game_tick.h"

#include "table/strings.h"
#include "table/town_land.h"

#include "citymania/cm_commands.hpp"
#include "citymania/cm_highlight.hpp"
#include "citymania/cm_main.hpp"

#include "safeguards.h"
#include <optional>

bool _cb_enabled = false;
uint _cb_storage = 0;
uint CBREQ[NUM_CARGO] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};//CB
uint CBFROM[NUM_CARGO] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};//CB
uint CBDECAY[NUM_CARGO] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};//CB
uint days_in_month[] = {31, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};//CB
void CB_UpdateTownStorage(Town *t); //CB


namespace citymania {
	extern TileIndex _closest_cache_ref;
}

/* Initialize the town-pool */
TownPool _town_pool("Town");
INSTANTIATE_POOL_METHODS(Town)


TownKdtree _town_kdtree{};

void RebuildTownKdtree()
{
	std::vector<TownID> townids;
	for (const Town *town : Town::Iterate()) {
		townids.push_back(town->index);
	}
	_town_kdtree.Build(townids.begin(), townids.end());
}

/** Set if a town is being generated. */
static bool _generating_town = false;

/**
 * Check if a town 'owns' a bridge.
 * Bridges do not directly have an owner, so we check the tiles adjacent to the bridge ends.
 * If either adjacent tile belongs to the town then it will be assumed that the town built
 * the bridge.
 * @param tile The bridge tile to test
 * @param t The town we are interested in
 * @return true If town 'owns' a bridge.
 */
static bool TestTownOwnsBridge(TileIndex tile, const Town *t)
{
	if (!IsTileOwner(tile, OWNER_TOWN)) return false;

	TileIndex adjacent = tile + TileOffsByDiagDir(ReverseDiagDir(GetTunnelBridgeDirection(tile)));
	bool town_owned = IsTileType(adjacent, MP_ROAD) && IsTileOwner(adjacent, OWNER_TOWN) && GetTownIndex(adjacent) == t->index;

	if (!town_owned) {
		/* Or other adjacent road */
		adjacent = tile + TileOffsByDiagDir(ReverseDiagDir(GetTunnelBridgeDirection(GetOtherTunnelBridgeEnd(tile))));
		town_owned = IsTileType(adjacent, MP_ROAD) && IsTileOwner(adjacent, OWNER_TOWN) && GetTownIndex(adjacent) == t->index;
	}

	return town_owned;
}

Town::~Town()
{
	if (CleaningPool()) return;

	/* Delete town authority window
	 * and remove from list of sorted towns */
	CloseWindowById(WC_TOWN_VIEW, this->index);

#ifdef WITH_ASSERT
	/* Check no industry is related to us. */
	for (const Industry *i : Industry::Iterate()) {
		assert(i->town != this);
	}

	/* ... and no object is related to us. */
	for (const Object *o : Object::Iterate()) {
		assert(o->town != this);
	}

	/* Check no tile is related to us. */
	for (const auto tile : Map::Iterate()) {
		switch (GetTileType(tile)) {
			case MP_HOUSE:
				assert(GetTownIndex(tile) != this->index);
				break;

			case MP_ROAD:
				assert(!HasTownOwnedRoad(tile) || GetTownIndex(tile) != this->index);
				break;

			case MP_TUNNELBRIDGE:
				assert(!TestTownOwnsBridge(tile, this));
				break;

			default:
				break;
		}
	}
#endif /* WITH_ASSERT */

	/* Clear the persistent storage list. */
	for (auto &psa : this->psa_list) {
		delete psa;
	}
	this->psa_list.clear();

	Source src{this->index, SourceType::Town};
	DeleteSubsidyWith(src);
	DeleteNewGRFInspectWindow(GSF_FAKE_TOWNS, this->index);
	CargoPacket::InvalidateAllFrom(src);
	MarkWholeScreenDirty();
}


/**
 * Invalidating of the "nearest town cache" has to be done
 * after removing item from the pool.
 */
void Town::PostDestructor(size_t)
{
	InvalidateWindowData(WC_TOWN_DIRECTORY, 0, TDIWD_FORCE_REBUILD);
	UpdateNearestTownForRoadTiles(false);

	/* Give objects a new home! */
	for (Object *o : Object::Iterate()) {
		if (o->town == nullptr) o->town = CalcClosestTownFromTile(o->location.tile, UINT_MAX);
	}
}

/**
 * Assign the town layout.
 * @param layout The desired layout. If TL_RANDOM, we pick one based on TileHash.
 */
void Town::InitializeLayout(TownLayout layout)
{
	if (layout != TL_RANDOM) {
		this->layout = layout;
		return;
	}

	this->layout = static_cast<TownLayout>(TileHash(TileX(this->xy), TileY(this->xy)) % (NUM_TLS - 1));
}

/**
 * Return a random valid town.
 * @return A random town, or nullptr if there are no towns.
 */
/* static */ Town *Town::GetRandom()
{
	if (Town::GetNumItems() == 0) return nullptr;
	int num = RandomRange((uint16_t)Town::GetNumItems());
	size_t index = std::numeric_limits<size_t>::max();

	while (num >= 0) {
		num--;
		index++;

		/* Make sure we have a valid town */
		while (!Town::IsValidID(index)) {
			index++;
			assert(index < Town::GetPoolSize());
		}
	}

	return Town::Get(index);
}

 /**
 * Updates the town label of the town after changes in rating. The colour scheme is:
 * Red: Appalling and Very poor ratings.
 * Orange: Poor and mediocre ratings.
 * Yellow: Good rating.
 * White: Very good rating (standard).
 * Green: Excellent and outstanding ratings.
 */
void Town::UpdateLabel()
{
	if (!(_game_mode == GM_EDITOR) && (_local_company < MAX_COMPANIES)) {
		int r = this->ratings[_local_company];
		if     (r < RATING_VERYPOOR) this->cm.town_label = 0; // Appalling and Very Poor, RED
		else if(r < RATING_MEDIOCRE) this->cm.town_label = 1; // Poor and Mediocre, ORANGE
		else if(r < RATING_GOOD)     this->cm.town_label = 2; // Good, YELLOW
		else if(r < RATING_VERYGOOD) this->cm.town_label = 3; // Very Good, WHITE
		else                         this->cm.town_label = 4; // Excellent and Outstanding, GREEN
	}
}

void Town::FillCachedName() const
{
	this->cached_name = GetTownName(this);
}

/**
 * Get the cost for removing this house.
 * @return The cost adjusted for inflation, etc.
 */
Money HouseSpec::GetRemovalCost() const
{
	return (_price[PR_CLEAR_HOUSE] * this->removal_cost) >> 8;
}

/* Local */
static int _grow_town_result;

/* The possible states of town growth. */
enum TownGrowthResult {
	GROWTH_SUCCEED         = -1,
	GROWTH_SEARCH_STOPPED  =  0
//	GROWTH_SEARCH_RUNNING >=  1
};

static bool TryBuildTownHouse(Town *t, TileIndex tile, bool cm_is_rebuilding = false);
static Town *CreateRandomTown(uint attempts, uint32_t townnameparts, TownSize size, bool city, TownLayout layout);

static void TownDrawHouseLift(const TileInfo *ti)
{
	AddChildSpriteScreen(SPR_LIFT, PAL_NONE, 14, 60 - GetLiftPosition(ti->tile));
}

typedef void TownDrawTileProc(const TileInfo *ti);
static TownDrawTileProc * const _town_draw_tile_procs[1] = {
	TownDrawHouseLift
};

/**
 * Return a random direction
 *
 * @return a random direction
 */
static inline DiagDirection RandomDiagDir()
{
	return (DiagDirection)(RandomRange(DIAGDIR_END));
}

/**
 * Draw a house and its tile. This is a tile callback routine.
 * @param ti TileInfo of the tile to draw
 */
static void DrawTile_Town(TileInfo *ti)
{
	HouseID house_id = GetHouseType(ti->tile);

	if (house_id >= NEW_HOUSE_OFFSET) {
		/* Houses don't necessarily need new graphics. If they don't have a
		 * spritegroup associated with them, then the sprite for the substitute
		 * house id is drawn instead. */
		if (HouseSpec::Get(house_id)->grf_prop.GetSpriteGroup() != nullptr) {
			DrawNewHouseTile(ti, house_id);
			return;
		} else {
			house_id = HouseSpec::Get(house_id)->grf_prop.subst_id;
		}
	}

	/* Retrieve pointer to the draw town tile struct */
	const DrawBuildingsTileStruct *dcts = &_town_draw_tile_data[house_id << 4 | TileHash2Bit(ti->x, ti->y) << 2 | GetHouseBuildingStage(ti->tile)];

	if (ti->tileh != SLOPE_FLAT) DrawFoundation(ti, FOUNDATION_LEVELED);

	DrawGroundSprite(dcts->ground.sprite, dcts->ground.pal);

	/* If houses are invisible, do not draw the upper part */
	if (IsInvisibilitySet(TO_HOUSES)) return;

	/* Add a house on top of the ground? */
	SpriteID image = dcts->building.sprite;
	if (image != 0) {
		AddSortableSpriteToDraw(image, dcts->building.pal,
			ti->x + dcts->subtile_x,
			ti->y + dcts->subtile_y,
			dcts->width,
			dcts->height,
			dcts->dz,
			ti->z,
			IsTransparencySet(TO_HOUSES)
		);

		if (IsTransparencySet(TO_HOUSES)) return;
	}

	{
		int proc = dcts->draw_proc - 1;

		if (proc >= 0) _town_draw_tile_procs[proc](ti);
	}
}

static int GetSlopePixelZ_Town(TileIndex tile, uint, uint, bool)
{
	return GetTileMaxPixelZ(tile);
}

/**
 * Get the foundation for a house. This is a tile callback routine.
 * @param tile The tile to find a foundation for.
 * @param tileh The slope of the tile.
 */
static Foundation GetFoundation_Town(TileIndex tile, Slope tileh)
{
	HouseID hid = GetHouseType(tile);

	/* For NewGRF house tiles we might not be drawing a foundation. We need to
	 * account for this, as other structures should
	 * draw the wall of the foundation in this case.
	 */
	if (hid >= NEW_HOUSE_OFFSET) {
		const HouseSpec *hs = HouseSpec::Get(hid);
		if (hs->grf_prop.GetSpriteGroup() != nullptr && hs->callback_mask.Test(HouseCallbackMask::DrawFoundations)) {
			uint32_t callback_res = GetHouseCallback(CBID_HOUSE_DRAW_FOUNDATIONS, 0, 0, hid, Town::GetByTile(tile), tile);
			if (callback_res != CALLBACK_FAILED && !ConvertBooleanCallback(hs->grf_prop.grffile, CBID_HOUSE_DRAW_FOUNDATIONS, callback_res)) return FOUNDATION_NONE;
		}
	}
	return FlatteningFoundation(tileh);
}

/**
 * Animate a tile for a town.
 * Only certain houses can be animated.
 * The newhouses animation supersedes regular ones.
 * @param tile TileIndex of the house to animate.
 */
static void AnimateTile_Town(TileIndex tile)
{
	if (GetHouseType(tile) >= NEW_HOUSE_OFFSET) {
		AnimateNewHouseTile(tile);
		return;
	}

	if (TimerGameTick::counter & 3) return;

	/* If the house is not one with a lift anymore, then stop this animating.
	 * Not exactly sure when this happens, but probably when a house changes.
	 * Before this was just a return...so it'd leak animated tiles..
	 * That bug seems to have been here since day 1?? */
	if (!HouseSpec::Get(GetHouseType(tile))->building_flags.Test(BuildingFlag::IsAnimated)) {
		DeleteAnimatedTile(tile);
		return;
	}

	if (!LiftHasDestination(tile)) {
		uint i;

		/* Building has 6 floors, number 0 .. 6, where 1 is illegal.
		 * This is due to the fact that the first floor is, in the graphics,
		 *  the height of 2 'normal' floors.
		 * Furthermore, there are 6 lift positions from floor N (incl) to floor N + 1 (excl) */
		do {
			i = RandomRange(7);
		} while (i == 1 || i * 6 == GetLiftPosition(tile));

		SetLiftDestination(tile, i);
	}

	int pos = GetLiftPosition(tile);
	int dest = GetLiftDestination(tile) * 6;
	pos += (pos < dest) ? 1 : -1;
	SetLiftPosition(tile, pos);

	if (pos == dest) {
		HaltLift(tile);
		DeleteAnimatedTile(tile);
	}

	MarkTileDirtyByTile(tile);
}

/**
 * Determines if a town is close to a tile.
 * @param tile TileIndex of the tile to query.
 * @param dist The maximum distance to be accepted.
 * @returns true if the tile is within the specified distance.
 */
static bool IsCloseToTown(TileIndex tile, uint dist)
{
	if (_town_kdtree.Count() == 0) return false;
	Town *t = Town::Get(_town_kdtree.FindNearest(TileX(tile), TileY(tile)));
	return DistanceManhattan(tile, t->xy) < dist;
}

/** Resize the sign (label) of the town after it changes population. */
void Town::UpdateVirtCoord()
{
	this->UpdateLabel();
	Point pt = RemapCoords2(TileX(this->xy) * TILE_SIZE, TileY(this->xy) * TILE_SIZE);

	if (this->cache.sign.kdtree_valid) _viewport_sign_kdtree.Remove(ViewportSignKdtreeItem::MakeTown(this->index));

	std::string town_string = GetString(STR_TOWN_NAME, this->index);
	this->cache.sign.UpdatePosition(pt.x, pt.y - 24 * ZOOM_BASE,
		_settings_client.gui.population_in_label ? GetString(STR_VIEWPORT_TOWN_POP, this->index, this->cache.population) : town_string,
		town_string);

	_viewport_sign_kdtree.Insert(ViewportSignKdtreeItem::MakeTown(this->index));

	SetWindowDirty(WC_TOWN_VIEW, this->index);
	SetWindowDirty(CM_WC_CB_TOWN, this->index);
}

/** Update the virtual coords needed to draw the town sign for all towns. */
void UpdateAllTownVirtCoords()
{
	for (Town *t : Town::Iterate()) {
		t->UpdateVirtCoord();
	}
}

/** Clear the cached_name of all towns. */
void ClearAllTownCachedNames()
{
	for (Town *t : Town::Iterate()) {
		t->cached_name.clear();
	}
}

/**
 * Change the town's population as recorded in the town cache, town label, and town directory.
 * @param t The town which has changed.
 * @param mod The population change (can be positive or negative).
 */
static void ChangePopulation(Town *t, int mod)
{
	t->cache.population += mod;
	if (_generating_town) [[unlikely]] return;

	InvalidateWindowData(WC_TOWN_VIEW, t->index); // Cargo requirements may appear/vanish for small populations
	if (_settings_client.gui.population_in_label) t->UpdateVirtCoord();

	InvalidateWindowData(WC_TOWN_DIRECTORY, 0, TDIWD_POPULATION_CHANGE);
}

/**
 * Get the total population, the sum of all towns in the world.
 * @return The calculated population of the world
 */
uint32_t GetWorldPopulation()
{
	uint32_t pop = 0;
	for (const Town *t : Town::Iterate()) pop += t->cache.population;
	return pop;
}

/**
 * Remove stations from nearby station list if a town is no longer in the catchment area of each.
 * To improve performance only checks stations that cover the provided house area (doesn't need to contain an actual house).
 * @param t Town to work on.
 * @param tile Location of house area (north tile).
 * @param flags BuildingFlags containing the size of house area.
 */
static void RemoveNearbyStations(Town *t, TileIndex tile, BuildingFlags flags)
{
	for (StationList::iterator it = t->stations_near.begin(); it != t->stations_near.end(); /* incremented inside loop */) {
		const Station *st = *it;

		bool covers_area = st->TileIsInCatchment(tile);
		if (flags.Any(BUILDING_2_TILES_Y))   covers_area |= st->TileIsInCatchment(tile + TileDiffXY(0, 1));
		if (flags.Any(BUILDING_2_TILES_X))   covers_area |= st->TileIsInCatchment(tile + TileDiffXY(1, 0));
		if (flags.Any(BUILDING_HAS_4_TILES)) covers_area |= st->TileIsInCatchment(tile + TileDiffXY(1, 1));

		if (covers_area && !st->CatchmentCoversTown(t->index)) {
			it = t->stations_near.erase(it);
		} else {
			++it;
		}
	}
}

/**
 * Helper function for house construction stage progression.
 * @param tile TileIndex of the house (or parts of it) to construct.
 */
static void AdvanceSingleHouseConstruction(TileIndex tile)
{
	assert(IsTileType(tile, MP_HOUSE));

	/* Progress in construction stages */
	IncHouseConstructionTick(tile);
	if (GetHouseConstructionTick(tile) != 0) return;

	AnimateNewHouseConstruction(tile);

	if (IsHouseCompleted(tile)) {
		/* Now that construction is complete, we can add the population of the
		 * building to the town. */
		HouseID house_id = GetHouseType(tile);
		auto hs = HouseSpec::Get(house_id);
		Town *town = Town::GetByTile(tile);
		ChangePopulation(Town::GetByTile(tile), hs->population);
		ResetHouseAge(tile);

		if (hs->building_flags.Any(BUILDING_HAS_1_TILE))
			citymania::Emit(citymania::event::HouseCompleted{town, tile, house_id, hs});
	}
	MarkTileDirtyByTile(tile);
}

/**
 * Increase the construction stage of a house.
 * @param tile The tile of the house under construction.
 */
static void AdvanceHouseConstruction(TileIndex tile)
{
	BuildingFlags flags = HouseSpec::Get(GetHouseType(tile))->building_flags;
	if (flags.Any(BUILDING_HAS_1_TILE))  AdvanceSingleHouseConstruction(TileAddXY(tile, 0, 0));
	if (flags.Any(BUILDING_2_TILES_Y))   AdvanceSingleHouseConstruction(TileAddXY(tile, 0, 1));
	if (flags.Any(BUILDING_2_TILES_X))   AdvanceSingleHouseConstruction(TileAddXY(tile, 1, 0));
	if (flags.Any(BUILDING_HAS_4_TILES)) AdvanceSingleHouseConstruction(TileAddXY(tile, 1, 1));
}

/**
 * Generate cargo for a house, scaled by the current economy scale.
 * @param t The current town.
 * @param ct Type of cargo to generate, usually CT_PASSENGERS or CT_MAIL.
 * @param amount The number of cargo units.
 * @param stations Available stations for this house.
 * @param affected_by_recession Is this cargo halved during recessions?
 */
static void TownGenerateCargo(Town *t, CargoType cargo, uint amount, StationFinder &stations, bool affected_by_recession)
{
	if (amount == 0) return;

	/* All production is halved during a recession (except for NewGRF-supplied town cargo). */
	if (affected_by_recession && EconomyIsInRecession()) {
		amount = (amount + 1) >> 1;
	}

	/* Scale by cargo scale setting. */
	amount = ScaleByCargoScale(amount, true);

	/* Actually generate cargo and update town statistics. */
	t->supplied[cargo].new_max += amount;
	t->supplied[cargo].new_act += MoveGoodsToStation(cargo, amount, {t->index, SourceType::Town}, stations.GetStations());;
}

/**
 * Generate cargo for a house using the original algorithm.
 * @param t The current town.
 * @param tpe The town production effect.
 * @param rate The town's product rate for this production.
 * @param stations Available stations for this house.
 */
static void TownGenerateCargoOriginal(Town *t, TownProductionEffect tpe, uint8_t rate, StationFinder &stations)
{
	for (const CargoSpec *cs : CargoSpec::town_production_cargoes[tpe]) {
		uint32_t r = Random();
		if (GB(r, 0, 8) < rate) {
			CargoType cargo_type = cs->Index();
			uint amt = (GB(r, 0, 8) * cs->town_production_multiplier / TOWN_PRODUCTION_DIVISOR) / 8 + 1;

			TownGenerateCargo(t, cargo_type, amt, stations, true);
		}
	}
}

/**
 * Generate cargo for a house using the binomial algorithm.
 * @param t The current town.
 * @param tpe The town production effect.
 * @param rate The town's product rate for this production.
 * @param stations Available stations for this house.
 */
static void TownGenerateCargoBinomial(Town *t, TownProductionEffect tpe, uint8_t rate, StationFinder &stations)
{
	for (const CargoSpec *cs : CargoSpec::town_production_cargoes[tpe]) {
		CargoType cargo_type = cs->Index();
		uint32_t r = Random();

		/* Make a bitmask with up to 32 bits set, one for each potential pax. */
		int genmax = (rate + 7) / 8;
		uint32_t genmask = (genmax >= 32) ? 0xFFFFFFFF : ((1 << genmax) - 1);

		/* Mask random value by potential pax and count number of actual pax. */
		uint amt = CountBits(r & genmask) * cs->town_production_multiplier / TOWN_PRODUCTION_DIVISOR;

		TownGenerateCargo(t, cargo_type, amt, stations, true);
	}
}

/**
 * Tile callback function.
 *
 * Tile callback function. Periodic tick handler for the tiles of a town.
 * @param tile been asked to do its stuff
 */
static void TileLoop_Town(TileIndex tile)
{
	HouseID house_id = GetHouseType(tile);

	/* NewHouseTileLoop returns false if Callback 21 succeeded, i.e. the house
	 * doesn't exist any more, so don't continue here. */
	if (house_id >= NEW_HOUSE_OFFSET && !NewHouseTileLoop(tile)) return;

	if (!IsHouseCompleted(tile)) {
		/* Construction is not completed, so we advance a construction stage. */
		AdvanceHouseConstruction(tile);
		return;
	}

	const HouseSpec *hs = HouseSpec::Get(house_id);

	/* If the lift has a destination, it is already an animated tile. */
	if (hs->building_flags.Test(BuildingFlag::IsAnimated) &&
			house_id < NEW_HOUSE_OFFSET &&
			!LiftHasDestination(tile) &&
			Chance16(1, 2)) {
		AddAnimatedTile(tile);
	}

	Town *t = Town::GetByTile(tile);
	uint32_t r = Random();

	StationFinder stations(TileArea(tile, 1, 1));

	if (hs->callback_mask.Test(HouseCallbackMask::ProduceCargo)) {
		for (uint i = 0; i < 256; i++) {
			uint16_t callback = GetHouseCallback(CBID_HOUSE_PRODUCE_CARGO, i, r, house_id, t, tile);

			if (callback == CALLBACK_FAILED || callback == CALLBACK_HOUSEPRODCARGO_END) break;

			CargoType cargo = GetCargoTranslation(GB(callback, 8, 7), hs->grf_prop.grffile);
			if (!IsValidCargoType(cargo)) continue;

			uint amt = GB(callback, 0, 8);
			if (amt == 0) continue;

			/* NewGRF-supplied town cargos are not affected by recessions. */
			TownGenerateCargo(t, cargo, amt, stations, false);
		}
	} else {
		switch (_settings_game.economy.town_cargogen_mode) {
			case TCGM_ORIGINAL:
				/* Original (quadratic) cargo generation algorithm */
				TownGenerateCargoOriginal(t, TPE_PASSENGERS, hs->population, stations);
				TownGenerateCargoOriginal(t, TPE_MAIL, hs->mail_generation, stations);
				break;

			case TCGM_BITCOUNT:
				/* Binomial distribution per tick, by a series of coin flips */
				/* Reduce generation rate to a 1/4, using tile bits to spread out distribution.
				 * As tick counter is incremented by 256 between each call, we ignore the lower 8 bits. */
				if (GB(TimerGameTick::counter, 8, 2) == GB(tile.base(), 0, 2)) {
					TownGenerateCargoBinomial(t, TPE_PASSENGERS, hs->population, stations);
					TownGenerateCargoBinomial(t, TPE_MAIL, hs->mail_generation, stations);
				}
				break;

			default:
				NOT_REACHED();
		}
	}

	Backup<CompanyID> cur_company(_current_company, OWNER_TOWN);

	if (hs->building_flags.Any(BUILDING_HAS_1_TILE) &&
			HasBit(t->flags, TOWN_IS_GROWING) &&
			CanDeleteHouse(tile) &&
			GetHouseAge(tile) >= hs->minimum_life &&
			--t->time_until_rebuild == 0) {
		t->time_until_rebuild = GB(r, 16, 8) + 192;

		ClearTownHouse(t, tile);

		/* Rebuild with another house? */
		bool rebuild_res = false;
		if (GB(r, 24, 8) >= 12) {
			/* If we are multi-tile houses, make sure to replace the house
			 * closest to city center. If we do not do this, houses tend to
			 * wander away from roads and other houses. */
			if (hs->building_flags.Any(BUILDING_HAS_2_TILES)) {
				/* House tiles are always the most north tile. Move the new
				 * house to the south if we are north of the city center. */
				TileIndexDiffC grid_pos = TileIndexToTileIndexDiffC(t->xy, tile);
				int x = Clamp(grid_pos.x, 0, 1);
				int y = Clamp(grid_pos.y, 0, 1);

				if (hs->building_flags.Test(BuildingFlag::Size2x2)) {
					tile = TileAddXY(tile, x, y);
				} else if (hs->building_flags.Test(BuildingFlag::Size1x2)) {
					tile = TileAddXY(tile, 0, y);
				} else if (hs->building_flags.Test(BuildingFlag::Size2x1)) {
					tile = TileAddXY(tile, x, 0);
				}
			}

			rebuild_res = TryBuildTownHouse(t, tile);
		}
		citymania::Emit(citymania::event::HouseRebuilt{t, tile, rebuild_res});
	}

	cur_company.Restore();
}

/**
 * Callback function to clear a house tile.
 * @param tile The tile to clear.
 * @param flags Type of operation.
 * @return The cost of this operation or an error.
 */
static CommandCost ClearTile_Town(TileIndex tile, DoCommandFlags flags)
{
	if (flags.Test(DoCommandFlag::Auto)) return CommandCost(STR_ERROR_BUILDING_MUST_BE_DEMOLISHED);
	if (!CanDeleteHouse(tile)) return CMD_ERROR;

	const HouseSpec *hs = HouseSpec::Get(GetHouseType(tile));

	CommandCost cost(EXPENSES_CONSTRUCTION);
	cost.AddCost(hs->GetRemovalCost());

	int rating = hs->remove_rating_decrease;
	Town *t = Town::GetByTile(tile);

	if (Company::IsValidID(_current_company)) {
		if (!_cheats.magic_bulldozer.value && !flags.Test(DoCommandFlag::NoTestTownRating)) {
			/* NewGRFs can add indestructible houses. */
			if (rating > RATING_MAXIMUM) {
				return CommandCost(CMD_ERROR);
			}
			/* If town authority controls removal, check the company's rating. */
			if (rating > t->ratings[_current_company] && _settings_game.difficulty.town_council_tolerance != TOWN_COUNCIL_PERMISSIVE) {
				return CommandCostWithParam(STR_ERROR_LOCAL_AUTHORITY_REFUSES_TO_ALLOW_THIS, t->index);
			}
		}
	}

	ChangeTownRating(t, -rating, RATING_HOUSE_MINIMUM, flags);
	if (flags.Test(DoCommandFlag::Execute)) {
		ClearTownHouse(t, tile);
		citymania::Emit(citymania::event::HouseDestroyed{_current_company, t, tile});
	}

	return cost;
}

static void AddProducedCargo_Town(TileIndex tile, CargoArray &produced)
{
	HouseID house_id = GetHouseType(tile);
	const HouseSpec *hs = HouseSpec::Get(house_id);
	Town *t = Town::GetByTile(tile);

	if (hs->callback_mask.Test(HouseCallbackMask::ProduceCargo)) {
		for (uint i = 0; i < 256; i++) {
			uint16_t callback = GetHouseCallback(CBID_HOUSE_PRODUCE_CARGO, i, 0, house_id, t, tile);

			if (callback == CALLBACK_FAILED || callback == CALLBACK_HOUSEPRODCARGO_END) break;

			CargoType cargo = GetCargoTranslation(GB(callback, 8, 7), hs->grf_prop.grffile);

			if (!IsValidCargoType(cargo)) continue;
			produced[cargo]++;
		}
	} else {
		if (hs->population > 0) {
			for (const CargoSpec *cs : CargoSpec::town_production_cargoes[TPE_PASSENGERS]) {
				produced[cs->Index()]++;
			}
		}
		if (hs->mail_generation > 0) {
			for (const CargoSpec *cs : CargoSpec::town_production_cargoes[TPE_MAIL]) {
				produced[cs->Index()]++;
			}
		}
	}
}

/**
 * Fill cargo acceptance array and always_accepted mask, if cargo type is valid.
 * @param cargo Cargo type to add.
 * @param amount Amount of cargo to add.
 * @param[out] acceptance Output array containing amount of cargo accepted.
 * @param[out] always_accepted Output mask of accepted cargo types.
 */
static void AddAcceptedCargoSetMask(CargoType cargo, uint amount, CargoArray &acceptance, CargoTypes &always_accepted)
{
	if (!IsValidCargoType(cargo) || amount == 0) return;
	acceptance[cargo] += amount;
	SetBit(always_accepted, cargo);
}

/**
 * Determine accepted cargo for a house.
 * @param tile Tile of house, or INVALID_TILE if not yet built.
 * @param house HouseID of house.
 * @param hs HouseSpec of house.
 * @param t Town that house belongs to, or nullptr if not yet built.
 * @param[out] acceptance CargoArray to be filled with acceptance information.
 * @param[out] always_accepted Bitmask of always accepted cargo types
 */
void AddAcceptedCargoOfHouse(TileIndex tile, HouseID house, const HouseSpec *hs, Town *t, CargoArray &acceptance, CargoTypes &always_accepted)
{
	CargoType accepts[lengthof(hs->accepts_cargo)];

	/* Set the initial accepted cargo types */
	for (uint8_t i = 0; i < lengthof(accepts); i++) {
		accepts[i] = hs->accepts_cargo[i];
	}

	/* Check for custom accepted cargo types */
	if (hs->callback_mask.Test(HouseCallbackMask::AcceptCargo)) {
		uint16_t callback = GetHouseCallback(CBID_HOUSE_ACCEPT_CARGO, 0, 0, house, t, tile, tile == INVALID_TILE);
		if (callback != CALLBACK_FAILED) {
			/* Replace accepted cargo types with translated values from callback */
			accepts[0] = GetCargoTranslation(GB(callback,  0, 5), hs->grf_prop.grffile);
			accepts[1] = GetCargoTranslation(GB(callback,  5, 5), hs->grf_prop.grffile);
			accepts[2] = GetCargoTranslation(GB(callback, 10, 5), hs->grf_prop.grffile);
		}
	}

	/* Check for custom cargo acceptance */
	if (hs->callback_mask.Test(HouseCallbackMask::CargoAcceptance)) {
		uint16_t callback = GetHouseCallback(CBID_HOUSE_CARGO_ACCEPTANCE, 0, 0, house, t, tile, tile == INVALID_TILE);
		if (callback != CALLBACK_FAILED) {
			AddAcceptedCargoSetMask(accepts[0], GB(callback, 0, 4), acceptance, always_accepted);
			AddAcceptedCargoSetMask(accepts[1], GB(callback, 4, 4), acceptance, always_accepted);
			if (_settings_game.game_creation.landscape != LandscapeType::Temperate && HasBit(callback, 12)) {
				/* The 'S' bit indicates food instead of goods */
				AddAcceptedCargoSetMask(GetCargoTypeByLabel(CT_FOOD), GB(callback, 8, 4), acceptance, always_accepted);
			} else {
				AddAcceptedCargoSetMask(accepts[2], GB(callback, 8, 4), acceptance, always_accepted);
			}
			return;
		}
	}

	/* No custom acceptance, so fill in with the default values */
	for (uint8_t i = 0; i < lengthof(accepts); i++) {
		AddAcceptedCargoSetMask(accepts[i], hs->cargo_acceptance[i], acceptance, always_accepted);
	}
}

static void AddAcceptedCargo_Town(TileIndex tile, CargoArray &acceptance, CargoTypes &always_accepted)
{
	HouseID house = GetHouseType(tile);
	AddAcceptedCargoOfHouse(tile, house, HouseSpec::Get(house), Town::GetByTile(tile), acceptance, always_accepted);
}

/**
 * Get accepted cargo of a house prototype.
 * @param hs Spec of the house.
 * @return CargoArray filled with cargo accepted by the house.
 */
CargoArray GetAcceptedCargoOfHouse(const HouseSpec *hs)
{
	CargoTypes always_accepted{};
	CargoArray acceptance{};
	AddAcceptedCargoOfHouse(INVALID_TILE, hs->Index(), hs, nullptr, acceptance, always_accepted);
	return acceptance;
}

static void GetTileDesc_Town(TileIndex tile, TileDesc &td)
{
	const HouseID house = GetHouseType(tile);
	const HouseSpec *hs = HouseSpec::Get(house);
	bool house_completed = IsHouseCompleted(tile);

	td.str = hs->building_name;
	td.town_can_upgrade = !IsHouseProtected(tile);
	td.cm_population = hs->population;

	uint16_t callback_res = GetHouseCallback(CBID_HOUSE_CUSTOM_NAME, house_completed ? 1 : 0, 0, house, Town::GetByTile(tile), tile);
	if (callback_res != CALLBACK_FAILED && callback_res != 0x400) {
		if (callback_res > 0x400) {
			ErrorUnknownCallbackResult(hs->grf_prop.grfid, CBID_HOUSE_CUSTOM_NAME, callback_res);
		} else {
			StringID new_name = GetGRFStringID(hs->grf_prop.grfid, GRFSTR_MISC_GRF_TEXT + callback_res);
			if (new_name != STR_NULL && new_name != STR_UNDEFINED) {
				td.str = new_name;
			}
		}
	}

	if (!house_completed) {
		td.dparam = td.str;
		td.str = STR_LAI_TOWN_INDUSTRY_DESCRIPTION_UNDER_CONSTRUCTION;
	}

	if (hs->grf_prop.HasGrfFile()) {
		const GRFConfig *gc = GetGRFConfig(hs->grf_prop.grfid);
		td.grf = gc->GetName();
	}

	td.owner[0] = OWNER_TOWN;
}

static TrackStatus GetTileTrackStatus_Town(TileIndex, TransportType, uint, DiagDirection)
{
	/* not used */
	return 0;
}

static void ChangeTileOwner_Town(TileIndex, Owner, Owner)
{
	/* not used */
}

static bool GrowTown(Town *t);

bool TownNeedsFunding(Town *t) {
	bool fund_regularly = t->cm.fund_regularly.Test(_local_company);
	bool do_powerfund = t->cm.do_powerfund.Test(_local_company);

	if (do_powerfund && (_settings_client.gui.cm_powerfund_money > Company::Get(_local_company)->money ||
		    _settings_client.gui.cm_powerfund_houses < t->cache.num_houses)) {
	    do_powerfund = false;
	}

	if (!fund_regularly && !do_powerfund)
		return false;


	bool is_growing = HasBit(t->flags, TOWN_IS_GROWING);

	if (HasBit(t->flags, TOWN_CUSTOM_GROWTH)) {
		if (!is_growing)
			return false;

		if (do_powerfund && t->grow_counter > 2 * Ticks::TOWN_GROWTH_TICKS)
			return true;

		return (fund_regularly &&
		        t->fund_buildings_months == 0 && (
			     	t->growth_rate <= 2 * Ticks::TOWN_GROWTH_TICKS ||
                	t->grow_counter > 2 * Ticks::TOWN_GROWTH_TICKS
                ));
	}

	if (!is_growing)
		return true;

	if (do_powerfund && t->grow_counter > 2 * Ticks::TOWN_GROWTH_TICKS)
		return true;

	return (fund_regularly &&
	        t->fund_buildings_months == 0 &&
		    t->growth_rate >= Ticks::TOWN_GROWTH_TICKS && (
		    	t->growth_rate <= 2 * Ticks::TOWN_GROWTH_TICKS ||
            	t->grow_counter > 2 * Ticks::TOWN_GROWTH_TICKS
            ));
}

static void DoRegularFunding(Town *t)
{
	if (_local_company == COMPANY_SPECTATOR)
		return;

	if (!TownNeedsFunding(t))
		return;

	/* never fund faster than every Ticks::TOWN_GROWTH_TICKS */
	if (TimerGameTick::counter < t->cm.last_funding) {
		if (UINT32_MAX - t->cm.last_funding + TimerGameTick::counter < Ticks::TOWN_GROWTH_TICKS) return;
	} else if (TimerGameTick::counter - t->cm.last_funding < Ticks::TOWN_GROWTH_TICKS) return;

	citymania::cmd::DoTownAction(t->xy, t->index, TownAction::FundBuildings)
		.no_estimate()
		.as_company(_local_company)
		.post();
	t->cm.last_funding = TimerGameTick::counter;
}

static void DoRegularAdvertising(Town *t) {
	if (t->cm.advertise_regularly.Test(_local_company))
		return;

	if (!t->cm.ad_ref_goods_entry.has_value()) {
		// Pick as ref station and cargo with min rating
		uint8_t rating = 0;
		for (Station *st : Station::Iterate()) {
			if (st->owner == _local_company && DistanceManhattan(t->xy, st->xy) <= 20) {
				for (CargoType i = 0; i < NUM_CARGO; i++)
					if (st->goods[i].HasRating() && (t->cm.ad_ref_goods_entry == std::nullopt ||
					    	rating < st->goods[i].rating)) {
						t->cm.ad_ref_goods_entry = {st->index, i};
						rating = st->goods[i].rating;
					}
			}
		}

		if (!t->cm.ad_ref_goods_entry.has_value())
			return;
	}

	auto [station_id, cargo_type] = t->cm.ad_ref_goods_entry.value();
	auto st = Station::GetIfValid(station_id);
	if (st == nullptr) {
		t->cm.ad_ref_goods_entry = std::nullopt;
		return;
	}
	auto rating = st->goods[cargo_type].rating;
	if (rating >= t->cm.ad_rating_goal)
		return;

	// don't advertise faster that once per 30 ticks
	if (TimerGameTick::counter < t->cm.last_advertisement) {
		if (UINT32_MAX - t->cm.last_advertisement + TimerGameTick::counter < 30) return;
	} else if (TimerGameTick::counter - t->cm.last_advertisement < 30) return;
	t->cm.last_advertisement = TimerGameTick::counter;

	citymania::cmd::DoTownAction(t->xy, t->index, TownAction::AdvertiseLarge)
		.no_estimate()
		.as_company(_local_company)
		.with_callback([=] (bool res) -> bool {
			if (!res) return true;
			if (!t->cm.ad_ref_goods_entry.has_value()) return true;
			auto [station_id, cargo_type] = t->cm.ad_ref_goods_entry.value();
			auto st = Station::GetIfValid(station_id);
			if (st == nullptr) return true;
			if (rating == st->goods[cargo_type].rating) {
				t->cm.ad_ref_goods_entry = std::nullopt;
			}
			return true;
		}).post();
}

/**
 * Handle the town tick for a single town, by growing the town if desired.
 * @param t The town to try growing.
 */
static void TownTickHandler(Town *t)
{
	if (HasBit(t->flags, TOWN_IS_GROWING)) {
		int i = (int)t->grow_counter - 1;
		if (i < 0) {
			if (GrowTown(t)) {
				i = t->growth_rate;
			} else {
				/* If growth failed wait a bit before retrying */
				i = std::min<uint16_t>(t->growth_rate, Ticks::TOWN_GROWTH_TICKS - 1);
			}
		}
		t->grow_counter = i;
	}
	DoRegularFunding(t);
	DoRegularAdvertising(t);
}

/** Iterate through all towns and call their tick handler. */
void OnTick_Town()
{
	if (_game_mode == GM_EDITOR) return;

	for (Town *t : Town::Iterate()) {
		TownTickHandler(t);
	}
}

/**
 * Return the RoadBits of a tile, ignoring depot and bay road stops.
 * @param tile The tile to check.
 * @return The roadbits of the given tile.
 */
static RoadBits GetTownRoadBits(TileIndex tile)
{
	if (IsRoadDepotTile(tile) || IsBayRoadStopTile(tile)) return ROAD_NONE;

	return GetAnyRoadBits(tile, RTT_ROAD, true);
}

/**
 * Get the road type that towns should build at this current moment.
 * They may have built a different type in the past.
 */
RoadType GetTownRoadType()
{
	RoadType best_rt = ROADTYPE_ROAD;
	const RoadTypeInfo *best = nullptr;
	const uint16_t assume_max_speed = 50;

	for (RoadType rt = ROADTYPE_BEGIN; rt != ROADTYPE_END; rt++) {
		if (RoadTypeIsTram(rt)) continue;

		const RoadTypeInfo *rti = GetRoadTypeInfo(rt);

		/* Unused road type. */
		if (rti->label == 0) continue;

		/* Can town build this road. */
		if (!rti->flags.Test(RoadTypeFlag::TownBuild)) continue;

		/* Not yet introduced at this date. */
		if (IsInsideMM(rti->introduction_date, 0, CalendarTime::MAX_DATE.base()) && rti->introduction_date > TimerGameCalendar::date) continue;

		if (best != nullptr) {
			if ((rti->max_speed == 0 ? assume_max_speed : rti->max_speed) < (best->max_speed == 0 ? assume_max_speed : best->max_speed)) continue;
		}

		best_rt = rt;
		best = rti;
	}

	return best_rt;
}

/**
 * Get the calendar date of the earliest town-buildable road type.
 * @return introduction date of earliest road type, or INT32_MAX if none available.
 */
static TimerGameCalendar::Date GetTownRoadTypeFirstIntroductionDate()
{
	const RoadTypeInfo *best = nullptr;
	for (RoadType rt = ROADTYPE_BEGIN; rt != ROADTYPE_END; rt++) {
		if (RoadTypeIsTram(rt)) continue;
		const RoadTypeInfo *rti = GetRoadTypeInfo(rt);
		if (rti->label == 0) continue; // Unused road type.
		if (!rti->flags.Test(RoadTypeFlag::TownBuild)) continue; // Town can't build this road type.

		if (best != nullptr && rti->introduction_date >= best->introduction_date) continue;
		best = rti;
	}

	if (best == nullptr) return TimerGameCalendar::Date(INT32_MAX);
	return best->introduction_date;
}

/**
 * Check if towns are able to build road.
 * @return true iff the towns are currently able to build road.
 */
bool CheckTownRoadTypes()
{
	auto min_date = GetTownRoadTypeFirstIntroductionDate();
	if (min_date <= TimerGameCalendar::date) return true;

	if (min_date < INT32_MAX) {
		ShowErrorMessage(
			GetEncodedString(STR_ERROR_NO_TOWN_ROADTYPES_AVAILABLE_YET),
			GetEncodedString(STR_ERROR_NO_TOWN_ROADTYPES_AVAILABLE_YET_EXPLANATION, min_date),
			WL_CRITICAL);
	} else {
		ShowErrorMessage(
			GetEncodedString(STR_ERROR_NO_TOWN_ROADTYPES_AVAILABLE_AT_ALL),
			GetEncodedString(STR_ERROR_NO_TOWN_ROADTYPES_AVAILABLE_AT_ALL_EXPLANATION), WL_CRITICAL);
	}
	return false;
}

/**
 * Check for parallel road inside a given distance.
 *   Assuming a road from (tile - TileOffsByDiagDir(dir)) to tile,
 *   is there a parallel road left or right of it within distance dist_multi?
 *
 * @param tile The current tile.
 * @param dir The target direction.
 * @param dist_multi The distance multiplier.
 * @return true if there is a parallel road.
 */
static bool IsNeighbourRoadTile(TileIndex tile, const DiagDirection dir, uint dist_multi)
{
	if (!IsValidTile(tile)) return false;

	/* Lookup table for the used diff values */
	const TileIndexDiff tid_lt[3] = {
		TileOffsByDiagDir(ChangeDiagDir(dir, DIAGDIRDIFF_90RIGHT)),
		TileOffsByDiagDir(ChangeDiagDir(dir, DIAGDIRDIFF_90LEFT)),
		TileOffsByDiagDir(ReverseDiagDir(dir)),
	};

	dist_multi = (dist_multi + 1) * 4;
	for (uint pos = 4; pos < dist_multi; pos++) {
		/* Go (pos / 4) tiles to the left or the right */
		TileIndexDiff cur = tid_lt[(pos & 1) ? 0 : 1] * (pos / 4);

		/* Use the current tile as origin, or go one tile backwards */
		if (pos & 2) cur += tid_lt[2];

		/* Test for roadbit parallel to dir and facing towards the middle axis */
		if (IsValidTile(tile + cur) &&
				GetTownRoadBits(TileAdd(tile, cur)) & DiagDirToRoadBits((pos & 2) ? dir : ReverseDiagDir(dir))) return true;
	}
	return false;
}

/**
 * Check if a Road is allowed on a given tile.
 *
 * @param t The current town.
 * @param tile The target tile.
 * @param dir The direction in which we want to extend the town.
 * @return true if it is allowed.
 */
static bool IsRoadAllowedHere(Town *t, TileIndex tile, DiagDirection dir)
{
	if (DistanceFromEdge(tile) == 0) return false;

	/* Prevent towns from building roads under bridges along the bridge. Looks silly. */
	if (IsBridgeAbove(tile) && GetBridgeAxis(tile) == DiagDirToAxis(dir)) return false;

	/* Check if there already is a road at this point? */
	if (GetTownRoadBits(tile) == ROAD_NONE) {
		/* No, try if we are able to build a road piece there.
		 * If that fails clear the land, and if that fails exit.
		 * This is to make sure that we can build a road here later. */
		RoadType rt = GetTownRoadType();
		if (Command<CMD_BUILD_ROAD>::Do({DoCommandFlag::Auto, DoCommandFlag::NoWater}, tile, (dir == DIAGDIR_NW || dir == DIAGDIR_SE) ? ROAD_Y : ROAD_X, rt, DRD_NONE, t->index).Failed() &&
				Command<CMD_LANDSCAPE_CLEAR>::Do({DoCommandFlag::Auto, DoCommandFlag::NoWater}, tile).Failed()) {
			return false;
		}
	}

	Slope cur_slope = _settings_game.construction.build_on_slopes ? std::get<0>(GetFoundationSlope(tile)) : GetTileSlope(tile);
	bool ret = !IsNeighbourRoadTile(tile, dir, t->layout == TL_ORIGINAL ? 1 : 2);
	if (cur_slope == SLOPE_FLAT) return ret;

	/* If the tile is not a slope in the right direction, then
	 * maybe terraform some. */
	Slope desired_slope = (dir == DIAGDIR_NW || dir == DIAGDIR_SE) ? SLOPE_NW : SLOPE_NE;
	if (desired_slope != cur_slope && ComplementSlope(desired_slope) != cur_slope) {
		if (Chance16(1, 8)) {
			CommandCost res = CMD_ERROR;
			if (!_generating_world && Chance16(1, 10)) {
				/* Note: Do not replace "^ SLOPE_ELEVATED" with ComplementSlope(). The slope might be steep. */
				res = std::get<0>(Command<CMD_TERRAFORM_LAND>::Do({DoCommandFlag::Execute, DoCommandFlag::Auto, DoCommandFlag::NoWater},
						tile, Chance16(1, 16) ? cur_slope : cur_slope ^ SLOPE_ELEVATED, false));
			}
			if (res.Failed() && Chance16(1, 3)) {
				/* We can consider building on the slope, though. */
				return ret;
			}
		}
		return false;
	}
	return ret;
}

static bool TerraformTownTile(TileIndex tile, Slope edges, bool dir)
{
	assert(tile < Map::Size());

	CommandCost r = std::get<0>(Command<CMD_TERRAFORM_LAND>::Do({DoCommandFlag::Auto, DoCommandFlag::NoWater}, tile, edges, dir));
	if (r.Failed() || r.GetCost() >= (_price[PR_TERRAFORM] + 2) * 8) return false;
	Command<CMD_TERRAFORM_LAND>::Do({DoCommandFlag::Auto, DoCommandFlag::NoWater, DoCommandFlag::Execute}, tile, edges, dir);
	return true;
}

static void LevelTownLand(TileIndex tile)
{
	assert(tile < Map::Size());

	/* Don't terraform if land is plain or if there's a house there. */
	if (IsTileType(tile, MP_HOUSE)) return;
	Slope tileh = GetTileSlope(tile);
	if (tileh == SLOPE_FLAT) return;

	/* First try up, then down */
	if (!TerraformTownTile(tile, ~tileh & SLOPE_ELEVATED, true)) {
		TerraformTownTile(tile, tileh & SLOPE_ELEVATED, false);
	}
}

/**
 * Generate the RoadBits of a grid tile.
 *
 * @param t The current town.
 * @param tile The tile in reference to the town.
 * @param dir The direction to which we are growing.
 * @return The RoadBit of the current tile regarding the selected town layout.
 */
static RoadBits GetTownRoadGridElement(Town *t, TileIndex tile, DiagDirection dir)
{
	/* align the grid to the downtown */
	TileIndexDiffC grid_pos = TileIndexToTileIndexDiffC(t->xy, tile); // Vector from downtown to the tile
	RoadBits rcmd = ROAD_NONE;

	switch (t->layout) {
		default: NOT_REACHED();

		case TL_2X2_GRID:
			if ((grid_pos.x % 3) == 0) rcmd |= ROAD_Y;
			if ((grid_pos.y % 3) == 0) rcmd |= ROAD_X;
			break;

		case TL_3X3_GRID:
			if ((grid_pos.x % 4) == 0) rcmd |= ROAD_Y;
			if ((grid_pos.y % 4) == 0) rcmd |= ROAD_X;
			break;
	}

	/* Optimise only X-junctions */
	if (rcmd != ROAD_ALL) return rcmd;

	RoadBits rb_template;

	switch (GetTileSlope(tile)) {
		default:       rb_template = ROAD_ALL; break;
		case SLOPE_W:  rb_template = ROAD_NW | ROAD_SW; break;
		case SLOPE_SW: rb_template = ROAD_Y  | ROAD_SW; break;
		case SLOPE_S:  rb_template = ROAD_SW | ROAD_SE; break;
		case SLOPE_SE: rb_template = ROAD_X  | ROAD_SE; break;
		case SLOPE_E:  rb_template = ROAD_SE | ROAD_NE; break;
		case SLOPE_NE: rb_template = ROAD_Y  | ROAD_NE; break;
		case SLOPE_N:  rb_template = ROAD_NE | ROAD_NW; break;
		case SLOPE_NW: rb_template = ROAD_X  | ROAD_NW; break;
		case SLOPE_STEEP_W:
		case SLOPE_STEEP_S:
		case SLOPE_STEEP_E:
		case SLOPE_STEEP_N:
			rb_template = ROAD_NONE;
			break;
	}

	/* Stop if the template is compatible to the growth dir */
	if (DiagDirToRoadBits(ReverseDiagDir(dir)) & rb_template) return rb_template;
	/* If not generate a straight road in the direction of the growth */
	return DiagDirToRoadBits(dir) | DiagDirToRoadBits(ReverseDiagDir(dir));
}

/**
 * Grows the town with an extra house.
 *  Check if there are enough neighbour house tiles
 *  next to the current tile. If there are enough
 *  add another house.
 *
 * @param t The current town.
 * @param tile The target tile for the extra house.
 * @return true if an extra house has been added.
 */
static bool GrowTownWithExtraHouse(Town *t, TileIndex tile)
{
	/* We can't look further than that. */
	if (DistanceFromEdge(tile) == 0) return false;

	uint counter = 0; // counts the house neighbour tiles

	/* Check the tiles E,N,W and S of the current tile for houses */
	for (DiagDirection dir = DIAGDIR_BEGIN; dir < DIAGDIR_END; dir++) {
		/* Count both void and house tiles for checking whether there
		 * are enough houses in the area. This to make it likely that
		 * houses get build up to the edge of the map. */
		switch (GetTileType(TileAddByDiagDir(tile, dir))) {
			case MP_HOUSE:
			case MP_VOID:
				counter++;
				break;

			default:
				break;
		}

		/* If there are enough neighbours stop here */
		if (counter >= 3) {
			if (TryBuildTownHouse(t, tile)) {
				_grow_town_result = GROWTH_SUCCEED;
				return true;
			}
			return false;
		}
	}
	return false;
}

/**
 * Grows the town with a road piece.
 *
 * @param t The current town.
 * @param tile The current tile.
 * @param rcmd The RoadBits we want to build on the tile.
 * @return true if the RoadBits have been added.
 */
static bool GrowTownWithRoad(const Town *t, TileIndex tile, RoadBits rcmd)
{
	RoadType rt = GetTownRoadType();
	if (Command<CMD_BUILD_ROAD>::Do({DoCommandFlag::Execute, DoCommandFlag::Auto, DoCommandFlag::NoWater}, tile, rcmd, rt, DRD_NONE, t->index).Succeeded()) {
		_grow_town_result = GROWTH_SUCCEED;
		return true;
	}
	return false;
}

/**
 * Checks if a town road can be continued into the next tile.
 *  Road vehicle stations, bridges, and tunnels are fine, as long as they are facing the right direction.
 *
 * @param t The current town
 * @param tile The tile where the road would be built
 * @param road_dir The direction of the road
 * @return true if the road can be continued, else false
 */
static bool CanRoadContinueIntoNextTile(const Town *t, const TileIndex tile, const DiagDirection road_dir)
{
	const TileIndexDiff delta = TileOffsByDiagDir(road_dir); // +1 tile in the direction of the road
	TileIndex next_tile = tile + delta; // The tile beyond which must be connectable to the target tile
	RoadBits rcmd = DiagDirToRoadBits(ReverseDiagDir(road_dir));
	RoadType rt = GetTownRoadType();

	/* Before we try anything, make sure the tile is on the map and not the void. */
	if (!IsValidTile(next_tile)) return false;

	/* If the next tile is a bridge or tunnel, allow if it's continuing in the same direction. */
	if (IsTileType(next_tile, MP_TUNNELBRIDGE)) {
		return GetTunnelBridgeTransportType(next_tile) == TRANSPORT_ROAD && GetTunnelBridgeDirection(next_tile) == road_dir;
	}

	/* If the next tile is a station, allow if it's a road station facing the proper direction. Otherwise return false. */
	if (IsTileType(next_tile, MP_STATION)) {
		/* If the next tile is a road station, allow if it can be entered by the new tunnel/bridge, otherwise disallow. */
		if (IsDriveThroughStopTile(next_tile)) return GetDriveThroughStopAxis(next_tile) == DiagDirToAxis(road_dir);
		if (IsBayRoadStopTile(next_tile)) return GetBayRoadStopDir(next_tile) == ReverseDiagDir(road_dir);
		return false;
	}

	/* If the next tile is a road depot, allow if it's facing the right way. */
	if (IsTileType(next_tile, MP_ROAD)) {
		return IsRoadDepot(next_tile) && GetRoadDepotDirection(next_tile) == ReverseDiagDir(road_dir);
	}

	/* If the next tile is a railroad track, check if towns are allowed to build level crossings.
	 * If level crossing are not allowed, reject the construction. Else allow DoCommand to determine if the rail track is buildable. */
	if (IsTileType(next_tile, MP_RAILWAY) && !_settings_game.economy.allow_town_level_crossings) return false;

	/* If a road tile can be built, the construction is allowed. */
	return Command<CMD_BUILD_ROAD>::Do({DoCommandFlag::Auto, DoCommandFlag::NoWater}, next_tile, rcmd, rt, DRD_NONE, t->index).Succeeded();
}

/**
 * CircularTileSearch proc which checks for a nearby parallel bridge to avoid building redundant bridges.
 * @param tile The tile to search.
 * @param user_data Reference to the valid direction of the proposed bridge.
 * @return true if another bridge exists, else false.
 */
static bool RedundantBridgeExistsNearby(TileIndex tile, void *user_data)
{
	/* Don't look into the void. */
	if (!IsValidTile(tile)) return false;

	/* Only consider bridge head tiles. */
	if (!IsBridgeTile(tile)) return false;

	/* Only consider road bridges. */
	if (GetTunnelBridgeTransportType(tile) != TRANSPORT_ROAD) return false;

	/* If the bridge is facing the same direction as the proposed bridge, we've found a redundant bridge. */
	return (GetTileSlope(tile) & InclinedSlope(ReverseDiagDir(*(DiagDirection *)user_data)));
}

/**
 * Grows the town with a bridge.
 *  At first we check if a bridge is reasonable.
 *  If so we check if we are able to build it.
 *
 * @param t The current town
 * @param tile The current tile
 * @param bridge_dir The valid direction in which to grow a bridge
 * @return true if a bridge has been build else false
 */
static bool GrowTownWithBridge(const Town *t, const TileIndex tile, const DiagDirection bridge_dir)
{
	assert(bridge_dir < DIAGDIR_END);

	const Slope slope = GetTileSlope(tile);

	/* Make sure the direction is compatible with the slope.
	 * Well we check if the slope has an up bit set in the
	 * reverse direction. */
	if (slope != SLOPE_FLAT && slope & InclinedSlope(bridge_dir)) return false;

	/* Assure that the bridge is connectable to the start side */
	if (!(GetTownRoadBits(TileAddByDiagDir(tile, ReverseDiagDir(bridge_dir))) & DiagDirToRoadBits(bridge_dir))) return false;

	/* We are in the right direction */
	uint bridge_length = 0;       // This value stores the length of the possible bridge
	TileIndex bridge_tile = tile; // Used to store the other waterside

	const TileIndexDiff delta = TileOffsByDiagDir(bridge_dir);

	/* To prevent really small towns from building disproportionately
	 * long bridges, make the max a function of its population. */
	const uint TOWN_BRIDGE_LENGTH_CAP = 11;
	uint base_bridge_length = 5;
	uint max_bridge_length = std::min(t->cache.population / 1000 + base_bridge_length, TOWN_BRIDGE_LENGTH_CAP);

	if (slope == SLOPE_FLAT) {
		/* Bridges starting on flat tiles are only allowed when crossing rivers, rails or one-way roads. */
		do {
			if (bridge_length++ >= base_bridge_length) {
				/* Allow to cross rivers, not big lakes, nor large amounts of rails or one-way roads. */
				return false;
			}
			bridge_tile += delta;
		} while (IsValidTile(bridge_tile) && ((IsWaterTile(bridge_tile) && !IsSea(bridge_tile)) || IsPlainRailTile(bridge_tile) || (IsNormalRoadTile(bridge_tile) && GetDisallowedRoadDirections(bridge_tile) != DRD_NONE)));
	} else {
		do {
			if (bridge_length++ >= max_bridge_length) {
				/* Ensure the bridge is not longer than the max allowed length. */
				return false;
			}
			bridge_tile += delta;
		} while (IsValidTile(bridge_tile) && (IsWaterTile(bridge_tile) || IsPlainRailTile(bridge_tile) || (IsNormalRoadTile(bridge_tile) && GetDisallowedRoadDirections(bridge_tile) != DRD_NONE)));
	}

	/* Don't allow a bridge where the start and end tiles are adjacent with no span between. */
	if (bridge_length == 1) return false;

	/* Make sure the road can be continued past the bridge. At this point, bridge_tile holds the end tile of the bridge. */
	if (!CanRoadContinueIntoNextTile(t, bridge_tile, bridge_dir)) return false;

	/* If another parallel bridge exists nearby, this one would be redundant and shouldn't be built. We don't care about flat bridges. */
	TileIndex search = tile;
	DiagDirection direction_to_match = bridge_dir;
	if (slope != SLOPE_FLAT && CircularTileSearch(&search, bridge_length, 0, 0, RedundantBridgeExistsNearby, &direction_to_match)) return false;

	for (uint8_t times = 0; times <= 22; times++) {
		uint8_t bridge_type = RandomRange(MAX_BRIDGES - 1);

		/* Can we actually build the bridge? */
		RoadType rt = GetTownRoadType();
		if (Command<CMD_BUILD_BRIDGE>::Do(CommandFlagsToDCFlags(GetCommandFlags<CMD_BUILD_BRIDGE>()), tile, bridge_tile, TRANSPORT_ROAD, bridge_type, rt).Succeeded()) {
			Command<CMD_BUILD_BRIDGE>::Do(CommandFlagsToDCFlags(GetCommandFlags<CMD_BUILD_BRIDGE>()).Set(DoCommandFlag::Execute), tile, bridge_tile, TRANSPORT_ROAD, bridge_type, rt);
			_grow_town_result = GROWTH_SUCCEED;
			return true;
		}
	}
	/* Quit if it selecting an appropriate bridge type fails a large number of times. */
	return false;
}

/**
 * Grows the town with a tunnel.
 *  First we check if a tunnel is reasonable.
 *  If so we check if we are able to build it.
 *
 * @param t The current town
 * @param tile The current tile
 * @param tunnel_dir The valid direction in which to grow a tunnel
 * @return true if a tunnel has been built, else false
 */
static bool GrowTownWithTunnel(const Town *t, const TileIndex tile, const DiagDirection tunnel_dir)
{
	assert(tunnel_dir < DIAGDIR_END);

	Slope slope = GetTileSlope(tile);

	/* Only consider building a tunnel if the starting tile is sloped properly. */
	if (slope != InclinedSlope(tunnel_dir)) return false;

	/* Assure that the tunnel is connectable to the start side */
	if (!(GetTownRoadBits(TileAddByDiagDir(tile, ReverseDiagDir(tunnel_dir))) & DiagDirToRoadBits(tunnel_dir))) return false;

	const TileIndexDiff delta = TileOffsByDiagDir(tunnel_dir);
	int max_tunnel_length = 0;

	/* There are two conditions for building tunnels: Under a mountain and under an obstruction. */
	if (CanRoadContinueIntoNextTile(t, tile, tunnel_dir)) {
		/* Only tunnel under a mountain if the slope is continuous for at least 4 tiles. We want tunneling to be a last resort for large hills. */
		TileIndex slope_tile = tile;
		for (uint8_t tiles = 0; tiles < 4; tiles++) {
			if (!IsValidTile(slope_tile)) return false;
			slope = GetTileSlope(slope_tile);
			if (slope != InclinedSlope(tunnel_dir) && !IsSteepSlope(slope) && !IsSlopeWithOneCornerRaised(slope)) return false;
			slope_tile += delta;
		}

		/* More population means longer tunnels, but make sure we can at least cover the smallest mountain which neccesitates tunneling. */
		max_tunnel_length = (t->cache.population / 1000) + 7;
	} else {
		/* When tunneling under an obstruction, the length limit is 5, enough to tunnel under a four-track railway. */
		max_tunnel_length = 5;
	}

	uint8_t tunnel_length = 0;
	TileIndex tunnel_tile = tile; // Iteratator to store the other end tile of the tunnel.

	/* Find the end tile of the tunnel for length and continuation checks. */
	do {
		if (tunnel_length++ >= max_tunnel_length) return false;
		tunnel_tile += delta;
		/* The tunnel ends when start and end tiles are the same height. */
	} while (IsValidTile(tunnel_tile) && GetTileZ(tile) != GetTileZ(tunnel_tile));

	/* Don't allow a tunnel where the start and end tiles are adjacent. */
	if (tunnel_length == 1) return false;

	/* Make sure the road can be continued past the tunnel. At this point, tunnel_tile holds the end tile of the tunnel. */
	if (!CanRoadContinueIntoNextTile(t, tunnel_tile, tunnel_dir)) return false;

	/* Attempt to build the tunnel. Return false if it fails to let the town build a road instead. */
	RoadType rt = GetTownRoadType();
	if (Command<CMD_BUILD_TUNNEL>::Do(CommandFlagsToDCFlags(GetCommandFlags<CMD_BUILD_TUNNEL>()), tile, TRANSPORT_ROAD, rt).Succeeded()) {
		Command<CMD_BUILD_TUNNEL>::Do(CommandFlagsToDCFlags(GetCommandFlags<CMD_BUILD_TUNNEL>()).Set(DoCommandFlag::Execute), tile, TRANSPORT_ROAD, rt);
		_grow_town_result = GROWTH_SUCCEED;
		return true;
	}

	return false;
}

/**
 * Checks whether at least one surrounding road allows to build a house here.
 *
 * @param t The tile where the house will be built.
 * @return true if at least one surrounding roadtype allows building houses here.
 */
static inline bool RoadTypesAllowHouseHere(TileIndex t)
{
	static const TileIndexDiffC tiles[] = { {-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}, {1, 1} };
	bool allow = false;

	for (const auto &ptr : tiles) {
		TileIndex cur_tile = t + ToTileIndexDiff(ptr);
		if (!IsValidTile(cur_tile)) continue;

		if (!(IsTileType(cur_tile, MP_ROAD) || IsAnyRoadStopTile(cur_tile))) continue;
		allow = true;

		RoadType road_rt = GetRoadTypeRoad(cur_tile);
		RoadType tram_rt = GetRoadTypeTram(cur_tile);
		if (road_rt != INVALID_ROADTYPE && !GetRoadTypeInfo(road_rt)->flags.Test(RoadTypeFlag::NoHouses)) return true;
		if (tram_rt != INVALID_ROADTYPE && !GetRoadTypeInfo(tram_rt)->flags.Test(RoadTypeFlag::NoHouses)) return true;
	}

	/* If no road was found surrounding the tile we can allow building the house since there is
	 * nothing which forbids it, if a road was found but the execution reached this point, then
	 * all the found roads don't allow houses to be built */
	return !allow;
}

/** Test if town can grow road onto a specific tile.
 * @param tile Tile to build upon.
 * @return true iff the tile's road type don't prevent extending the road.
 */
static bool TownCanGrowRoad(TileIndex tile)
{
	if (!IsTileType(tile, MP_ROAD)) return true;

	/* Allow extending on roadtypes which can be built by town, or if the road type matches the type the town will build. */
	RoadType rt = GetRoadTypeRoad(tile);
	return GetRoadTypeInfo(rt)->flags.Test(RoadTypeFlag::TownBuild) || GetTownRoadType() == rt;
}

/**
 * Check if the town is allowed to build roads.
 * @return true If the town is allowed to build roads.
 */
static inline bool TownAllowedToBuildRoads()
{
	return _settings_game.economy.allow_town_roads || _generating_world || _game_mode == GM_EDITOR;
}

/**
 * Grows the given town.
 * There are at the moment 3 possible way's for
 * the town expansion:
 *  @li Generate a random tile and check if there is a road allowed
 *  @li TL_ORIGINAL
 *  @li TL_BETTER_ROADS
 *  @li Check if the town geometry allows a road and which one
 *  @li TL_2X2_GRID
 *  @li TL_3X3_GRID
 *  @li Forbid roads, only build houses
 *
 * @param tile_ptr The current tile
 * @param cur_rb The current tiles RoadBits
 * @param target_dir The target road dir
 * @param t1 The current town
 */
static void GrowTownInTile(TileIndex *tile_ptr, RoadBits cur_rb, DiagDirection target_dir, Town *t1)
{
	RoadBits rcmd = ROAD_NONE;  // RoadBits for the road construction command
	TileIndex tile = *tile_ptr; // The main tile on which we base our growth

	assert(tile < Map::Size());

	if (cur_rb == ROAD_NONE) {
		/* Tile has no road. First reset the status counter
		 * to say that this is the last iteration. */
		_grow_town_result = GROWTH_SEARCH_STOPPED;

		if (!TownAllowedToBuildRoads()) return;
		if (!_settings_game.economy.allow_town_level_crossings && IsTileType(tile, MP_RAILWAY)) return;

		/* Remove hills etc */
		if (!_settings_game.construction.build_on_slopes || Chance16(1, 6)) LevelTownLand(tile);

		/* Is a road allowed here? */
		switch (t1->layout) {
			default: NOT_REACHED();

			case TL_3X3_GRID:
			case TL_2X2_GRID:
				rcmd = GetTownRoadGridElement(t1, tile, target_dir);
				if (rcmd == ROAD_NONE) return;
				break;

			case TL_BETTER_ROADS:
			case TL_ORIGINAL:
				if (!IsRoadAllowedHere(t1, tile, target_dir)) return;

				DiagDirection source_dir = ReverseDiagDir(target_dir);

				if (Chance16(1, 4)) {
					/* Randomize a new target dir */
					do target_dir = RandomDiagDir(); while (target_dir == source_dir);
				}

				if (!IsRoadAllowedHere(t1, TileAddByDiagDir(tile, target_dir), target_dir)) {
					/* A road is not allowed to continue the randomized road,
					 *  return if the road we're trying to build is curved. */
					if (target_dir != ReverseDiagDir(source_dir)) return;

					/* Return if neither side of the new road is a house */
					if (!IsTileType(TileAddByDiagDir(tile, ChangeDiagDir(target_dir, DIAGDIRDIFF_90RIGHT)), MP_HOUSE) &&
							!IsTileType(TileAddByDiagDir(tile, ChangeDiagDir(target_dir, DIAGDIRDIFF_90LEFT)), MP_HOUSE)) {
						return;
					}

					/* That means that the road is only allowed if there is a house
					 *  at any side of the new road. */
				}

				rcmd = DiagDirToRoadBits(target_dir) | DiagDirToRoadBits(source_dir);
				break;
		}

	} else if (target_dir < DIAGDIR_END && !(cur_rb & DiagDirToRoadBits(ReverseDiagDir(target_dir)))) {
		if (!TownCanGrowRoad(tile)) return;

		/* Continue building on a partial road.
		 * Should be always OK, so we only generate
		 * the fitting RoadBits */
		_grow_town_result = GROWTH_SEARCH_STOPPED;

		if (!TownAllowedToBuildRoads()) return;

		switch (t1->layout) {
			default: NOT_REACHED();

			case TL_3X3_GRID:
			case TL_2X2_GRID:
				rcmd = GetTownRoadGridElement(t1, tile, target_dir);
				break;

			case TL_BETTER_ROADS:
			case TL_ORIGINAL:
				rcmd = DiagDirToRoadBits(ReverseDiagDir(target_dir));
				break;
		}
	} else {
		bool allow_house = true; // Value which decides if we want to construct a house

		/* Reached a tunnel/bridge? Then continue at the other side of it, unless
		 * it is the starting tile. Half the time, we stay on this side then.*/
		if (IsTileType(tile, MP_TUNNELBRIDGE)) {
			if (GetTunnelBridgeTransportType(tile) == TRANSPORT_ROAD && (target_dir != DIAGDIR_END || Chance16(1, 2))) {
				*tile_ptr = GetOtherTunnelBridgeEnd(tile);
			}
			return;
		}

		/* Possibly extend the road in a direction.
		 * Randomize a direction and if it has a road, bail out. */
		target_dir = RandomDiagDir();
		RoadBits target_rb = DiagDirToRoadBits(target_dir);
		TileIndex house_tile; // position of a possible house

		if (cur_rb & target_rb) {
			/* If it's a road turn possibly build a house in a corner.
			 * Use intersection with straight road as an indicator
			 * that we randomed corner house position.
			 * A turn (and we check for that later) always has only
			 * one common bit with a straight road so it has the same
			 * chance to be chosen as the house on the side of a road.
			 */
			if ((cur_rb & ROAD_X) != target_rb) return;

			/* Check whether it is a turn and if so determine
			 * position of the corner tile */
			switch (cur_rb) {
				case ROAD_N:
					house_tile = TileAddByDir(tile, DIR_S);
					break;
				case ROAD_S:
					house_tile = TileAddByDir(tile, DIR_N);
					break;
				case ROAD_E:
					house_tile = TileAddByDir(tile, DIR_W);
					break;
				case ROAD_W:
					house_tile = TileAddByDir(tile, DIR_E);
					break;
				default:
					return;  // not a turn
			}
			target_dir = DIAGDIR_END;
		} else {
			house_tile = TileAddByDiagDir(tile, target_dir);
		}

		/* Don't walk into water. */
		if (HasTileWaterGround(house_tile)) return;

		if (!IsValidTile(house_tile)) return;

		if (target_dir != DIAGDIR_END && TownAllowedToBuildRoads()) {
			switch (t1->layout) {
				default: NOT_REACHED();

				case TL_3X3_GRID: // Use 2x2 grid afterwards!
					GrowTownWithExtraHouse(t1, TileAddByDiagDir(house_tile, target_dir));
					[[fallthrough]];

				case TL_2X2_GRID:
					rcmd = GetTownRoadGridElement(t1, tile, target_dir);
					allow_house = (rcmd & target_rb) == ROAD_NONE;
					break;

				case TL_BETTER_ROADS: // Use original afterwards!
					GrowTownWithExtraHouse(t1, TileAddByDiagDir(house_tile, target_dir));
					[[fallthrough]];

				case TL_ORIGINAL:
					/* Allow a house at the edge. 60% chance or
					 * always ok if no road allowed. */
					rcmd = target_rb;
					allow_house = (!IsRoadAllowedHere(t1, house_tile, target_dir) || Chance16(6, 10));
					break;
			}
		}

		allow_house &= RoadTypesAllowHouseHere(house_tile);

		if (allow_house) {
			/* Build a house, but not if there already is a house there. */
			if (!IsTileType(house_tile, MP_HOUSE)) {
				/* Level the land if possible */
				if (Chance16(1, 6)) LevelTownLand(house_tile);

				/* And build a house.
				 * Set result to -1 if we managed to build it. */
				if (TryBuildTownHouse(t1, house_tile)) {
					_grow_town_result = GROWTH_SUCCEED;
				}
			}
			return;
		}

		if (!TownCanGrowRoad(tile)) return;

		_grow_town_result = GROWTH_SEARCH_STOPPED;
	}

	/* Return if a water tile */
	if (HasTileWaterGround(tile)) return;

	/* Make the roads look nicer */
	rcmd = CleanUpRoadBits(tile, rcmd);
	if (rcmd == ROAD_NONE) return;

	/* Only use the target direction for bridges and tunnels to ensure they're connected.
	 * The target_dir is as computed previously according to town layout, so
	 * it will match it perfectly. */
	if (GrowTownWithBridge(t1, tile, target_dir)) return;
	if (GrowTownWithTunnel(t1, tile, target_dir)) return;

	GrowTownWithRoad(t1, tile, rcmd);
}

/**
 * Checks whether a road can be followed or is a dead end, that can not be extended to the next tile.
 * This only checks trivial but often cases.
 * @param tile Start tile for road.
 * @param dir Direction for road to follow or build.
 * @return true If road is or can be connected in the specified direction.
 */
static bool CanFollowRoad(TileIndex tile, DiagDirection dir)
{
	TileIndex target_tile = tile + TileOffsByDiagDir(dir);
	if (!IsValidTile(target_tile)) return false;
	if (HasTileWaterGround(target_tile)) return false;

	RoadBits target_rb = GetTownRoadBits(target_tile);
	if (TownAllowedToBuildRoads()) {
		/* Check whether a road connection exists or can be build. */
		switch (GetTileType(target_tile)) {
			case MP_ROAD:
				return target_rb != ROAD_NONE;

			case MP_STATION:
				return IsDriveThroughStopTile(target_tile);

			case MP_TUNNELBRIDGE:
				return GetTunnelBridgeTransportType(target_tile) == TRANSPORT_ROAD;

			case MP_HOUSE:
			case MP_INDUSTRY:
			case MP_OBJECT:
				return false;

			default:
				/* Checked for void and water earlier */
				return true;
		}
	} else {
		/* Check whether a road connection already exists,
		 * and it leads somewhere else. */
		RoadBits back_rb = DiagDirToRoadBits(ReverseDiagDir(dir));
		return (target_rb & back_rb) != 0 && (target_rb & ~back_rb) != 0;
	}
}

/**
 * Try to grow a town at a given road tile.
 * @param t The town to grow.
 * @param tile The road tile to try growing from.
 * @return true if we successfully expanded the town.
 */
static bool GrowTownAtRoad(Town *t, TileIndex tile)
{
	/* Special case.
	 * @see GrowTownInTile Check the else if
	 */
	DiagDirection target_dir = DIAGDIR_END; // The direction in which we want to extend the town

	assert(tile < Map::Size());

	/* Number of times to search.
	 * Better roads, 2X2 and 3X3 grid grow quite fast so we give
	 * them a little handicap. */
	switch (t->layout) {
		case TL_BETTER_ROADS:
			_grow_town_result = 10 + t->cache.num_houses * 2 / 9;
			break;

		case TL_3X3_GRID:
		case TL_2X2_GRID:
			_grow_town_result = 10 + t->cache.num_houses * 1 / 9;
			break;

		default:
			_grow_town_result = 10 + t->cache.num_houses * 4 / 9;
			break;
	}

	uint16_t prev_houses = t->cache.num_houses;
	do {
		RoadBits cur_rb = GetTownRoadBits(tile); // The RoadBits of the current tile

		/* Try to grow the town from this point */
		GrowTownInTile(&tile, cur_rb, target_dir, t);
		if (_grow_town_result == GROWTH_SUCCEED) {
			citymania::Emit(citymania::event::TownGrowthSucceeded{t, tile, prev_houses});
			return true;
		}

		/* Exclude the source position from the bitmask
		 * and return if no more road blocks available */
		if (IsValidDiagDirection(target_dir)) cur_rb &= ~DiagDirToRoadBits(ReverseDiagDir(target_dir));
		if (cur_rb == ROAD_NONE) {
			citymania::Emit(citymania::event::TownGrowthFailed{t, tile});
			return false;
		}

		if (IsTileType(tile, MP_TUNNELBRIDGE)) {
			/* Only build in the direction away from the tunnel or bridge. */
			target_dir = ReverseDiagDir(GetTunnelBridgeDirection(tile));
		} else {
			/* Select a random bit from the blockmask, walk a step
			 * and continue the search from there. */
			do {
				if (cur_rb == ROAD_NONE) {
					citymania::Emit(citymania::event::TownGrowthFailed{t, tile});
					return false;
				}
				RoadBits target_bits;
				do {
					target_dir = RandomDiagDir();
					target_bits = DiagDirToRoadBits(target_dir);
				} while (!(cur_rb & target_bits));
				cur_rb &= ~target_bits;
			} while (!CanFollowRoad(tile, target_dir));
		}
		tile = TileAddByDiagDir(tile, target_dir);

		if (IsTileType(tile, MP_ROAD) && !IsRoadDepot(tile) && HasTileRoadType(tile, RTT_ROAD)) {
			/* Don't allow building over roads of other cities */
			if (IsRoadOwner(tile, RTT_ROAD, OWNER_TOWN) && Town::GetByTile(tile) != t) {
				citymania::Emit(citymania::event::TownGrowthFailed{t, tile});
				return false;
			} else if (IsRoadOwner(tile, RTT_ROAD, OWNER_NONE) && _game_mode == GM_EDITOR) {
				/* If we are in the SE, and this road-piece has no town owner yet, it just found an
				 * owner :) (happy happy happy road now) */
				SetRoadOwner(tile, RTT_ROAD, OWNER_TOWN);
				SetTownIndex(tile, t->index);
			}
		}

		/* Max number of times is checked. */
	} while (--_grow_town_result >= 0);

	citymania::Emit(citymania::event::TownGrowthFailed{t, tile});
	return false;
}

/**
 * Generate a random road block.
 * The probability of a straight road
 * is somewhat higher than a curved.
 *
 * @return A RoadBits value with 2 bits set
 */
static RoadBits GenRandomRoadBits()
{
	uint32_t r = Random();
	uint a = GB(r, 0, 2);
	uint b = GB(r, 8, 2);
	if (a == b) b ^= 2;
	return (RoadBits)((ROAD_NW << a) + (ROAD_NW << b));
}

/**
 * Grow the town.
 * @param t The town to grow
 * @return true if we successfully grew the town with a road or house.
 */
static bool GrowTown(Town *t)
{
	static const TileIndexDiffC _town_coord_mod[] = {
		{-1,  0},
		{ 1,  1},
		{ 1, -1},
		{-1, -1},
		{-1,  0},
		{ 0,  2},
		{ 2,  0},
		{ 0, -2},
		{-1, -1},
		{-2,  2},
		{ 2,  2},
		{ 2, -2},
		{ 0,  0}
	};

	/* Current "company" is a town */
	Backup<CompanyID> cur_company(_current_company, OWNER_TOWN);

	TileIndex tile = t->xy; // The tile we are working with ATM

	/* Find a road that we can base the construction on. */
	for (const auto &ptr : _town_coord_mod) {
		if (GetTownRoadBits(tile) != ROAD_NONE) {
			bool success = GrowTownAtRoad(t, tile);
			cur_company.Restore();
			return success;
		}
		tile = TileAdd(tile, ToTileIndexDiff(ptr));
	}

	/* No road available, try to build a random road block by
	 * clearing some land and then building a road there. */
	if (TownAllowedToBuildRoads()) {
		tile = t->xy;
		for (const auto &ptr : _town_coord_mod) {
			/* Only work with plain land that not already has a house */
			if (!IsTileType(tile, MP_HOUSE) && IsTileFlat(tile)) {
				if (Command<CMD_LANDSCAPE_CLEAR>::Do({DoCommandFlag::Auto, DoCommandFlag::NoWater}, tile).Succeeded()) {
					RoadType rt = GetTownRoadType();
					Command<CMD_BUILD_ROAD>::Do({DoCommandFlag::Execute, DoCommandFlag::Auto}, tile, GenRandomRoadBits(), rt, DRD_NONE, t->index);
					cur_company.Restore();
					citymania::Emit(citymania::event::TownGrowthSucceeded{t, tile, t->cache.num_houses});
					return true;
				}
			}
			tile = TileAdd(tile, ToTileIndexDiff(ptr));
		}
	}

	cur_company.Restore();
	citymania::Emit(citymania::event::TownGrowthFailed{t, tile});
	return false;
}

/**
 * Update the cached town zone radii of a town, based on the number of houses.
 * @param t The town to update.
 */
void UpdateTownRadius(Town *t)
{
	auto prev_tz0 = t->cache.squared_town_zone_radius[0];
	static const std::array<std::array<uint32_t, HZB_END>, 23> _town_squared_town_zone_radius_data = {{
		{  4,  0,  0,  0,  0}, // 0
		{ 16,  0,  0,  0,  0},
		{ 25,  0,  0,  0,  0},
		{ 36,  0,  0,  0,  0},
		{ 49,  0,  4,  0,  0},
		{ 64,  0,  4,  0,  0}, // 20
		{ 64,  0,  9,  0,  1},
		{ 64,  0,  9,  0,  4},
		{ 64,  0, 16,  0,  4},
		{ 81,  0, 16,  0,  4},
		{ 81,  0, 16,  0,  4}, // 40
		{ 81,  0, 25,  0,  9},
		{ 81, 36, 25,  0,  9},
		{ 81, 36, 25, 16,  9},
		{ 81, 49,  0, 25,  9},
		{ 81, 64,  0, 25,  9}, // 60
		{ 81, 64,  0, 36,  9},
		{ 81, 64,  0, 36, 16},
		{100, 81,  0, 49, 16},
		{100, 81,  0, 49, 25},
		{121, 81,  0, 49, 25}, // 80
		{121, 81,  0, 49, 25},
		{121, 81,  0, 49, 36}, // 88
	}};

	if (t->cache.num_houses < std::size(_town_squared_town_zone_radius_data) * 4) {
		t->cache.squared_town_zone_radius = _town_squared_town_zone_radius_data[t->cache.num_houses / 4];
	} else {
		int mass = t->cache.num_houses / 8;
		/* Actually we are proportional to sqrt() but that's right because we are covering an area.
		 * The offsets are to make sure the radii do not decrease in size when going from the table
		 * to the calculated value.*/
		t->cache.squared_town_zone_radius[HZB_TOWN_EDGE] = mass * 15 - 40;
		t->cache.squared_town_zone_radius[HZB_TOWN_OUTSKIRT] = mass * 9 - 15;
		t->cache.squared_town_zone_radius[HZB_TOWN_OUTER_SUBURB] = 0;
		t->cache.squared_town_zone_radius[HZB_TOWN_INNER_SUBURB] = mass * 5 - 5;
		t->cache.squared_town_zone_radius[HZB_TOWN_CENTRE] = mass * 3 + 5;
	}
  	if (!_generating_world)
		citymania::UpdateTownZoning(t, prev_tz0);
}

/**
 * Update the maximum amount of monthly passengers and mail for a town, based on its population.
 * @param t The town to update.
 */
void UpdateTownMaxPass(Town *t)
{
	for (const CargoSpec *cs : CargoSpec::town_production_cargoes[TPE_PASSENGERS]) {
		t->supplied[cs->Index()].old_max = ScaleByCargoScale(t->cache.population >> 3, true);
	}
	for (const CargoSpec *cs : CargoSpec::town_production_cargoes[TPE_MAIL]) {
		t->supplied[cs->Index()].old_max = ScaleByCargoScale(t->cache.population >> 4, true);
	}
}

static void UpdateTownGrowthRate(Town *t);
static void UpdateTownGrowth(Town *t);

//CB

bool CB_Enabled(){
	return _cb_enabled;
}
void CB_SetCB(bool cb){
	_cb_enabled = cb;
	if(!_cb_enabled){
		CB_ResetRequirements();
	}
}

uint CB_GetStorage() {
	return _cb_storage;
}

void CB_SetStorage(uint storage){
	_cb_storage = storage;
}

void CB_SetRequirements(CargoType cargo_type, uint req, uint from, uint decay){
	CBREQ[cargo_type] = req;
	CBFROM[cargo_type] = from;
	CBDECAY[cargo_type] = decay;
}
void CB_ResetRequirements() {
	for(CargoType cargo = 0; cargo < NUM_CARGO; cargo++){
		CB_SetRequirements(cargo, 0, 0, 0);
	}
}
uint CB_GetReq(CargoType cargo_type) {
	return CBREQ[cargo_type];
}
uint CB_GetFrom(CargoType cargo_type) {
	return CBFROM[cargo_type];
}
uint CB_GetDecay(CargoType cargo_type) {
	return CBDECAY[cargo_type];
}

int CB_GetTownReq(uint population, uint req, uint from, bool from_non_important, bool prev_month)
{
	if (req > 0 && (population >= from || from_non_important)) {
		uint leap = 0;
		auto month = TimerGameEconomy::month;
		if (!prev_month) month++;
		if(month == 1) {
			if((TimerGameEconomy::year.base() % 4 == 0 && TimerGameEconomy::year.base() % 100 != 0) || TimerGameEconomy::year.base() % 400 == 0) leap = 1;
		}
		uint days_this_month = days_in_month[month + 1] + leap;
		// x cargo for 1000 people
		return population * req * days_this_month / 31000; // 31 days divide by 1000 (pop)
	}
	return 0;
}
uint CB_GetMaxTownStorage(uint32 population, uint req)
{
	return req > 0 ? (population * req * _cb_storage / 1000) : 0;
}

uint CB_GetMaxTownStorage(Town *town, uint cargo) {
	return CBREQ[cargo] > 0 ? (town->cache.population * CBREQ[cargo] * _cb_storage / 1000) : 0;
}

void CB_UpdateTownStorage([[maybe_unused]] Town *t)
{
	// InvalidateWindowData(WC_CB_TOWN, t->index);
	// t->growing = true;
	// if (!HasBit(t->flags, TOWN_IS_GROWING)) { //dont grow if not funded or missing transportation
	// 	t->growing = false;
	// }
	// for (uint i = 0; i < NUM_CARGO ; i++) {
	// 	if(CBREQ[i] == 0) continue;

	// 	t->storage[i] += t->new_act_cargo[i]; // add accumulated last month
	// 	t->storage[i] -= CB_GetTownReq(t->cache.population, CBREQ[i], CBFROM[i], false, true); //subtract monthly req
	// 	t->storage[i] = min((int)CB_GetMaxTownStorage(t->cache.population, CBREQ[i]), t->storage[i]); //check max storage

	// 	if (t->storage[i] < 0) {
	// 		t->growing = false;
	// 		t->delivered_enough[i] = false;
	// 		t->storage[i] = 0;
	// 	}
	// 	else t->delivered_enough[i] = true;

	// 	if (CBDECAY[i] == 100 && t->storage[i] > 0) {
	// 		t->storage[i] = 0;
	// 	}
	// 	else {
	// 		t->storage[i] *= (100 - CBDECAY[i]);
	// 		t->storage[i] /= 100;
	// 	}
	// 	t->act_cargo[i] = t->new_act_cargo[i];
	// 	t->new_act_cargo[i] = 0;
	// }

	// if (_settings_game.game_creation.landscape == LT_TROPIC) {
	// 	if (GetTropicZone(t->xy) == TROPICZONE_DESERT && (t->received[TE_FOOD].old_act <= 0 || t->received[TE_WATER].old_act <= 0) && t->cache.population > 60) {
	// 		t->growing = false;
	// 	}
	// }
	// else if (_settings_game.game_creation.landscape == LT_ARCTIC) {
	// 	if (TilePixelHeight(t->xy) >= GetSnowLine() && t->received[TE_FOOD].old_act <= 0 && t->cache.population > 90) {
	// 		t->growing = false;
	// 	}
	// }
}
//CB

/**
 * Actually create a town.
 *
 * @param t The town.
 * @param tile Where to put it.
 * @param townnameparts The town name.
 * @param size The preset size of the town.
 * @param city Should we create a city?
 * @param layout The road layout of the town.
 * @param manual Was the town placed manually?
 */
static void DoCreateTown(Town *t, TileIndex tile, uint32_t townnameparts, TownSize size, bool city, TownLayout layout, bool manual)
{
	AutoRestoreBackup backup(_generating_town, true);

	t->xy = tile;
	t->cache.num_houses = 0;
	t->time_until_rebuild = 10;
	UpdateTownRadius(t);
	t->flags = 0;
	t->cache.population = 0;
	InitializeBuildingCounts(t);
	/* Spread growth across ticks so even if there are many
	 * similar towns they're unlikely to grow all in one tick */
	t->grow_counter = t->index % Ticks::TOWN_GROWTH_TICKS;
	t->growth_rate = TownTicksToGameTicks(250);
	t->show_zone = false;

	_town_kdtree.Insert(t->index);

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

	t->fund_buildings_months = 0;
	//CB
	t->cm.cb.growth_state = citymania::TownGrowthState::NOT_GROWING;
	for (uint i = 0; i < NUM_CARGO ; i++) {
		t->cm.cb.stored[i] = 0;
		t->cm.cb.delivered[i] = 0;
		t->cm.cb.required[i] = 0;
		t->cm.cb.delivered_last_month[i] = 0;
		t->cm.cb.required_last_month[i] = 0;
	}
	t->cm.fund_regularly = {};
	t->cm.do_powerfund = {};
	t->cm.advertise_regularly = {};
	t->cm.ad_rating_goal = 95;
	t->cm.ad_ref_goods_entry = std::nullopt;
	//CB

	for (uint i = 0; i != MAX_COMPANIES; i++) t->ratings[i] = RATING_INITIAL;

	t->have_ratings = {};
	t->exclusivity = CompanyID::Invalid();
	t->exclusive_counter = 0;
	t->statues = {};

	{
		TownNameParams tnp(_settings_game.game_creation.town_name);
		t->townnamegrfid = tnp.grfid;
		t->townnametype = tnp.type;
	}
	t->townnameparts = townnameparts;

	t->cm.town_label = 3;  // CM

	t->InitializeLayout(layout);

	t->larger_town = city;

	int x = (int)size * 16 + 3;
	if (size == TSZ_RANDOM) x = (Random() & 0xF) + 8;
	/* Don't create huge cities when founding town in-game */
	if (city && (!manual || _game_mode == GM_EDITOR)) x *= _settings_game.economy.initial_city_size;

	t->cache.num_houses += x;
	UpdateTownRadius(t);

	int i = x * 4;
	do {
		GrowTown(t);
	} while (--i);

	t->UpdateVirtCoord();
	InvalidateWindowData(WC_TOWN_DIRECTORY, 0, TDIWD_FORCE_REBUILD);

	t->cache.num_houses -= x;
	citymania::_closest_cache_ref = INVALID_TILE;
	UpdateTownRadius(t);
	UpdateTownGrowthRate(t);
	UpdateTownMaxPass(t);
	UpdateAirportsNoise();
	citymania::Emit(citymania::event::TownBuilt{t});
}

/**
 * Check if it's possible to place a town on a given tile.
 * @param tile The tile to check.
 * @return A zero cost if allowed, otherwise an error.
 */
static CommandCost TownCanBePlacedHere(TileIndex tile)
{
	/* Check if too close to the edge of map */
	if (DistanceFromEdge(tile) < 12) {
		return CommandCost(STR_ERROR_TOO_CLOSE_TO_EDGE_OF_MAP_SUB);
	}

	/* Check distance to all other towns. */
	if (IsCloseToTown(tile, 20)) {
		return CommandCost(STR_ERROR_TOO_CLOSE_TO_ANOTHER_TOWN);
	}

	/* Can only build on clear flat areas, possibly with trees. */
	if ((!IsTileType(tile, MP_CLEAR) && !IsTileType(tile, MP_TREES)) || !IsTileFlat(tile)) {
		return CommandCost(STR_ERROR_SITE_UNSUITABLE);
	}

	return CommandCost(EXPENSES_OTHER);
}

/**
 * Verifies this custom name is unique. Only custom names are checked.
 * @param name The name to check.
 * @return true if the name is unique
 */
static bool IsUniqueTownName(const std::string &name)
{
	for (const Town *t : Town::Iterate()) {
		if (!t->name.empty() && t->name == name) return false;
	}

	return true;
}

/**
 * Create a new town.
 * @param flags The type of operation.
 * @param tile The coordinates where town is built.
 * @param size The size of the town (@see TownSize).
 * @param city Should we build a city?
 * @param layout The town road layout (@see TownLayout).
 * @param random_location Should we use a random location? (randomize \c tile )
 * @param townnameparts Town name parts.
 * @param text Custom name for the town. If empty, the town name parts will be used.
 * @return The cost of this operation or an error.
 */
std::tuple<CommandCost, Money, TownID> CmdFoundTown(DoCommandFlags flags, TileIndex tile, TownSize size, bool city, TownLayout layout, bool random_location, uint32_t townnameparts, const std::string &text)
{
	TownNameParams par(_settings_game.game_creation.town_name);

	if (size >= TSZ_END) return { CMD_ERROR, 0, TownID::Invalid() };
	if (layout >= NUM_TLS) return { CMD_ERROR, 0, TownID::Invalid() };

	/* Some things are allowed only in the scenario editor and for game scripts. */
	if (_game_mode != GM_EDITOR && _current_company != OWNER_DEITY) {
		if (_settings_game.economy.found_town == TF_FORBIDDEN) return { CMD_ERROR, 0, TownID::Invalid() };
		if (size == TSZ_LARGE) return { CMD_ERROR, 0, TownID::Invalid() };
		if (random_location) return { CMD_ERROR, 0, TownID::Invalid() };
		if (_settings_game.economy.found_town != TF_CUSTOM_LAYOUT && layout != _settings_game.economy.town_layout) {
			return { CMD_ERROR, 0, TownID::Invalid() };
		}
	} else if (_current_company == OWNER_DEITY && random_location) {
		/* Random parameter is not allowed for Game Scripts. */
		return { CMD_ERROR, 0, TownID::Invalid() };
	}

	if (text.empty()) {
		/* If supplied name is empty, townnameparts has to generate unique automatic name */
		if (!VerifyTownName(townnameparts, &par)) return { CommandCost(STR_ERROR_NAME_MUST_BE_UNIQUE), 0, TownID::Invalid() };
	} else {
		/* If name is not empty, it has to be unique custom name */
		if (Utf8StringLength(text) >= MAX_LENGTH_TOWN_NAME_CHARS) return { CMD_ERROR, 0, TownID::Invalid() };
		if (!IsUniqueTownName(text)) return { CommandCost(STR_ERROR_NAME_MUST_BE_UNIQUE), 0, TownID::Invalid() };
	}

	/* Allocate town struct */
	if (!Town::CanAllocateItem()) return { CommandCost(STR_ERROR_TOO_MANY_TOWNS), 0, TownID::Invalid() };

	if (!random_location) {
		CommandCost ret = TownCanBePlacedHere(tile);
		if (ret.Failed()) return { ret, 0, TownID::Invalid() };
	}

	static const uint8_t price_mult[][TSZ_RANDOM + 1] = {{ 15, 25, 40, 25 }, { 20, 35, 55, 35 }};
	/* multidimensional arrays have to have defined length of non-first dimension */
	static_assert(lengthof(price_mult[0]) == 4);

	CommandCost cost(EXPENSES_OTHER, _price[PR_BUILD_TOWN]);
	uint8_t mult = price_mult[city][size];

	cost.MultiplyCost(mult);

	/* Create the town */
	TownID new_town = TownID::Invalid();
	if (flags.Test(DoCommandFlag::Execute)) {
		if (cost.GetCost() > GetAvailableMoneyForCommand()) {
			return { CommandCost(EXPENSES_OTHER), cost.GetCost(), TownID::Invalid() };
		}

		Backup<bool> old_generating_world(_generating_world, true);
		UpdateNearestTownForRoadTiles(true);
		Town *t;
		if (random_location) {
			t = CreateRandomTown(20, townnameparts, size, city, layout);
		} else {
			t = new Town(tile);
			DoCreateTown(t, tile, townnameparts, size, city, layout, true);
		}

		UpdateNearestTownForRoadTiles(false);
		old_generating_world.Restore();

		if (t == nullptr) return { CommandCost(STR_ERROR_NO_SPACE_FOR_TOWN), 0, TownID::Invalid() };

		new_town = t->index;

		if (!text.empty()) {
			t->name = text;
			t->UpdateVirtCoord();
		}

		if (_game_mode != GM_EDITOR) {
			/* 't' can't be nullptr since 'random' is false outside scenedit */
			assert(!random_location);

			if (_current_company == OWNER_DEITY) {
				AddTileNewsItem(GetEncodedString(STR_NEWS_NEW_TOWN_UNSPONSORED, t->index), NewsType::IndustryOpen, tile);
			} else {
				std::string company_name = GetString(STR_COMPANY_NAME, _current_company);
				AddTileNewsItem(GetEncodedString(STR_NEWS_NEW_TOWN, company_name, t->index), NewsType::IndustryOpen, tile);
			}
			AI::BroadcastNewEvent(new ScriptEventTownFounded(t->index));
			Game::NewEvent(new ScriptEventTownFounded(t->index));
		}
	}
	return { cost, 0, new_town };
}

/**
 * Towns must all be placed on the same grid or when they eventually
 * interpenetrate their road networks will not mesh nicely; this
 * function adjusts a tile so that it aligns properly.
 *
 * @param tile The tile to start at.
 * @param layout The town layout in effect.
 * @return The adjusted tile.
 */
static TileIndex AlignTileToGrid(TileIndex tile, TownLayout layout)
{
	switch (layout) {
		case TL_2X2_GRID: return TileXY(TileX(tile) - TileX(tile) % 3, TileY(tile) - TileY(tile) % 3);
		case TL_3X3_GRID: return TileXY(TileX(tile) & ~3, TileY(tile) & ~3);
		default:          return tile;
	}
}

/**
 * Towns must all be placed on the same grid or when they eventually
 * interpenetrate their road networks will not mesh nicely; this
 * function tells you if a tile is properly aligned.
 *
 * @param tile The tile to start at.
 * @param layout The town layout in effect.
 * @return true if the tile is in the correct location.
 */
static bool IsTileAlignedToGrid(TileIndex tile, TownLayout layout)
{
	switch (layout) {
		case TL_2X2_GRID: return TileX(tile) % 3 == 0 && TileY(tile) % 3 == 0;
		case TL_3X3_GRID: return TileX(tile) % 4 == 0 && TileY(tile) % 4 == 0;
		default:          return true;
	}
}

/**
 * Used as the user_data for FindFurthestFromWater
 */
struct SpotData {
	TileIndex tile; ///< holds the tile that was found
	uint max_dist;  ///< holds the distance that tile is from the water
	TownLayout layout; ///< tells us what kind of town we're building
};

/**
 * CircularTileSearch callback; finds the tile furthest from any
 * water. slightly bit tricky, since it has to do a search of its own
 * in order to find the distance to the water from each square in the
 * radius.
 *
 * Also, this never returns true, because it needs to take into
 * account all locations being searched before it knows which is the
 * furthest.
 *
 * @param tile Start looking from this tile
 * @param user_data Storage area for data that must last across calls;
 * must be a pointer to struct SpotData
 *
 * @return always false
 */
static bool FindFurthestFromWater(TileIndex tile, void *user_data)
{
	SpotData *sp = (SpotData*)user_data;
	uint dist = GetClosestWaterDistance(tile, true);

	if (IsTileType(tile, MP_CLEAR) &&
			IsTileFlat(tile) &&
			IsTileAlignedToGrid(tile, sp->layout) &&
			dist > sp->max_dist) {
		sp->tile = tile;
		sp->max_dist = dist;
	}

	return false;
}

/**
 * CircularTileSearch callback to find the nearest land tile.
 * @param tile Start looking from this tile
 */
static bool FindNearestEmptyLand(TileIndex tile, void *)
{
	return IsTileType(tile, MP_CLEAR);
}

/**
 * Given a spot on the map (presumed to be a water tile), find a good
 * coastal spot to build a city. We don't want to build too close to
 * the edge if we can help it (since that inhibits city growth) hence
 * the search within a search within a search. O(n*m^2), where n is
 * how far to search for land, and m is how far inland to look for a
 * flat spot.
 *
 * @param tile Start looking from this spot.
 * @param layout the road layout to search for
 * @return tile that was found
 */
static TileIndex FindNearestGoodCoastalTownSpot(TileIndex tile, TownLayout layout)
{
	SpotData sp = { INVALID_TILE, 0, layout };

	TileIndex coast = tile;
	if (CircularTileSearch(&coast, 40, FindNearestEmptyLand, nullptr)) {
		CircularTileSearch(&coast, 10, FindFurthestFromWater, &sp);
		return sp.tile;
	}

	/* if we get here just give up */
	return INVALID_TILE;
}

/**
 * Create a random town somewhere in the world.
 * @param attempts How many times should we try?
 * @param townnameparts The name of the town.
 * @param size The size preset of the town.
 * @param city Should we build a city?
 * @param layout The road layout to build.
 * @return The town object, or nullptr if we failed to create a town anywhere.
 */
static Town *CreateRandomTown(uint attempts, uint32_t townnameparts, TownSize size, bool city, TownLayout layout)
{
	assert(_game_mode == GM_EDITOR || _generating_world); // These are the preconditions for CMD_DELETE_TOWN

	if (!Town::CanAllocateItem()) return nullptr;

	do {
		/* Generate a tile index not too close from the edge */
		TileIndex tile = AlignTileToGrid(RandomTile(), layout);

		/* if we tried to place the town on water, slide it over onto
		 * the nearest likely-looking spot */
		if (IsTileType(tile, MP_WATER)) {
			tile = FindNearestGoodCoastalTownSpot(tile, layout);
			if (tile == INVALID_TILE) continue;
		}

		/* Make sure town can be placed here */
		if (TownCanBePlacedHere(tile).Failed()) continue;

		/* Allocate a town struct */
		Town *t = new Town(tile);

		DoCreateTown(t, tile, townnameparts, size, city, layout, false);

		/* if the population is still 0 at the point, then the
		 * placement is so bad it couldn't grow at all */
		if (t->cache.population > 0) return t;

		Backup<CompanyID> cur_company(_current_company, OWNER_TOWN);
		[[maybe_unused]] CommandCost rc = Command<CMD_DELETE_TOWN>::Do(DoCommandFlag::Execute, t->index);
		cur_company.Restore();
		assert(rc.Succeeded());

		/* We already know that we can allocate a single town when
		 * entering this function. However, we create and delete
		 * a town which "resets" the allocation checks. As such we
		 * need to check again when assertions are enabled. */
		assert(Town::CanAllocateItem());
	} while (--attempts != 0);

	return nullptr;
}

static const uint8_t _num_initial_towns[4] = {5, 11, 23, 46};  // very low, low, normal, high

/**
 * Generate a number of towns with a given layout.
 * This function is used by the Random Towns button in Scenario Editor as well as in world generation.
 * @param layout The road layout to build.
 * @return true if towns have been successfully created.
 */
bool GenerateTowns(TownLayout layout)
{
	uint current_number = 0;
	uint difficulty = (_game_mode != GM_EDITOR) ? _settings_game.difficulty.number_towns : 0;
	uint total = (difficulty == (uint)CUSTOM_TOWN_NUMBER_DIFFICULTY) ? _settings_game.game_creation.custom_town_number : Map::ScaleBySize(_num_initial_towns[difficulty] + (Random() & 7));
	total = std::min<uint>(TownPool::MAX_SIZE, total);
	uint32_t townnameparts;
	TownNames town_names;

	SetGeneratingWorldProgress(GWP_TOWN, total);

	/* Pre-populate the town names list with the names of any towns already on the map */
	for (const Town *town : Town::Iterate()) {
		town_names.insert(town->GetCachedName());
	}

	/* Randomised offset for city status. This means with e.g. 1-in-4 towns being cities, a map with 10 towns
	 * may have 2 or 3 cities, instead of always 3. */
	uint city_random_offset = _settings_game.economy.larger_towns == 0 ? 0 : (Random() % _settings_game.economy.larger_towns);

	/* First attempt will be made at creating the suggested number of towns.
	 * Note that this is really a suggested value, not a required one.
	 * We would not like the system to lock up just because the user wanted 100 cities on a 64*64 map, would we? */
	do {
		bool city = (_settings_game.economy.larger_towns != 0 && ((city_random_offset + current_number) % _settings_game.economy.larger_towns) == 0);
		IncreaseGeneratingWorldProgress(GWP_TOWN);
		/* Get a unique name for the town. */
		if (!GenerateTownName(_random, &townnameparts, &town_names)) continue;
		/* try 20 times to create a random-sized town for the first loop. */
		if (CreateRandomTown(20, townnameparts, TSZ_RANDOM, city, layout) != nullptr) current_number++; // If creation was successful, raise a flag.
	} while (--total);

	town_names.clear();

	/* Build the town k-d tree again to make sure it's well balanced */
	RebuildTownKdtree();
	citymania::Emit(citymania::event::TownCachesRebuilt());

	if (current_number != 0) return true;

	/* If current_number is still zero at this point, it means that not a single town has been created.
	 * So give it a last try, but now more aggressive */
	if (GenerateTownName(_random, &townnameparts) &&
			CreateRandomTown(10000, townnameparts, TSZ_RANDOM, _settings_game.economy.larger_towns != 0, layout) != nullptr) {
		return true;
	}

	/* If there are no towns at all and we are generating new game, bail out */
	if (Town::GetNumItems() == 0 && _game_mode != GM_EDITOR) {
		ShowErrorMessage(GetEncodedString(STR_ERROR_COULD_NOT_CREATE_TOWN), {}, WL_CRITICAL);
	}

	return false;  // we are still without a town? we failed, simply
}


/**
 * Returns the bit corresponding to the town zone of the specified tile.
 * @param t Town on which town zone is to be found.
 * @param tile TileIndex where town zone needs to be found.
 * @return the bit position of the given zone, as defined in HouseZones.
 */
HouseZonesBits GetTownRadiusGroup(const Town *t, TileIndex tile)
{
	uint dist = DistanceSquare(tile, t->xy);

	if (t->fund_buildings_months && dist <= 25) return HZB_TOWN_CENTRE;

	HouseZonesBits smallest = HZB_TOWN_EDGE;
	for (HouseZonesBits i = HZB_BEGIN; i < HZB_END; i++) {
		if (dist < t->cache.squared_town_zone_radius[i]) smallest = i;
	}

	return smallest;
}

/**
 * Clears tile and builds a house or house part.
 * @param tile The tile to build upon.
 * @param t The town which will own the house.
 * @param counter The construction stage counter for the house.
 * @param stage The current construction stage of the house.
 * @param type The type of house.
 * @param random_bits Random bits for newgrf houses to use.
 * @param is_protected Whether the house is protected from the town upgrading it.
 * @pre The house can be built here.
 */
static inline void ClearMakeHouseTile(TileIndex tile, Town *t, uint8_t counter, uint8_t stage, HouseID type, uint8_t random_bits, bool is_protected)
{
	[[maybe_unused]] CommandCost cc = Command<CMD_LANDSCAPE_CLEAR>::Do({DoCommandFlag::Execute, DoCommandFlag::Auto, DoCommandFlag::NoWater}, tile);
	assert(cc.Succeeded());

	IncreaseBuildingCount(t, type);
	MakeHouseTile(tile, t->index, counter, stage, type, random_bits, is_protected);
	if (HouseSpec::Get(type)->building_flags.Test(BuildingFlag::IsAnimated)) AddAnimatedTile(tile, false);

	MarkTileDirtyByTile(tile);
}


/**
 * Write house information into the map. For multi-tile houses, all tiles are marked.
 * @param town The town related to this house
 * @param t The tile to build on. If a multi-tile house, this is the northern-most tile.
 * @param counter The counter of the construction stage.
 * @param stage The current construction stage.
 * @param The type of house.
 * @param random_bits Random bits for newgrf houses to use.
 * @param is_protected Whether the house is protected from the town upgrading it.
 * @pre The house can be built here.
 */
static void MakeTownHouse(TileIndex tile, Town *t, uint8_t counter, uint8_t stage, HouseID type, uint8_t random_bits, bool is_protected)
{
	BuildingFlags size = HouseSpec::Get(type)->building_flags;

	ClearMakeHouseTile(tile, t, counter, stage, type, random_bits, is_protected);
	if (size.Any(BUILDING_2_TILES_Y))   ClearMakeHouseTile(tile + TileDiffXY(0, 1), t, counter, stage, ++type, random_bits, is_protected);
	if (size.Any(BUILDING_2_TILES_X))   ClearMakeHouseTile(tile + TileDiffXY(1, 0), t, counter, stage, ++type, random_bits, is_protected);
	if (size.Any(BUILDING_HAS_4_TILES)) ClearMakeHouseTile(tile + TileDiffXY(1, 1), t, counter, stage, ++type, random_bits, is_protected);

	ForAllStationsAroundTiles(TileArea(tile, size.Any(BUILDING_2_TILES_X) ? 2 : 1, size.Any(BUILDING_2_TILES_Y) ? 2 : 1), [t](Station *st, TileIndex) {
		t->stations_near.insert(st);
		return true;
	});
}


/**
 * Check if a house can be built here, based on slope, whether there's a bridge above, and if we can clear the land.
 * @param tile The tile to check.
 * @param noslope Are foundations prohibited for this house?
 * @return true iff house can be built here.
 */
static inline bool CanBuildHouseHere(TileIndex tile, bool noslope)
{
	/* cannot build on these slopes... */
	Slope slope = GetTileSlope(tile);
	if ((noslope && slope != SLOPE_FLAT) || IsSteepSlope(slope)) return false;

	/* at least one RoadTypes allow building the house here? */
	if (!RoadTypesAllowHouseHere(tile)) return false;

	/* building under a bridge? */
	if (IsBridgeAbove(tile)) return false;

	/* can we clear the land? */
	return Command<CMD_LANDSCAPE_CLEAR>::Do({DoCommandFlag::Auto, DoCommandFlag::NoWater}, tile).Succeeded();
}


/**
 * Check if a tile where we want to build a multi-tile house has an appropriate max Z.
 * @param tile The tile to check.
 * @param z The max Z level to allow.
 * @param noslope Are foundations disallowed for this house?
 * @return true iff house can be built here.
 * @see CanBuildHouseHere()
 */
static inline bool CheckBuildHouseSameZ(TileIndex tile, int z, bool noslope)
{
	if (!CanBuildHouseHere(tile, noslope)) return false;

	/* if building on slopes is allowed, there will be flattening foundation (to tile max z) */
	if (GetTileMaxZ(tile) != z) return false;

	return true;
}


/**
 * Checks if a house of size 2x2 can be built at this tile.
 * @param tile The tile of the house's northernmost tile.
 * @param z The maximum tile z, so all tiles are the same height.
 * @param noslope Are foundations disallowed for this house?
 * @return true iff house can be built here.
 * @see CheckBuildHouseSameZ()
 */
static bool CheckFree2x2Area(TileIndex tile, int z, bool noslope)
{
	/* we need to check this tile too because we can be at different tile now */
	if (!CheckBuildHouseSameZ(tile, z, noslope)) return false;

	for (DiagDirection d = DIAGDIR_SE; d < DIAGDIR_END; d++) {
		tile += TileOffsByDiagDir(d);
		if (!CheckBuildHouseSameZ(tile, z, noslope)) return false;
	}

	return true;
}


/**
 * Checks if the current town layout allows building here.
 * @param t The town.
 * @param tile The tile to check.
 * @return true iff town layout allows building here.
 * @note see layouts
 */
static inline bool TownLayoutAllowsHouseHere(Town *t, TileIndex tile)
{
	/* Allow towns everywhere when we don't build roads */
	if (!TownAllowedToBuildRoads()) return true;

	TileIndexDiffC grid_pos = TileIndexToTileIndexDiffC(t->xy, tile);

	switch (t->layout) {
		case TL_2X2_GRID:
			if ((grid_pos.x % 3) == 0 || (grid_pos.y % 3) == 0) return false;
			break;

		case TL_3X3_GRID:
			if ((grid_pos.x % 4) == 0 || (grid_pos.y % 4) == 0) return false;
			break;

		default:
			break;
	}

	return true;
}


/**
 * Checks if the current town layout allows a 2x2 building here.
 * @param t The town.
 * @param tile The tile to check.
 * @return true iff town layout allows a 2x2 building here.
 * @note see layouts
 */
static inline bool TownLayoutAllows2x2HouseHere(Town *t, TileIndex tile)
{
	/* Allow towns everywhere when we don't build roads */
	if (!TownAllowedToBuildRoads()) return true;

	/* Compute relative position of tile. (Positive offsets are towards north) */
	TileIndexDiffC grid_pos = TileIndexToTileIndexDiffC(t->xy, tile);

	switch (t->layout) {
		case TL_2X2_GRID:
			grid_pos.x %= 3;
			grid_pos.y %= 3;
			if ((grid_pos.x != 2 && grid_pos.x != -1) ||
				(grid_pos.y != 2 && grid_pos.y != -1)) return false;
			break;

		case TL_3X3_GRID:
			if ((grid_pos.x & 3) < 2 || (grid_pos.y & 3) < 2) return false;
			break;

		default:
			break;
	}

	return true;
}


/**
 * Checks if a 1x2 or 2x1 building is allowed here, accounting for road layout and tile heights.
 * Also, tests both building positions that occupy this tile.
 * @param tile The tile where the building should be built.
 * @param t The town.
 * @param maxz The maximum Z level, since all tiles must have the same height.
 * @param noslope Are foundations disallowed for this house?
 * @param second The diagdir from the first tile to the second tile.
 */
static bool CheckTownBuild2House(TileIndex *tile, Town *t, int maxz, bool noslope, DiagDirection second)
{
	/* 'tile' is already checked in BuildTownHouse() - CanBuildHouseHere() and slope test */

	TileIndex tile2 = *tile + TileOffsByDiagDir(second);
	if (TownLayoutAllowsHouseHere(t, tile2) && CheckBuildHouseSameZ(tile2, maxz, noslope)) return true;

	tile2 = *tile + TileOffsByDiagDir(ReverseDiagDir(second));
	if (TownLayoutAllowsHouseHere(t, tile2) && CheckBuildHouseSameZ(tile2, maxz, noslope)) {
		*tile = tile2;
		return true;
	}

	return false;
}


/**
 * Checks if a 1x2 or 2x1 building is allowed here, accounting for road layout and tile heights.
 * Also, tests all four building positions that occupy this tile.
 * @param tile The tile where the building should be built.
 * @param t The town.
 * @param maxz The maximum Z level, since all tiles must have the same height.
 * @param noslope Are foundations disallowed for this house?
 */
static bool CheckTownBuild2x2House(TileIndex *tile, Town *t, int maxz, bool noslope)
{
	TileIndex tile2 = *tile;

	for (DiagDirection d = DIAGDIR_SE;; d++) { // 'd' goes through DIAGDIR_SE, DIAGDIR_SW, DIAGDIR_NW, DIAGDIR_END
		if (TownLayoutAllows2x2HouseHere(t, tile2) && CheckFree2x2Area(tile2, maxz, noslope)) {
			*tile = tile2;
			return true;
		}
		if (d == DIAGDIR_END) break;
		tile2 += TileOffsByDiagDir(ReverseDiagDir(d)); // go clockwise
	}

	return false;
}

/**
 * Build a house at this tile.
 * @param t The town the house will belong to.
 * @param tile The tile to try building on.
 * @param hs The @a HouseSpec of the house.
 * @param house The @a HouseID of the house.
 * @param random_bits The random data to be associated with the house.
 * @param house_completed Should the house be placed already complete, instead of under construction?
 * @param is_protected Whether the house is protected from the town upgrading it.
 */
static void BuildTownHouse(Town *t, TileIndex tile, const HouseSpec *hs, HouseID house, uint8_t random_bits, bool house_completed, bool is_protected, bool cm_is_rebuilding)
{
	/* build the house */
	t->cache.num_houses++;

	uint8_t construction_counter = 0;
	uint8_t construction_stage = 0;

	bool completed = false;
	if (_generating_world || _game_mode == GM_EDITOR || house_completed) {
		uint32_t construction_random = Random();

		construction_stage = TOWN_HOUSE_COMPLETED;
		if (_generating_world && !hs->extra_flags.Test(HouseExtraFlag::BuildingIsHistorical) && Chance16(1, 7)) construction_stage = GB(construction_random, 0, 2);

		if (construction_stage == TOWN_HOUSE_COMPLETED) {
			ChangePopulation(t, hs->population);
			completed = true;
		} else {
			construction_counter = GB(construction_random, 2, 2);
		}
	}
	
	citymania::UpdateZoningTownHouses(t, t->cache.num_houses - 1);

	MakeTownHouse(tile, t, construction_counter, construction_stage, house, random_bits, is_protected);
	UpdateTownRadius(t);
	UpdateTownGrowthRate(t);

	if (!_generating_world) {
		citymania::Emit(citymania::event::HouseBuilt{t, tile, house, hs, cm_is_rebuilding});
		if (completed) citymania::Emit(citymania::event::HouseCompleted{t, tile, house, hs});
	}
}

/**
 * Tries to build a house at this tile.
 * @param t The town the house will belong to.
 * @param tile The tile to try building on.
 * @return false iff no house can be built on this tile.
 */
static bool TryBuildTownHouse(Town *t, TileIndex tile, bool cm_is_rebuilding)
{
	/* forbidden building here by town layout */
	if (!TownLayoutAllowsHouseHere(t, tile)) return false;

	/* no house allowed at all, bail out */
	if (!CanBuildHouseHere(tile, false)) return false;

	Slope slope = GetTileSlope(tile);
	int maxz = GetTileMaxZ(tile);

	/* Get the town zone type of the current tile, as well as the climate.
	 * This will allow to easily compare with the specs of the new house to build */
	HouseZonesBits rad = GetTownRadiusGroup(t, tile);

	/* Above snow? */
	int land = to_underlying(_settings_game.game_creation.landscape);
	if (_settings_game.game_creation.landscape == LandscapeType::Arctic && maxz > HighestSnowLine()) land = -1;

	uint bitmask = (1 << rad) + (1 << (land + 12));

	/* bits 0-4 are used
	 * bits 11-15 are used
	 * bits 5-10 are not used. */
	static std::vector<std::pair<HouseID, uint>> probs;
	probs.clear();

	uint probability_max = 0;

	/* Generate a list of all possible houses that can be built. */
	for (const auto &hs : HouseSpec::Specs()) {
		/* Verify that the candidate house spec matches the current tile status */
		if ((~hs.building_availability & bitmask) != 0 || !hs.enabled || hs.grf_prop.override_id != INVALID_HOUSE_ID) continue;

		/* Don't let these counters overflow. Global counters are 32bit, there will never be that many houses. */
		if (hs.class_id != HOUSE_NO_CLASS) {
			/* id_count is always <= class_count, so it doesn't need to be checked */
			if (t->cache.building_counts.class_count[hs.class_id] == UINT16_MAX) continue;
		} else {
			/* If the house has no class, check id_count instead */
			if (t->cache.building_counts.id_count[hs.Index()] == UINT16_MAX) continue;
		}

		uint cur_prob = hs.probability;
		probability_max += cur_prob;
		probs.emplace_back(hs.Index(), cur_prob);
	}

	TileIndex base_tile = tile;

	while (probability_max > 0) {
		/* Building a multitile building can change the location of tile.
		 * The building would still be built partially on that tile, but
		 * its northern tile would be elsewhere. However, if the callback
		 * fails we would be basing further work from the changed tile.
		 * So a next 1x1 tile building could be built on the wrong tile. */
		tile = base_tile;

		uint r = RandomRange(probability_max);
		uint i;
		for (i = 0; i < probs.size(); i++) {
			if (probs[i].second > r) break;
			r -= probs[i].second;
		}

		HouseID house = probs[i].first;
		probability_max -= probs[i].second;

		/* remove tested house from the set */
		probs[i] = probs.back();
		probs.pop_back();

		const HouseSpec *hs = HouseSpec::Get(house);

		if (!_generating_world && _game_mode != GM_EDITOR && hs->extra_flags.Test(HouseExtraFlag::BuildingIsHistorical)) {
			continue;
		}

		if (TimerGameCalendar::year < hs->min_year || TimerGameCalendar::year > hs->max_year) continue;

		/* Special houses that there can be only one of. */
		uint oneof = 0;

		if (hs->building_flags.Test(BuildingFlag::IsChurch)) {
			SetBit(oneof, TOWN_HAS_CHURCH);
		} else if (hs->building_flags.Test(BuildingFlag::IsStadium)) {
			SetBit(oneof, TOWN_HAS_STADIUM);
		}

		if (t->flags & oneof) continue;

		/* Make sure there is no slope? */
		bool noslope = hs->building_flags.Test(BuildingFlag::NotSloped);
		if (noslope && slope != SLOPE_FLAT) continue;

		if (hs->building_flags.Test(BuildingFlag::Size2x2)) {
			if (!CheckTownBuild2x2House(&tile, t, maxz, noslope)) continue;
		} else if (hs->building_flags.Test(BuildingFlag::Size2x1)) {
			if (!CheckTownBuild2House(&tile, t, maxz, noslope, DIAGDIR_SW)) continue;
		} else if (hs->building_flags.Test(BuildingFlag::Size1x2)) {
			if (!CheckTownBuild2House(&tile, t, maxz, noslope, DIAGDIR_SE)) continue;
		} else {
			/* 1x1 house checks are already done */
		}

		uint8_t random_bits = Random();

		if (hs->callback_mask.Test(HouseCallbackMask::AllowConstruction)) {
			uint16_t callback_res = GetHouseCallback(CBID_HOUSE_ALLOW_CONSTRUCTION, 0, 0, house, t, tile, true, random_bits);
			if (callback_res != CALLBACK_FAILED && !Convert8bitBooleanCallback(hs->grf_prop.grffile, CBID_HOUSE_ALLOW_CONSTRUCTION, callback_res)) continue;
		}

		/* Special houses that there can be only one of. */
		t->flags |= oneof;

		BuildTownHouse(t, tile, hs, house, random_bits, false, hs->extra_flags.Test(HouseExtraFlag::BuildingIsProtected), cm_is_rebuilding);

		return true;
	}

	return false;
}

/**
 * Place an individual house.
 * @param flags Type of operation.
 * @param tile Tile on which to place the house.
 * @param HouseID The HouseID of the house spec.
 * @param is_protected Whether the house is protected from the town upgrading it.
 * @return Empty cost or an error.
 */
CommandCost CmdPlaceHouse(DoCommandFlags flags, TileIndex tile, HouseID house, bool is_protected)
{
	if (_game_mode != GM_EDITOR && _settings_game.economy.place_houses == PH_FORBIDDEN) return CMD_ERROR;

	if (Town::GetNumItems() == 0) return CommandCost(STR_ERROR_MUST_FOUND_TOWN_FIRST);

	if (static_cast<size_t>(house) >= HouseSpec::Specs().size()) return CMD_ERROR;
	const HouseSpec *hs = HouseSpec::Get(house);
	if (!hs->enabled) return CMD_ERROR;

	Town *t = ClosestTownFromTile(tile, UINT_MAX);

	/* cannot build on these slopes... */
	Slope slope = GetTileSlope(tile);
	if (IsSteepSlope(slope)) return CommandCost(STR_ERROR_LAND_SLOPED_IN_WRONG_DIRECTION);

	/* building under a bridge? */
	if (IsBridgeAbove(tile)) return CommandCost(STR_ERROR_MUST_DEMOLISH_BRIDGE_FIRST);

	/* can we clear the land? */
	CommandCost cost = Command<CMD_LANDSCAPE_CLEAR>::Do({DoCommandFlag::Auto, DoCommandFlag::NoWater}, tile);
	if (!cost.Succeeded()) return cost;

	int maxz = GetTileMaxZ(tile);

	/* Make sure there is no slope? */
	bool noslope = hs->building_flags.Test(BuildingFlag::NotSloped);
	if (noslope && slope != SLOPE_FLAT) return CommandCost(STR_ERROR_FLAT_LAND_REQUIRED);

	TileArea ta = tile;
	if (hs->building_flags.Test(BuildingFlag::Size2x2)) ta.Add(TileAddXY(tile, 1, 1));
	if (hs->building_flags.Test(BuildingFlag::Size2x1)) ta.Add(TileAddByDiagDir(tile, DIAGDIR_SW));
	if (hs->building_flags.Test(BuildingFlag::Size1x2)) ta.Add(TileAddByDiagDir(tile, DIAGDIR_SE));

	/* Check additional tiles covered by this house. */
	for (const TileIndex &subtile : ta) {
		cost = Command<CMD_LANDSCAPE_CLEAR>::Do({DoCommandFlag::Auto, DoCommandFlag::NoWater}, subtile);
		if (!cost.Succeeded()) return cost;

		if (!CheckBuildHouseSameZ(subtile, maxz, noslope)) return CommandCost(STR_ERROR_LAND_SLOPED_IN_WRONG_DIRECTION);
	}

	if (flags.Test(DoCommandFlag::Execute)) {
		bool house_completed = _settings_game.economy.place_houses == PH_ALLOWED_CONSTRUCTED;
		BuildTownHouse(t, tile, hs, house, Random(), house_completed, is_protected, false);
	}

	return CommandCost();
}

/**
 * Update data structures when a house is removed
 * @param tile  Tile of the house
 * @param t     Town owning the house
 * @param house House type
 */
static void DoClearTownHouseHelper(TileIndex tile, Town *t, HouseID house)
{
	assert(IsTileType(tile, MP_HOUSE));
	DecreaseBuildingCount(t, house);
	DoClearSquare(tile);

	DeleteNewGRFInspectWindow(GSF_HOUSES, tile.base());
}

/**
 * Determines if a given HouseID is part of a multitile house.
 * The given ID is set to the ID of the north tile and the TileDiff to the north tile is returned.
 *
 * @param house Is changed to the HouseID of the north tile of the same house
 * @return TileDiff from the tile of the given HouseID to the north tile
 */
TileIndexDiff GetHouseNorthPart(HouseID &house)
{
	if (house >= 3) { // house id 0,1,2 MUST be single tile houses, or this code breaks.
		if (HouseSpec::Get(house - 1)->building_flags.Test(BuildingFlag::Size2x1)) {
			house--;
			return TileDiffXY(-1, 0);
		} else if (HouseSpec::Get(house - 1)->building_flags.Any(BUILDING_2_TILES_Y)) {
			house--;
			return TileDiffXY(0, -1);
		} else if (HouseSpec::Get(house - 2)->building_flags.Any(BUILDING_HAS_4_TILES)) {
			house -= 2;
			return TileDiffXY(-1, 0);
		} else if (HouseSpec::Get(house - 3)->building_flags.Any(BUILDING_HAS_4_TILES)) {
			house -= 3;
			return TileDiffXY(-1, -1);
		}
	}
	return TileDiffXY(0, 0);
}

/**
 * Clear a town house.
 * @param t The town which owns the house.
 * @param tile The tile to clear.
 */
void ClearTownHouse(Town *t, TileIndex tile)
{
	assert(IsTileType(tile, MP_HOUSE));

	HouseID house = GetHouseType(tile);

	/* The northernmost tile of the house is the main house. */
	tile += GetHouseNorthPart(house);

	const HouseSpec *hs = HouseSpec::Get(house);

	/* Remove population from the town if the house is finished. */
	bool is_completed = IsHouseCompleted(tile);
	if (is_completed) {
		ChangePopulation(t, -hs->population);
	}

	t->cache.num_houses--;

	/* Clear flags for houses that only may exist once/town. */
	if (hs->building_flags.Test(BuildingFlag::IsChurch)) {
		ClrBit(t->flags, TOWN_HAS_CHURCH);
	} else if (hs->building_flags.Test(BuildingFlag::IsStadium)) {
		ClrBit(t->flags, TOWN_HAS_STADIUM);
	}

	/* Do the actual clearing of tiles */
	DoClearTownHouseHelper(tile, t, house);
	if (hs->building_flags.Any(BUILDING_2_TILES_Y))   DoClearTownHouseHelper(tile + TileDiffXY(0, 1), t, ++house);
	if (hs->building_flags.Any(BUILDING_2_TILES_X))   DoClearTownHouseHelper(tile + TileDiffXY(1, 0), t, ++house);
	if (hs->building_flags.Any(BUILDING_HAS_4_TILES)) DoClearTownHouseHelper(tile + TileDiffXY(1, 1), t, ++house);

	RemoveNearbyStations(t, tile, hs->building_flags);

	UpdateTownRadius(t);
	citymania::UpdateZoningTownHouses(t, t->cache.num_houses + 1);

	citymania::Emit(citymania::event::HouseCleared{t, tile, house, hs, is_completed});
}

/**
 * Rename a town (server-only).
 * @param flags type of operation
 * @param town_id town ID to rename
 * @param text the new name or an empty string when resetting to the default
 * @return the cost of this operation or an error
 */
CommandCost CmdRenameTown(DoCommandFlags flags, TownID town_id, const std::string &text)
{
	Town *t = Town::GetIfValid(town_id);
	if (t == nullptr) return CMD_ERROR;

	bool reset = text.empty();

	if (!reset) {
		if (Utf8StringLength(text) >= MAX_LENGTH_TOWN_NAME_CHARS) return CMD_ERROR;
		if (!IsUniqueTownName(text)) return CommandCost(STR_ERROR_NAME_MUST_BE_UNIQUE);
	}

	if (flags.Test(DoCommandFlag::Execute)) {
		t->cached_name.clear();
		if (reset) {
			t->name.clear();
		} else {
			t->name = text;
		}

		t->UpdateVirtCoord();
		InvalidateWindowData(WC_TOWN_DIRECTORY, 0, TDIWD_FORCE_RESORT);
		ClearAllStationCachedNames();
		ClearAllIndustryCachedNames();
		UpdateAllStationVirtCoords();
	}
	return CommandCost();
}

/**
 * Determines the first cargo with a certain town effect
 * @param effect Town effect of interest
 * @return first active cargo slot with that effect
 */
const CargoSpec *FindFirstCargoWithTownAcceptanceEffect(TownAcceptanceEffect effect)
{
	for (const CargoSpec *cs : CargoSpec::Iterate()) {
		if (cs->town_acceptance_effect == effect) return cs;
	}
	return nullptr;
}

/**
 * Change the cargo goal of a town.
 * @param flags Type of operation.
 * @param town_id Town ID to cargo game of.
 * @param tae TownEffect to change the game of.
 * @param goal The new goal value.
 * @return Empty cost or an error.
 */
CommandCost CmdTownCargoGoal(DoCommandFlags flags, TownID town_id, TownAcceptanceEffect tae, uint32_t goal)
{
	if (_current_company != OWNER_DEITY) return CMD_ERROR;

	if (tae < TAE_BEGIN || tae >= TAE_END) return CMD_ERROR;

	Town *t = Town::GetIfValid(town_id);
	if (t == nullptr) return CMD_ERROR;

	/* Validate if there is a cargo which is the requested TownEffect */
	const CargoSpec *cargo = FindFirstCargoWithTownAcceptanceEffect(tae);
	if (cargo == nullptr) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) {
		t->goal[tae] = goal;
		UpdateTownGrowth(t);
		InvalidateWindowData(WC_TOWN_VIEW, town_id);
	}

	return CommandCost();
}

/**
 * Set a custom text in the Town window.
 * @param flags Type of operation.
 * @param town_id Town ID to change the text of.
 * @param text The new text (empty to remove the text).
 * @return Empty cost or an error.
 */
CommandCost CmdTownSetText(DoCommandFlags flags, TownID town_id, const EncodedString &text)
{
	if (_current_company != OWNER_DEITY) return CMD_ERROR;
	Town *t = Town::GetIfValid(town_id);
	if (t == nullptr) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) {
		t->text.clear();
		if (!text.empty()) t->text = text;
		InvalidateWindowData(WC_TOWN_VIEW, town_id);
	}

	return CommandCost();
}

/**
 * Change the growth rate of the town.
 * @param flags Type of operation.
 * @param town_id Town ID to cargo game of.
 * @param growth_rate Amount of days between growth, or TOWN_GROWTH_RATE_NONE, or 0 to reset custom growth rate.
 * @return Empty cost or an error.
 */
CommandCost CmdTownGrowthRate(DoCommandFlags flags, TownID town_id, uint16_t growth_rate)
{
	if (_current_company != OWNER_DEITY) return CMD_ERROR;

	Town *t = Town::GetIfValid(town_id);
	if (t == nullptr) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) {
		if (growth_rate == 0) {
			/* Just clear the flag, UpdateTownGrowth will determine a proper growth rate */
			ClrBit(t->flags, TOWN_CUSTOM_GROWTH);
		} else {
			uint old_rate = t->growth_rate;
			if (t->grow_counter >= old_rate) {
				/* This also catches old_rate == 0 */
				t->grow_counter = growth_rate;
			} else {
				/* Scale grow_counter, so half finished houses stay half finished */
				t->grow_counter = t->grow_counter * growth_rate / old_rate;
			}
			t->growth_rate = growth_rate;
			SetBit(t->flags, TOWN_CUSTOM_GROWTH);
		}
		UpdateTownGrowth(t);
		InvalidateWindowData(WC_TOWN_VIEW, town_id);
		InvalidateWindowData(CM_WC_CB_TOWN, town_id);
	}

	return CommandCost();
}

/**
 * Change the rating of a company in a town
 * @param flags Type of operation.
 * @param town_id Town ID to change, bit 16..23 =
 * @param company_id Company ID to change.
 * @param rating New rating of company (signed int16_t).
 * @return Empty cost or an error.
 */
CommandCost CmdTownRating(DoCommandFlags flags, TownID town_id, CompanyID company_id, int16_t rating)
{
	if (_current_company != OWNER_DEITY) return CMD_ERROR;

	Town *t = Town::GetIfValid(town_id);
	if (t == nullptr) return CMD_ERROR;

	if (!Company::IsValidID(company_id)) return CMD_ERROR;

	int16_t new_rating = Clamp(rating, RATING_MINIMUM, RATING_MAXIMUM);
	if (flags.Test(DoCommandFlag::Execute)) {
		t->ratings[company_id] = new_rating;
		InvalidateWindowData(WC_TOWN_AUTHORITY, town_id);
	}

	return CommandCost();
}

/**
 * Expand a town (scenario editor only).
 * @param flags Type of operation.
 * @param TownID Town ID to expand.
 * @param grow_amount Amount to grow, or 0 to grow a random size up to the current amount of houses.
 * @return Empty cost or an error.
 */
CommandCost CmdExpandTown(DoCommandFlags flags, TownID town_id, uint32_t grow_amount)
{
	if (_game_mode != GM_EDITOR && _current_company != OWNER_DEITY) return CMD_ERROR;
	Town *t = Town::GetIfValid(town_id);
	if (t == nullptr) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) {
		/* The more houses, the faster we grow */
		if (grow_amount == 0) {
			uint amount = RandomRange(ClampTo<uint16_t>(t->cache.num_houses / 10)) + 3;
			t->cache.num_houses += amount;
			UpdateTownRadius(t);

			uint n = amount * 10;
			do GrowTown(t); while (--n);

			t->cache.num_houses -= amount;
		} else {
			for (; grow_amount > 0; grow_amount--) {
				/* Try several times to grow, as we are really suppose to grow */
				for (uint i = 0; i < 25; i++) if (GrowTown(t)) break;
			}
		}
		UpdateTownRadius(t);

		UpdateTownMaxPass(t);
	}

	return CommandCost();
}

/**
 * Delete a town (scenario editor or worldgen only).
 * @param flags Type of operation.
 * @param town_id Town ID to delete.
 * @return Empty cost or an error.
 */
CommandCost CmdDeleteTown(DoCommandFlags flags, TownID town_id)
{
	if (_game_mode != GM_EDITOR && !_generating_world) return CMD_ERROR;
	Town *t = Town::GetIfValid(town_id);
	if (t == nullptr) return CMD_ERROR;

	/* Stations refer to towns. */
	for (const Station *st : Station::Iterate()) {
		if (st->town == t) {
			/* Non-oil rig stations are always a problem. */
			if (!st->facilities.Test(StationFacility::Airport) || st->airport.type != AT_OILRIG) return CMD_ERROR;
			/* We can only automatically delete oil rigs *if* there's no vehicle on them. */
			CommandCost ret = Command<CMD_LANDSCAPE_CLEAR>::Do(flags, st->airport.tile);
			if (ret.Failed()) return ret;
		}
	}

	/* Waypoints refer to towns. */
	for (const Waypoint *wp : Waypoint::Iterate()) {
		if (wp->town == t) return CMD_ERROR;
	}

	/* Depots refer to towns. */
	for (const Depot *d : Depot::Iterate()) {
		if (d->town == t) return CMD_ERROR;
	}

	/* Check all tiles for town ownership. First check for bridge tiles, as
	 * these do not directly have an owner so we need to check adjacent
	 * tiles. This won't work correctly in the same loop if the adjacent
	 * tile was already deleted earlier in the loop. */
	for (const auto current_tile : Map::Iterate()) {
		if (IsTileType(current_tile, MP_TUNNELBRIDGE) && TestTownOwnsBridge(current_tile, t)) {
			CommandCost ret = Command<CMD_LANDSCAPE_CLEAR>::Do(flags, current_tile);
			if (ret.Failed()) return ret;
		}
	}

	/* Check all remaining tiles for town ownership. */
	for (const auto current_tile : Map::Iterate()) {
		bool try_clear = false;
		switch (GetTileType(current_tile)) {
			case MP_ROAD:
				try_clear = HasTownOwnedRoad(current_tile) && GetTownIndex(current_tile) == t->index;
				break;

			case MP_HOUSE:
				try_clear = GetTownIndex(current_tile) == t->index;
				break;

			case MP_INDUSTRY:
				try_clear = Industry::GetByTile(current_tile)->town == t;
				break;

			case MP_OBJECT:
				if (Town::GetNumItems() == 1) {
					/* No towns will be left, remove it! */
					try_clear = true;
				} else {
					Object *o = Object::GetByTile(current_tile);
					if (o->town == t) {
						if (o->type == OBJECT_STATUE) {
							/* Statue... always remove. */
							try_clear = true;
						} else {
							/* Tell to find a new town. */
							if (flags.Test(DoCommandFlag::Execute)) o->town = nullptr;
						}
					}
				}
				break;

			default:
				break;
		}
		if (try_clear) {
			CommandCost ret = Command<CMD_LANDSCAPE_CLEAR>::Do(flags, current_tile);
			if (ret.Failed()) return ret;
		}
	}

	/* The town destructor will delete the other things related to the town. */
	if (flags.Test(DoCommandFlag::Execute)) {
		_town_kdtree.Remove(t->index);
		if (t->cache.sign.kdtree_valid) _viewport_sign_kdtree.Remove(ViewportSignKdtreeItem::MakeTown(t->index));
		delete t;
	}

	return CommandCost();
}

/**
 * Get cost factors for a TownAction
 * @param action TownAction to get cost factor for.
 * @returns Cost factor.
 */
uint8_t GetTownActionCost(TownAction action)
{
	/**
	 * Factor in the cost of each town action.
	 * @see TownActions
	 */
	static const uint8_t town_action_costs[] = {
		2, 4, 9, 35, 48, 53, 117, 175
	};
	static_assert(std::size(town_action_costs) == to_underlying(TownAction::End));

	assert(to_underlying(action) < std::size(town_action_costs));
	return town_action_costs[to_underlying(action)];
}

/**
 * Perform the "small advertising campaign" town action.
 * @param t The town to advertise in.
 * @param flags Type of operation.
 * @return An empty cost.
 */
static CommandCost TownActionAdvertiseSmall(Town *t, DoCommandFlags flags)
{
	if (flags.Test(DoCommandFlag::Execute)) {
		ModifyStationRatingAround(t->xy, _current_company, 0x40, 10);
	}
	return CommandCost();
}

/**
 * Perform the "medium advertising campaign" town action.
 * @param t The town to advertise in.
 * @param flags Type of operation.
 * @return An empty cost.
 */
static CommandCost TownActionAdvertiseMedium(Town *t, DoCommandFlags flags)
{
	if (flags.Test(DoCommandFlag::Execute)) {
		ModifyStationRatingAround(t->xy, _current_company, 0x70, 15);
	}
	return CommandCost();
}

/**
 * Perform the "large advertising campaign" town action.
 * @param t The town to advertise in.
 * @param flags Type of operation.
 * @return An empty cost.
 */
static CommandCost TownActionAdvertiseLarge(Town *t, DoCommandFlags flags)
{
	if (flags.Test(DoCommandFlag::Execute)) {
		ModifyStationRatingAround(t->xy, _current_company, 0xA0, 20);
	}
	return CommandCost();
}

/**
 * Perform the "local road reconstruction" town action.
 * @param t The town to grief in.
 * @param flags Type of operation.
 * @return An empty cost.
 */
static CommandCost TownActionRoadRebuild(Town *t, DoCommandFlags flags)
{
	/* Check if the company is allowed to fund new roads. */
	if (!_settings_game.economy.fund_roads) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) {
		t->road_build_months = 6;

		std::string company_name = GetString(STR_COMPANY_NAME, _current_company);

		AddNewsItem(
			GetEncodedString(TimerGameEconomy::UsingWallclockUnits() ? STR_NEWS_ROAD_REBUILDING_MINUTES : STR_NEWS_ROAD_REBUILDING_MONTHS, t->index, company_name),
			NewsType::General, NewsStyle::Normal, {}, t->index);
		AI::BroadcastNewEvent(new ScriptEventRoadReconstruction(_current_company, t->index));
		Game::NewEvent(new ScriptEventRoadReconstruction(_current_company, t->index));
	}
	return CommandCost();
}

/**
 * Check whether the land can be cleared.
 * @param tile Tile to check.
 * @return true if the tile can be cleared.
 */
static bool CheckClearTile(TileIndex tile)
{
	Backup<CompanyID> cur_company(_current_company, OWNER_NONE);
	CommandCost r = Command<CMD_LANDSCAPE_CLEAR>::Do({}, tile);
	cur_company.Restore();
	return r.Succeeded();
}

/** Structure for storing data while searching the best place to build a statue. */
struct StatueBuildSearchData {
	TileIndex best_position; ///< Best position found so far.
	int tile_count;          ///< Number of tiles tried.

	StatueBuildSearchData(TileIndex best_pos, int count) : best_position(best_pos), tile_count(count) { }
};

/**
 * Search callback function for #TownActionBuildStatue.
 * @param tile Tile on which to perform the search.
 * @param user_data Reference to the statue search data.
 * @return Result of the test.
 */
static bool SearchTileForStatue(TileIndex tile, void *user_data)
{
	static const int STATUE_NUMBER_INNER_TILES = 25; // Number of tiles int the center of the city, where we try to protect houses.

	StatueBuildSearchData *statue_data = (StatueBuildSearchData *)user_data;
	statue_data->tile_count++;

	/* Statues can be build on slopes, just like houses. Only the steep slopes is a no go. */
	if (IsSteepSlope(GetTileSlope(tile))) return false;
	/* Don't build statues under bridges. */
	if (IsBridgeAbove(tile)) return false;

	/* A clear-able open space is always preferred. */
	if ((IsTileType(tile, MP_CLEAR) || IsTileType(tile, MP_TREES)) && CheckClearTile(tile)) {
		statue_data->best_position = tile;
		return true;
	}

	bool house = IsTileType(tile, MP_HOUSE);

	/* Searching inside the inner circle. */
	if (statue_data->tile_count <= STATUE_NUMBER_INNER_TILES) {
		/* Save first house in inner circle. */
		if (house && statue_data->best_position == INVALID_TILE && CheckClearTile(tile)) {
			statue_data->best_position = tile;
		}

		/* If we have reached the end of the inner circle, and have a saved house, terminate the search. */
		return statue_data->tile_count == STATUE_NUMBER_INNER_TILES && statue_data->best_position != INVALID_TILE;
	}

	/* Searching outside the circle, just pick the first possible spot. */
	statue_data->best_position = tile; // Is optimistic, the condition below must also hold.
	return house && CheckClearTile(tile);
}

/**
 * Perform a 9x9 tiles circular search from the center of the town
 * in order to find a free tile to place a statue
 * @param t town to search in
 * @param flags Used to check if the statue must be built or not.
 * @return Empty cost or an error.
 */
static CommandCost TownActionBuildStatue(Town *t, DoCommandFlags flags)
{
	if (!Object::CanAllocateItem()) return CommandCost(STR_ERROR_TOO_MANY_OBJECTS);

	TileIndex tile = t->xy;
	StatueBuildSearchData statue_data(INVALID_TILE, 0);
	if (!CircularTileSearch(&tile, 9, SearchTileForStatue, &statue_data)) return CommandCost(STR_ERROR_STATUE_NO_SUITABLE_PLACE);

	if (flags.Test(DoCommandFlag::Execute)) {
		Backup<CompanyID> cur_company(_current_company, OWNER_NONE);
		Command<CMD_LANDSCAPE_CLEAR>::Do(DoCommandFlag::Execute, statue_data.best_position);
		cur_company.Restore();
		BuildObject(OBJECT_STATUE, statue_data.best_position, _current_company, t);
		t->statues.Set(_current_company); // Once found and built, "inform" the Town.
		MarkTileDirtyByTile(statue_data.best_position);
	}
	return CommandCost();
}

/**
 * Perform the "fund new buildings" town action.
 * @param t The town to fund buildings in.
 * @param flags Type of operation.
 * @return An empty cost.
 */
static CommandCost TownActionFundBuildings(Town *t, DoCommandFlags flags)
{
	/* Check if it's allowed to buy the rights */
	if (!_settings_game.economy.fund_buildings) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) {
		/* And grow for 3 months */
		t->fund_buildings_months = 3;

		/* Enable growth (also checking GameScript's opinion) */
		UpdateTownGrowth(t);

		/* Build a new house, but add a small delay to make sure
		 * that spamming funding doesn't let town grow any faster
		 * than 1 house per 2 * TOWN_GROWTH_TICKS ticks.
		 * Also emulate original behaviour when town was only growing in
		 * TOWN_GROWTH_TICKS intervals, to make sure that it's not too
		 * tick-perfect and gives player some time window where they can
		 * spam funding with the exact same efficiency.
		 */
		t->grow_counter = std::min<uint16_t>(t->grow_counter, 2 * Ticks::TOWN_GROWTH_TICKS - (t->growth_rate - t->grow_counter) % Ticks::TOWN_GROWTH_TICKS);

		SetWindowDirty(WC_TOWN_VIEW, t->index);
		SetWindowDirty(CM_WC_CB_TOWN, t->index);
	}
	return CommandCost();
}

/**
 * Perform the "buy exclusive transport rights" town action.
 * @param t The town to buy exclusivity in.
 * @param flags Type of operation.
 * @return An empty cost.
 */
static CommandCost TownActionBuyRights(Town *t, DoCommandFlags flags)
{
	/* Check if it's allowed to buy the rights */
	if (!_settings_game.economy.exclusive_rights) return CMD_ERROR;
	if (t->exclusivity != CompanyID::Invalid()) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) {
		t->exclusive_counter = 12;
		t->exclusivity = _current_company;

		ModifyStationRatingAround(t->xy, _current_company, 130, 17);

		SetWindowClassesDirty(WC_STATION_VIEW);

		/* Spawn news message */
		auto cni = std::make_unique<CompanyNewsInformation>(STR_NEWS_EXCLUSIVE_RIGHTS_TITLE, Company::Get(_current_company));
		EncodedString message = GetEncodedString(TimerGameEconomy::UsingWallclockUnits() ? STR_NEWS_EXCLUSIVE_RIGHTS_DESCRIPTION_MINUTES : STR_NEWS_EXCLUSIVE_RIGHTS_DESCRIPTION_MONTHS, t->index, cni->company_name);
		AddNewsItem(std::move(message),
			NewsType::General, NewsStyle::Company, {}, t->index, {}, std::move(cni));
		AI::BroadcastNewEvent(new ScriptEventExclusiveTransportRights(_current_company, t->index));
		Game::NewEvent(new ScriptEventExclusiveTransportRights(_current_company, t->index));
	}
	return CommandCost();
}

/**
 * Perform the "bribe" town action.
 * @param t The town to bribe.
 * @param flags Type of operation.
 * @return An empty cost.
 */
static CommandCost TownActionBribe(Town *t, DoCommandFlags flags)
{
	if (flags.Test(DoCommandFlag::Execute)) {
		if (Chance16(1, 14)) {
			/* set as unwanted for 6 months */
			t->unwanted[_current_company] = 6;

			/* set all close by station ratings to 0 */
			for (Station *st : Station::Iterate()) {
				if (st->town == t && st->owner == _current_company) {
					for (GoodsEntry &ge : st->goods) ge.rating = 0;
				}
			}

			/* only show error message to the executing player. All errors are handled command.c
			 * but this is special, because it can only 'fail' on a DoCommandFlag::Execute */
			if (IsLocalCompany()) ShowErrorMessage(GetEncodedString(STR_ERROR_BRIBE_FAILED), {}, WL_INFO);

			/* decrease by a lot!
			 * ChangeTownRating is only for stuff in demolishing. Bribe failure should
			 * be independent of any cheat settings
			 */
			if (t->ratings[_current_company] > RATING_BRIBE_DOWN_TO) {
				t->ratings[_current_company] = RATING_BRIBE_DOWN_TO;
				t->UpdateVirtCoord();
				SetWindowDirty(WC_TOWN_AUTHORITY, t->index);
			}
		} else {
			ChangeTownRating(t, RATING_BRIBE_UP_STEP, RATING_BRIBE_MAXIMUM, DoCommandFlag::Execute);
			if (t->exclusivity != _current_company && t->exclusivity != CompanyID::Invalid()) {
				t->exclusivity = CompanyID::Invalid();
				t->exclusive_counter = 0;
			}
		}
	}
	return CommandCost();
}

typedef CommandCost TownActionProc(Town *t, DoCommandFlags flags);
static TownActionProc * const _town_action_proc[] = {
	TownActionAdvertiseSmall,
	TownActionAdvertiseMedium,
	TownActionAdvertiseLarge,
	TownActionRoadRebuild,
	TownActionBuildStatue,
	TownActionFundBuildings,
	TownActionBuyRights,
	TownActionBribe
};
static_assert(std::size(_town_action_proc) == to_underlying(TownAction::End));

/**
 * Get a list of available town authority actions.
 * @param cid The company that is querying the town.
 * @param t The town that is queried.
 * @return The bitmasked value of enabled actions.
 */
TownActions GetMaskOfTownActions(CompanyID cid, const Town *t)
{
	TownActions buttons{};

	/* Spectators and unwanted have no options */
	if (cid != COMPANY_SPECTATOR && !(_settings_game.economy.bribe && t->unwanted[cid])) {

		/* Actions worth more than this are not able to be performed */
		Money avail = GetAvailableMoney(cid);

		/* Check the action bits for validity and
		 * if they are valid add them */
		for (TownAction cur = {}; cur != TownAction::End; ++cur) {

			/* Is the company prohibited from bribing ? */
			if (cur == TownAction::Bribe) {
				/* Company can't bribe if setting is disabled */
				if (!_settings_game.economy.bribe) continue;
				/* Company can bribe if another company has exclusive transport rights,
				 * or its standing with the town is less than outstanding. */
				if (t->ratings[cid] >= RATING_BRIBE_MAXIMUM) {
					if (t->exclusivity == _current_company) continue;
					if (t->exclusive_counter == 0) continue;
				}
			}

			/* Is the company not able to buy exclusive rights ? */
			if (cur == TownAction::BuyRights && (!_settings_game.economy.exclusive_rights || t->exclusive_counter != 0)) continue;

			/* Is the company not able to fund buildings ? */
			if (cur == TownAction::FundBuildings && !_settings_game.economy.fund_buildings) continue;

			/* Is the company not able to fund local road reconstruction? */
			if (cur == TownAction::RoadRebuild && !_settings_game.economy.fund_roads) continue;

			/* Is the company not able to build a statue ? */
			if (cur == TownAction::BuildStatue && t->statues.Test(cid)) continue;

			if (avail >= GetTownActionCost(cur) * _price[PR_TOWN_ACTION] >> 8) {
				buttons.Set(cur);
			}
		}
	}

	return buttons;
}

/**
 * Do a town action.
 * This performs an action such as advertising, building a statue, funding buildings,
 * but also bribing the town-council
 * @param flags type of operation
 * @param town_id town to do the action at
 * @param action action to perform, @see _town_action_proc for the list of available actions
 * @return the cost of this operation or an error
 */
CommandCost CmdDoTownAction(DoCommandFlags flags, TownID town_id, TownAction action)
{
	Town *t = Town::GetIfValid(town_id);
	if (t == nullptr || to_underlying(action) >= std::size(_town_action_proc)) return CMD_ERROR;

	if (!GetMaskOfTownActions(_current_company, t).Test(action)) return CMD_ERROR;

	CommandCost cost(EXPENSES_OTHER, _price[PR_TOWN_ACTION] * GetTownActionCost(action) >> 8);

	CommandCost ret = _town_action_proc[to_underlying(action)](t, flags);
	if (ret.Failed()) return ret;

	if (flags.Test(DoCommandFlag::Execute)) {
		SetWindowDirty(WC_TOWN_AUTHORITY, town_id);
	}

	return cost;
}

template <typename Func>
static void ForAllStationsNearTown(Town *t, Func func)
{
	/* Ideally the search radius should be close to the actual town zone 0 radius.
	 * The true radius is not stored or calculated anywhere, only the squared radius. */
	/* The efficiency of this search might be improved for large towns and many stations on the map,
	 * by using an integer square root approximation giving a value not less than the true square root. */
	uint search_radius = t->cache.squared_town_zone_radius[HZB_TOWN_EDGE] / 2;
	ForAllStationsRadius(t->xy, search_radius, [&](const Station * st) {
		if (DistanceSquare(st->xy, t->xy) <= t->cache.squared_town_zone_radius[HZB_TOWN_EDGE]) {
			func(st);
		}
	});
}

/**
 * Monthly callback to update town and station ratings.
 * @param t The town to update.
 */
static void UpdateTownRating(Town *t)
{
	/* Increase company ratings if they're low */
	for (const Company *c : Company::Iterate()) {
		if (t->ratings[c->index] < RATING_GROWTH_MAXIMUM) {
			t->ratings[c->index] = std::min((int)RATING_GROWTH_MAXIMUM, t->ratings[c->index] + RATING_GROWTH_UP_STEP);
		}
	}

	ForAllStationsNearTown(t, [&](const Station *st) {
		if (st->time_since_load <= 20 || st->time_since_unload <= 20) {
			if (Company::IsValidID(st->owner)) {
				int new_rating = t->ratings[st->owner] + RATING_STATION_UP_STEP;
				t->ratings[st->owner] = std::min<int>(new_rating, INT16_MAX); // do not let it overflow
			}
		} else {
			if (Company::IsValidID(st->owner)) {
				int new_rating = t->ratings[st->owner] + RATING_STATION_DOWN_STEP;
				t->ratings[st->owner] = std::max(new_rating, INT16_MIN);
			}
		}
	});

	/* clamp all ratings to valid values */
	for (uint i = 0; i < MAX_COMPANIES; i++) {
		t->ratings[i] = Clamp(t->ratings[i], RATING_MINIMUM, RATING_MAXIMUM);
	}

	t->UpdateVirtCoord();
	SetWindowDirty(WC_TOWN_AUTHORITY, t->index);
}


/**
 * Updates town grow counter after growth rate change.
 * Preserves relative house builting progress whenever it can.
 * @param t The town to calculate grow counter for
 * @param prev_growth_rate Town growth rate before it changed (one that was used with grow counter to be updated)
 */
static void UpdateTownGrowCounter(Town *t, uint16_t prev_growth_rate)
{
	if (t->growth_rate == TOWN_GROWTH_RATE_NONE) return;
	if (prev_growth_rate == TOWN_GROWTH_RATE_NONE) {
		t->grow_counter = std::min<uint16_t>(t->growth_rate, t->grow_counter);
		return;
	}
	t->grow_counter = RoundDivSU((uint32_t)t->grow_counter * (t->growth_rate + 1), prev_growth_rate + 1);
}

/**
 * Calculates amount of active stations in the range of town (HZB_TOWN_EDGE).
 * @param t The town to calculate stations for
 * @returns Amount of active stations
 */
static int CountActiveStations(Town *t)
{
	int n = 0;
	ForAllStationsNearTown(t, [&](const Station * st) {
		if (st->time_since_load <= 20 || st->time_since_unload <= 20) {
			n++;
		}
	});
	return n;
}

/**
 * Calculates town growth rate in normal conditions (custom growth rate not set).
 * If town growth speed is set to None(0) returns the same rate as if it was Normal(2).
 * @param t The town to calculate growth rate for
 * @returns Calculated growth rate
 */
static uint GetNormalGrowthRate(Town *t)
{
	/**
	 * Note:
	 * Unserviced+unfunded towns get an additional malus in UpdateTownGrowth(),
	 * so the "320" is actually not better than the "420".
	 */
	static const uint16_t _grow_count_values[2][6] = {
		{ 120, 120, 120, 100,  80,  60 }, // Fund new buildings has been activated
		{ 320, 420, 300, 220, 160, 100 }  // Normal values
	};

	int n = CountActiveStations(t);
	uint16_t m = _grow_count_values[t->fund_buildings_months != 0 ? 0 : 1][std::min(n, 5)];

	uint growth_multiplier = _settings_game.economy.town_growth_rate != 0 ? _settings_game.economy.town_growth_rate - 1 : 1;

	m >>= growth_multiplier;
	if (t->larger_town) m /= 2;

	return TownTicksToGameTicks(m / (t->cache.num_houses / 50 + 1));
}

/**
 * Updates town growth rate.
 * @param t The town to update growth rate for
 */
static void UpdateTownGrowthRate(Town *t)
{
	if (HasBit(t->flags, TOWN_CUSTOM_GROWTH)) return;
	uint old_rate = t->growth_rate;
	t->growth_rate = GetNormalGrowthRate(t);
	UpdateTownGrowCounter(t, old_rate);
	SetWindowDirty(WC_TOWN_VIEW, t->index);
}

/**
 * Updates town growth state (whether it is growing or not).
 * @param t The town to update growth for
 */
static void UpdateTownGrowth(Town *t)
{
	UpdateTownGrowthRate(t);

	ClrBit(t->flags, TOWN_IS_GROWING);
	SetWindowDirty(WC_TOWN_VIEW, t->index);
	SetWindowDirty(CM_WC_CB_TOWN, t->index);
	t->cm.growing_by_chance = false;

	if (_settings_game.economy.town_growth_rate == 0 && t->fund_buildings_months == 0) return;

	if (t->fund_buildings_months == 0) {
		/* Check if all goals are reached for this town to grow (given we are not funding it) */
		for (int i = TAE_BEGIN; i < TAE_END; i++) {
			switch (t->goal[i]) {
				case TOWN_GROWTH_WINTER:
					if (TileHeight(t->xy) >= GetSnowLine() && t->received[i].old_act == 0 && t->cache.population > 90) return;
					break;
				case TOWN_GROWTH_DESERT:
					if (GetTropicZone(t->xy) == TROPICZONE_DESERT && t->received[i].old_act == 0 && t->cache.population > 60) return;
					break;
				default:
					if (t->goal[i] > t->received[i].old_act) return;
					break;
			}
		}
	}

	if (HasBit(t->flags, TOWN_CUSTOM_GROWTH)) {
		if (t->growth_rate != TOWN_GROWTH_RATE_NONE) SetBit(t->flags, TOWN_IS_GROWING);
		SetWindowDirty(WC_TOWN_VIEW, t->index);
		SetWindowDirty(CM_WC_CB_TOWN, t->index);
		return;
	}

	if (t->fund_buildings_months == 0 && CountActiveStations(t) == 0) {
		if(!Chance16(1, 12)) return;
		t->cm.growing_by_chance = true;
	}

	SetBit(t->flags, TOWN_IS_GROWING);
	SetWindowDirty(WC_TOWN_VIEW, t->index);
	SetWindowDirty(CM_WC_CB_TOWN, t->index);
}

/**
 * Checks whether the local authority allows construction of a new station (rail, road, airport, dock) on the given tile
 * @param tile The tile where the station shall be constructed.
 * @param flags Command flags. DoCommandFlag::NoTestTownRating is tested.
 * @return Succeeded or failed command.
 */
CommandCost CheckIfAuthorityAllowsNewStation(TileIndex tile, DoCommandFlags flags)
{
	/* The required rating is hardcoded to RATING_VERYPOOR (see below), not the authority attitude setting, so we can bail out like this. */
	if (_settings_game.difficulty.town_council_tolerance == TOWN_COUNCIL_PERMISSIVE) return CommandCost();

	if (!Company::IsValidID(_current_company) || flags.Test(DoCommandFlag::NoTestTownRating)) return CommandCost();

	Town *t = ClosestTownFromTile(tile, _settings_game.economy.dist_local_authority);
	if (t == nullptr) return CommandCost();

	if (t->ratings[_current_company] > RATING_VERYPOOR) return CommandCost();

	return CommandCostWithParam(STR_ERROR_LOCAL_AUTHORITY_REFUSES_TO_ALLOW_THIS, t->index);
}

/**
 * Return the town closest to the given tile within \a threshold.
 * @param tile      Starting point of the search.
 * @param threshold Biggest allowed distance to the town.
 * @return Closest town to \a tile within \a threshold, or \c nullptr if there is no such town.
 *
 * @note This function only uses distance, the #ClosestTownFromTile function also takes town ownership into account.
 */
Town *CalcClosestTownFromTile(TileIndex tile, uint threshold)
{
	if (Town::GetNumItems() == 0) return nullptr;

	TownID tid = _town_kdtree.FindNearest(TileX(tile), TileY(tile));
	Town *town = Town::Get(tid);
	if (DistanceManhattan(tile, town->xy) < threshold) return town;
	return nullptr;
}

/**
 * Return the town closest (in distance or ownership) to a given tile, within a given threshold.
 * @param tile      Starting point of the search.
 * @param threshold Biggest allowed distance to the town.
 * @return Closest town to \a tile within \a threshold, or \c nullptr if there is no such town.
 *
 * @note If you only care about distance, you can use the #CalcClosestTownFromTile function.
 */
Town *ClosestTownFromTile(TileIndex tile, uint threshold)
{
	switch (GetTileType(tile)) {
		case MP_ROAD:
			if (IsRoadDepot(tile)) return CalcClosestTownFromTile(tile, threshold);

			if (!HasTownOwnedRoad(tile)) {
				TownID tid = GetTownIndex(tile);

				if (tid == TownID::Invalid()) {
					/* in the case we are generating "many random towns", this value may be TownID::Invalid() */
					if (_generating_world) return CalcClosestTownFromTile(tile, threshold);
					assert(Town::GetNumItems() == 0);
					return nullptr;
				}

				assert(Town::IsValidID(tid));
				Town *town = Town::Get(tid);

				if (DistanceManhattan(tile, town->xy) >= threshold) town = nullptr;

				return town;
			}
			[[fallthrough]];

		case MP_HOUSE:
			return Town::GetByTile(tile);

		default:
			return CalcClosestTownFromTile(tile, threshold);
	}
}

static bool _town_rating_test = false; ///< If \c true, town rating is in test-mode.
static std::map<const Town *, int> _town_test_ratings; ///< Map of towns to modified ratings, while in town rating test-mode.

/**
 * Switch the town rating to test-mode, to allow commands to be tested without affecting current ratings.
 * The function is safe to use in nested calls.
 * @param mode Test mode switch (\c true means go to test-mode, \c false means leave test-mode).
 */
void SetTownRatingTestMode(bool mode)
{
	static int ref_count = 0; // Number of times test-mode is switched on.
	if (mode) {
		if (ref_count == 0) {
			_town_test_ratings.clear();
		}
		ref_count++;
	} else {
		assert(ref_count > 0);
		ref_count--;
	}
	_town_rating_test = !(ref_count == 0);
}

/**
 * Get the rating of a town for the #_current_company.
 * @param t Town to get the rating from.
 * @return Rating of the current company in the given town.
 */
static int GetRating(const Town *t)
{
	if (_town_rating_test) {
		auto it = _town_test_ratings.find(t);
		if (it != _town_test_ratings.end()) {
			return it->second;
		}
	}
	return t->ratings[_current_company];
}

/**
 * Changes town rating of the current company
 * @param t Town to affect
 * @param add Value to add
 * @param max Minimum (add < 0) resp. maximum (add > 0) rating that should be achievable with this change.
 * @param flags Command flags, especially DoCommandFlag::NoModifyTownRating is tested
 */
void ChangeTownRating(Town *t, int add, int max, DoCommandFlags flags)
{
	/* if magic_bulldozer cheat is active, town doesn't penalize for removing stuff */
	if (t == nullptr || flags.Test(DoCommandFlag::NoModifyTownRating) ||
			!Company::IsValidID(_current_company) ||
			(_cheats.magic_bulldozer.value && add < 0)) {
		return;
	}

	int rating = GetRating(t);
	if (add < 0) {
		if (rating > max) {
			rating += add;
			if (rating < max) rating = max;
		}
	} else {
		if (rating < max) {
			rating += add;
			if (rating > max) rating = max;
		}
	}
	if (_town_rating_test) {
		_town_test_ratings[t] = rating;
	} else {
		t->have_ratings.Set(_current_company);
		t->ratings[_current_company] = rating;
		t->UpdateVirtCoord();
		SetWindowDirty(WC_TOWN_AUTHORITY, t->index);
	}
}

/**
 * Does the town authority allow the (destructive) action of the current company?
 * @param flags Checking flags of the command.
 * @param t     Town that must allow the company action.
 * @param type  Type of action that is wanted.
 * @return A succeeded command if the action is allowed, a failed command if it is not allowed.
 */
CommandCost CheckforTownRating(DoCommandFlags flags, Town *t, TownRatingCheckType type)
{
	/* if magic_bulldozer cheat is active, town doesn't restrict your destructive actions */
	if (t == nullptr || !Company::IsValidID(_current_company) ||
			_cheats.magic_bulldozer.value || flags.Test(DoCommandFlag::NoTestTownRating)) {
		return CommandCost();
	}

	/* minimum rating needed to be allowed to remove stuff */
	static const int needed_rating[][TOWN_RATING_CHECK_TYPE_COUNT] = {
		/*                  ROAD_REMOVE,                    TUNNELBRIDGE_REMOVE */
		{    RATING_ROAD_NEEDED_LENIENT,    RATING_TUNNEL_BRIDGE_NEEDED_LENIENT}, // Lenient
		{    RATING_ROAD_NEEDED_NEUTRAL,    RATING_TUNNEL_BRIDGE_NEEDED_NEUTRAL}, // Neutral
		{    RATING_ROAD_NEEDED_HOSTILE,    RATING_TUNNEL_BRIDGE_NEEDED_HOSTILE}, // Hostile
		{ RATING_ROAD_NEEDED_PERMISSIVE, RATING_TUNNEL_BRIDGE_NEEDED_PERMISSIVE}, // Permissive
	};

	/* check if you're allowed to remove the road/bridge/tunnel
	 * owned by a town no removal if rating is lower than ... depends now on
	 * difficulty setting. Minimum town rating selected by difficulty level
	 */
	int needed = needed_rating[_settings_game.difficulty.town_council_tolerance][type];

	if (GetRating(t) < needed) {
		return CommandCostWithParam(STR_ERROR_LOCAL_AUTHORITY_REFUSES_TO_ALLOW_THIS, t->index);
	}

	return CommandCost();
}

static IntervalTimer<TimerGameEconomy> _economy_towns_monthly({TimerGameEconomy::MONTH, TimerGameEconomy::Priority::TOWN}, [](auto)
{
	for (Town *t : Town::Iterate()) {
		/* Check for active town actions and decrement their counters. */
		if (t->road_build_months != 0) t->road_build_months--;
		if (t->fund_buildings_months != 0) t->fund_buildings_months--;

		if (t->exclusive_counter != 0) {
			if (--t->exclusive_counter == 0) t->exclusivity = CompanyID::Invalid();
		}

		/* Check for active failed bribe cooloff periods and decrement them. */
		for (const Company *c : Company::Iterate()) {
			if (t->unwanted[c->index] > 0) t->unwanted[c->index]--;
		}

		/* Update cargo statistics. */
		for (auto &supplied : t->supplied) supplied.NewMonth();
		for (auto &received : t->received) received.NewMonth();

		UpdateTownGrowth(t);
		UpdateTownRating(t);

		/* CityMania code start */
		if (CB_Enabled() && !t->larger_town) CB_UpdateTownStorage(t); //CB
		DoRegularFunding(t); // TODO shouldn't that be on tick / day base?
		/* CityMania code end */

		SetWindowDirty(WC_TOWN_VIEW, t->index);
		SetWindowDirty(CM_WC_CB_TOWN, t->index);
	}
});

static IntervalTimer<TimerGameEconomy> _economy_towns_yearly({TimerGameEconomy::YEAR, TimerGameEconomy::Priority::TOWN}, [](auto)
{
	/* Increment house ages */
	for (const auto t : Map::Iterate()) {
		if (!IsTileType(t, MP_HOUSE)) continue;
		IncrementHouseAge(t);
	}
});

static CommandCost TerraformTile_Town(TileIndex tile, DoCommandFlags flags, int z_new, Slope tileh_new)
{
	if (AutoslopeEnabled()) {
		HouseID house = GetHouseType(tile);
		GetHouseNorthPart(house); // modifies house to the ID of the north tile
		const HouseSpec *hs = HouseSpec::Get(house);

		/* Here we differ from TTDP by checking BuildingFlag::NotSloped */
		if (!hs->building_flags.Test(BuildingFlag::NotSloped) && !IsSteepSlope(tileh_new) &&
				(GetTileMaxZ(tile) == z_new + GetSlopeMaxZ(tileh_new))) {
			bool allow_terraform = true;

			/* Call the autosloping callback per tile, not for the whole building at once. */
			house = GetHouseType(tile);
			hs = HouseSpec::Get(house);
			if (hs->callback_mask.Test(HouseCallbackMask::Autoslope)) {
				/* If the callback fails, allow autoslope. */
				uint16_t res = GetHouseCallback(CBID_HOUSE_AUTOSLOPE, 0, 0, house, Town::GetByTile(tile), tile);
				if (res != CALLBACK_FAILED && ConvertBooleanCallback(hs->grf_prop.grffile, CBID_HOUSE_AUTOSLOPE, res)) allow_terraform = false;
			}

			if (allow_terraform) return CommandCost(EXPENSES_CONSTRUCTION, _price[PR_BUILD_FOUNDATION]);
		}
	}

	return Command<CMD_LANDSCAPE_CLEAR>::Do(flags, tile);
}

/** Tile callback functions for a town */
extern const TileTypeProcs _tile_type_town_procs = {
	DrawTile_Town,           // draw_tile_proc
	GetSlopePixelZ_Town,     // get_slope_z_proc
	ClearTile_Town,          // clear_tile_proc
	AddAcceptedCargo_Town,   // add_accepted_cargo_proc
	GetTileDesc_Town,        // get_tile_desc_proc
	GetTileTrackStatus_Town, // get_tile_track_status_proc
	nullptr,                    // click_tile_proc
	AnimateTile_Town,        // animate_tile_proc
	TileLoop_Town,           // tile_loop_proc
	ChangeTileOwner_Town,    // change_tile_owner_proc
	AddProducedCargo_Town,   // add_produced_cargo_proc
	nullptr,                    // vehicle_enter_tile_proc
	GetFoundation_Town,      // get_foundation_proc
	TerraformTile_Town,      // terraform_tile_proc
};

std::span<const DrawBuildingsTileStruct> GetTownDrawTileData()
{
	return _town_draw_tile_data;
}

namespace citymania {
	auto UpdateTownGrowthRate = &::UpdateTownGrowthRate;
}
