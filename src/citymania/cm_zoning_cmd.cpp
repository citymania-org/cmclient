#include "../stdafx.h"

#include "cm_zoning.hpp"

#include "cm_highlight.hpp"
#include "cm_game.hpp"
#include "cm_main.hpp"

#include "../station_base.h"
#include "../industry.h"
#include "../viewport_func.h"
#include "../town.h"
#include "../genworld.h"

#include <algorithm>
#include <vector>

#include "../safeguards.h"

namespace citymania {

Zoning _zoning = {EvaluationMode::CHECKNOTHING, EvaluationMode::CHECKNOTHING};
static const SpriteID INVALID_SPRITE_ID = UINT_MAX;
//RED GREEN BLACK LIGHT_BLUE ORANGE WHITE YELLOW PURPLE

TileIndex _closest_cache_ref = INVALID_TILE;
const uint CLOSEST_CACHE_THRESHOLD = 128;
typedef std::pair<uint, Town*>ClosestTownRecord;
std::vector<ClosestTownRecord> _closest_cache;


void RebuildClosestHash(TileIndex tile) {
	_closest_cache_ref = INVALID_TILE;
	_closest_cache.clear();
	for (Town *t : Town::Iterate()) {
		_closest_cache.push_back(std::make_pair(
		    DistanceManhattan(t->xy, tile), t));
	}
	std::sort(
	    _closest_cache.begin(), _closest_cache.end(),
		[](ClosestTownRecord &a, ClosestTownRecord &b) -> bool {
	    	return a.first < b.first;
		}
	);

	_closest_cache_ref = tile;
}


Town *CMCalcClosestTownFromTile(TileIndex tile, uint threshold = INT_MAX)
{
	if (_closest_cache_ref == INVALID_TILE
	    	|| DistanceManhattan(_closest_cache_ref, tile) > CLOSEST_CACHE_THRESHOLD) {
		RebuildClosestHash(tile);
		// RebuildClosestHash(TileXY(
		//     TileX(tile) + CLOSEST_CACHE_THRESHOLD / 2,
		//     TileY(tile) + CLOSEST_CACHE_THRESHOLD / 2));
	}
	int ref_dist = DistanceManhattan(_closest_cache_ref, tile);

	uint best = threshold;
	Town *best_town = NULL;

	for (auto p: _closest_cache) {
		if (p.first > best + ref_dist)
			break;
		uint dist = DistanceManhattan(tile, p.second->xy);
		if (dist < best) {
			best = dist;
			best_town = p.second;
		}
	}

	return best_town;
}

// Copy ClosestTownFromTile but uses CMCalcClosestTownFromTile
Town *CMClosestTownFromTile(TileIndex tile, uint threshold)
{
	switch (GetTileType(tile)) {
		case MP_ROAD:
			if (IsRoadDepot(tile)) return CalcClosestTownFromTile(tile, threshold);

			if (!HasTownOwnedRoad(tile)) {
				TownID tid = GetTownIndex(tile);

				if (tid == (TownID)INVALID_TOWN) {
					/* in the case we are generating "many random towns", this value may be INVALID_TOWN */
					if (_generating_world) return CalcClosestTownFromTile(tile, threshold);
					assert(Town::GetNumItems() == 0);
					return NULL;
				}

				assert(Town::IsValidID(tid));
				Town *town = Town::Get(tid);

				if (DistanceManhattan(tile, town->xy) >= threshold) town = NULL;

				return town;
			}
			[[ fallthrough ]];

		case MP_HOUSE:
			return Town::GetByTile(tile);

		default:
			return CMCalcClosestTownFromTile(tile, threshold);
	}
}

/**
 * Draw the zoning sprites.
 * @param SpriteID image
 *        the image
 * @param SpriteID colour
 *        the colour of the zoning
 * @param TileInfo ti
 *        the tile
 */
const uint8_t _tileh_to_sprite[32] = {
	0, 1, 2, 3, 4, 5, 6,  7, 8, 9, 10, 11, 12, 13, 14, 0,
	0, 0, 0, 0, 0, 0, 0, 16, 0, 0,  0, 17,  0, 15, 18, 0,
};

void DrawZoningSprites(SpriteID image, SpriteID colour, const TileInfo *ti) {
	if (colour != INVALID_SPRITE_ID){
		AddSortableSpriteToDraw(image + _tileh_to_sprite[ti->tileh], colour, ti->x, ti->y, 0x10, 0x10, 1, ti->z + 7);
	}
}

/**
 * Detect whether this area is within the acceptance of any station.
 * @param TileArea area
 *        the area to search by
 * @return true if a station is found
 */
bool IsAreaWithinAcceptanceZoneOfStation(TileArea area) {
	int catchment = _settings_game.station.station_spread + (_settings_game.station.modified_catchment ? MAX_CATCHMENT : CA_UNMODIFIED);

	StationFinder morestations(TileArea(TileXY(TileX(area.tile) - catchment / 2, TileY(area.tile) - catchment / 2),
		TileX(area.tile) + area.w + catchment, TileY(area.tile) + area.h + catchment));

	for (Station *st: morestations.GetStations()) {
		Rect rect = st->GetCatchmentRect();
		return TileArea(TileXY(rect.left, rect.top), TileXY(rect.right, rect.bottom)).Intersects(area);
	}
	return false;
}

/**
 * Detect whether this tile is within the acceptance of any station.
 * @param TileIndex tile
 *        the tile to search by
 * @return true if a station is found
 */
bool IsTileWithinAcceptanceZoneOfStation(TileIndex tile) {
	StationFinder morestations(TileArea(tile, 1, 1));

	for (Station *st: morestations.GetStations()) {
		if (st->TileIsInCatchment(tile))
			return true;
	}
	return false;
}

//Check the opinion of the local authority in the tile.
SpriteID TileZoneCheckOpinionEvaluation(TileIndex tile, Owner owner) {
	Town *town = CMClosestTownFromTile(tile, _settings_game.economy.dist_local_authority);

	if (town == NULL) return INVALID_SPRITE_ID; // no town
	else if (HasBit(town->have_ratings, owner)) {  // good : bad
		int16 rating = town->ratings[owner];
		if(rating <= RATING_APPALLING) return CM_SPR_PALETTE_ZONING_RED;
		if(rating <= RATING_POOR) return CM_SPR_PALETTE_ZONING_ORANGE;
		if(rating <= RATING_MEDIOCRE) return CM_SPR_PALETTE_ZONING_YELLOW;
		if(rating <= RATING_GOOD) return CM_SPR_PALETTE_ZONING_WHITE;
		if(rating <= RATING_VERYGOOD) return CM_SPR_PALETTE_ZONING_PURPLE;
		if(rating <= RATING_EXCELLENT) return CM_SPR_PALETTE_ZONING_LIGHT_BLUE;
		return CM_SPR_PALETTE_ZONING_GREEN;
	}
	else {
		return CM_SPR_PALETTE_ZONING_BLACK;      // no opinion
	}
}

//Check whether the player can build in tile.
SpriteID TileZoneCheckBuildEvaluation(TileIndex tile, Owner owner) {
	/* Let's first check for the obvious things you cannot build on */
	switch ( GetTileType(tile) ) {
		case MP_INDUSTRY:
		case MP_OBJECT:
		case MP_HOUSE:
			return CM_SPR_PALETTE_ZONING_RED; //can't build
		case MP_STATION:
		case MP_TUNNELBRIDGE:
		case MP_ROAD:
		case MP_RAILWAY: {
			if (GetTileOwner(tile) != owner) return CM_SPR_PALETTE_ZONING_RED; //can't build
			else return INVALID_SPRITE_ID;
		}
		default: return INVALID_SPRITE_ID;
	}
}

//Detect whether the tile is within the catchment zone of a station.
//black if within, light blue if only in acceptance zone and nothing if no nearby station.
 SpriteID TileZoneCheckStationCatchmentEvaluation(TileIndex tile) {
	// Never on a station.
	if (IsTileType(tile, MP_STATION)){
		return INVALID_SPRITE_ID;
	}
	return INVALID_SPRITE_ID;
}

//Detect whether a building is unserved by a station of owner.
//return red if unserved, orange if only accepting, nothing if served or not a building
SpriteID TileZoneCheckUnservedBuildingsEvaluation(TileIndex tile) {
	if (IsTileType (tile, MP_HOUSE))
	{
		StationFinder stations(TileArea(tile, 1, 1));

		if (!stations.GetStations().empty()) {
			return INVALID_SPRITE_ID;
		}
		return CM_SPR_PALETTE_ZONING_RED;
	}
	return INVALID_SPRITE_ID;
}

//Detect whether an industry is unserved by a station of owner.
//return red if unserved, orange if only accepting, nothing if served or not a building
SpriteID TileZoneCheckUnservedIndustriesEvaluation(TileIndex tile) {
	if (IsTileType(tile, MP_INDUSTRY)) {
		Industry *ind = Industry::GetByTile(tile);
		StationFinder stations(ind->location);

		if (!stations.GetStations().empty()){
			return INVALID_SPRITE_ID;
		}

		return CM_SPR_PALETTE_ZONING_RED;
	}
	return INVALID_SPRITE_ID;
}

//Check which town zone tile belongs to.
SpriteID TileZoneCheckTownZones(TileIndex tile) {
	HouseZonesBits next_zone = HZB_BEGIN, tz = HZB_END;

	for (Town *town : Town::Iterate()) {
		uint dist = DistanceSquare(tile, town->xy);
		// town code uses <= for checking town borders (tz0) but < for other zones
		while (next_zone < HZB_END
			&& (town->cache.squared_town_zone_radius[next_zone] == 0
				|| dist <= town->cache.squared_town_zone_radius[next_zone] - (next_zone == HZB_BEGIN ? 0 : 1))
		){
			if(town->cache.squared_town_zone_radius[next_zone] != 0)  tz = next_zone;
			next_zone++;
		}
	}

	switch (tz) {
		case HZB_TOWN_EDGE:         return CM_SPR_PALETTE_ZONING_LIGHT_BLUE; // Tz0
		case HZB_TOWN_OUTSKIRT:     return CM_SPR_PALETTE_ZONING_RED; // Tz1
		case HZB_TOWN_OUTER_SUBURB: return CM_SPR_PALETTE_ZONING_YELLOW; // Tz2
		case HZB_TOWN_INNER_SUBURB: return CM_SPR_PALETTE_ZONING_GREEN; // Tz3
		case HZB_TOWN_CENTRE:       return CM_SPR_PALETTE_ZONING_WHITE; // Tz4 - center
		default:                    return INVALID_SPRITE_ID; // no town
	}
	return INVALID_SPRITE_ID;
}

//Check CB town acceptance area
SpriteID TileZoneCheckNewCBBorders(TileIndex tile) {
	for (Town *town : Town::Iterate()) {
		if (DistanceSquare(tile, town->xy) <= town->cache.squared_town_zone_radius[0] + 30)
			return CM_SPR_PALETTE_ZONING_BLACK;
	}
	return INVALID_SPRITE_ID;
}

//Check CB town acceptance area
SpriteID TileZoneCheckCBBorders(TileIndex tile) {
	for (Town *town : Town::Iterate()) {
		if (DistanceMax(town->xy, tile) <= _settings_game.citymania.cb.acceptance_range)
			return CM_SPR_PALETTE_ZONING_LIGHT_BLUE;
	}
	return INVALID_SPRITE_ID; // no town
}

//Check whether the tile is within citybuilder server town border (where houses could be built)
SpriteID TileZoneCheckCBTownBorders(TileIndex tile) {
	for (Town *town : Town::Iterate()) {
		uint32 distMax = DistanceMax(town->xy, tile);
		if (distMax * distMax < town->cache.squared_town_zone_radius[0]){
			return CM_SPR_PALETTE_ZONING_GREEN;
		}
	}
	return INVALID_SPRITE_ID;
}

//Check which advertisement zone(small, medium, large) tile belongs to
SpriteID TileZoneCheckTownAdvertisementZones(TileIndex tile) {
	Town *town = CMCalcClosestTownFromTile(tile, 21U);
	if (town == NULL) return INVALID_SPRITE_ID; //nothing

	uint dist = DistanceManhattan(town->xy, tile);

	if (dist <= 10) return CM_SPR_PALETTE_ZONING_GREEN;
	if (dist <= 15) return CM_SPR_PALETTE_ZONING_YELLOW;
	if (dist <= 20) return CM_SPR_PALETTE_ZONING_LIGHT_BLUE;
	return INVALID_SPRITE_ID;
}

//Checks for tile in growth tiles info
SpriteID TileZoneCheckTownsGrowthTiles(TileIndex tile) {
	switch (citymania::_game->get_town_growth_tile(tile)) {
		// case TGTS_CB_HOUSE_REMOVED_NOGROW: return SPR_PALETTE_ZONING_LIGHT_BLUE;
		case citymania::TownGrowthTileState::RH_REMOVED:   return CM_SPR_PALETTE_ZONING_LIGHT_BLUE;
		case citymania::TownGrowthTileState::RH_REBUILT:   return CM_SPR_PALETTE_ZONING_WHITE;
		case citymania::TownGrowthTileState::NEW_HOUSE:    return CM_SPR_PALETTE_ZONING_GREEN;
		case citymania::TownGrowthTileState::CS:           return CM_SPR_PALETTE_ZONING_ORANGE;
		case citymania::TownGrowthTileState::HS:           return CM_SPR_PALETTE_ZONING_YELLOW;
		case citymania::TownGrowthTileState::HR:           return CM_SPR_PALETTE_ZONING_RED;
		default: return INVALID_SPRITE_ID;
	}
}

// For station shows whether it is active
SpriteID TileZoneCheckActiveStations(TileIndex tile) {
	// Never on a station.
	if (!IsTileType(tile, MP_STATION)){
		return INVALID_SPRITE_ID;
	}

	Station *st = Station::GetByTile(tile);
	if (st == NULL) {
		return INVALID_SPRITE_ID;
	}

	if (st->time_since_load <= 20 || st->time_since_unload <= 20) {
		return CM_SPR_PALETTE_ZONING_GREEN;
	}

	return CM_SPR_PALETTE_ZONING_RED;
}


/**
 * General evaluation function; calls all the other functions depending on
 * evaluation mode.
 * @param TileIndex tile
 *        Tile to be evaluated.
 * @param EvaluationMode ev_mode
 *        The current evaluation mode.
 * @return The colour returned by the evaluation functions (none if no ev_mode).
 */
SpriteID TileZoningSpriteEvaluation(TileIndex tile, Owner owner, EvaluationMode ev_mode) {
	switch (ev_mode) {
		case EvaluationMode::CHECKOPINION:     return TileZoneCheckOpinionEvaluation(tile, owner);
		case EvaluationMode::CHECKBUILD:       return TileZoneCheckBuildEvaluation(tile, owner);
		case EvaluationMode::CHECKSTACATCH:    return TileZoneCheckStationCatchmentEvaluation(tile);
		case EvaluationMode::CHECKBULUNSER:    return TileZoneCheckUnservedBuildingsEvaluation(tile);
		case EvaluationMode::CHECKINDUNSER:    return TileZoneCheckUnservedIndustriesEvaluation(tile);
		case EvaluationMode::CHECKTOWNZONES:   return TileZoneCheckTownZones(tile);
		case EvaluationMode::CHECKCBACCEPTANCE: return TileZoneCheckCBBorders(tile);
		case EvaluationMode::CHECKCBTOWNLIMIT:   return TileZoneCheckNewCBBorders(tile);
		case EvaluationMode::CHECKTOWNADZONES: return TileZoneCheckTownAdvertisementZones(tile);
		case EvaluationMode::CHECKTOWNGROWTHTILES: return TileZoneCheckTownsGrowthTiles(tile);
		case EvaluationMode::CHECKACTIVESTATIONS: return TileZoneCheckActiveStations(tile);
		default:               return INVALID_SPRITE_ID;
	}
}

SpriteID GetTownZoneBorderColor(uint8 zone) {
	switch (zone) {
		default: return CM_SPR_PALETTE_ZONING_WHITE;  // Tz0
		case 2: return CM_SPR_PALETTE_ZONING_YELLOW;  // Tz1
		case 3: return CM_SPR_PALETTE_ZONING_ORANGE;  // Tz2
		case 4: return CM_SPR_PALETTE_ZONING_ORANGE;  // Tz3
		case 5: return CM_SPR_PALETTE_ZONING_RED;  // Tz4 - center
	};
	switch (zone) {
		default: return CM_SPR_PALETTE_ZONING_LIGHT_BLUE;  // Tz0
		case 2: return CM_SPR_PALETTE_ZONING_RED;  // Tz1
		case 3: return CM_SPR_PALETTE_ZONING_YELLOW;  // Tz2
		case 4: return CM_SPR_PALETTE_ZONING_GREEN;  // Tz3
		case 5: return CM_SPR_PALETTE_ZONING_WHITE;  // Tz4 - center
	};
}


/**
 * Draw the the zoning on the tile.
 * @param TileInfo ti
 *        the tile to draw on.
 */
void DrawTileZoning(const TileInfo *ti) {

	if(_zoning.outer == EvaluationMode::CHECKNOTHING && _zoning.inner == EvaluationMode::CHECKNOTHING) return; //nothing to do
	if (_game_mode != GM_NORMAL || ti->tile >= Map::Size() || IsTileType(ti->tile, MP_VOID)) return; //check invalid
	if (_zoning.outer != EvaluationMode::CHECKNOTHING){
		if (_zoning.outer == EvaluationMode::CHECKTOWNZONES ||
			    _zoning.outer == EvaluationMode::CHECKBULUNSER ||
			    _zoning.outer == EvaluationMode::CHECKINDUNSER ||
			    _zoning.outer == EvaluationMode::CHECKTOWNADZONES ||
				_zoning.outer == EvaluationMode::CHECKSTACATCH ||
				_zoning.outer == EvaluationMode::CHECKCBACCEPTANCE ||
				_zoning.outer == EvaluationMode::CHECKCBTOWNLIMIT ||
				_zoning.outer == EvaluationMode::CHECKACTIVESTATIONS ||
				_zoning.outer == EvaluationMode::CHECKTOWNGROWTHTILES) {
			// handled by citymania zoning
		} else {
			DrawZoningSprites(SPR_SELECT_TILE, TileZoningSpriteEvaluation(ti->tile, _local_company, _zoning.outer), ti);
		}
	}
	if (_zoning.inner != EvaluationMode::CHECKNOTHING){
		DrawZoningSprites(CM_SPR_INNER_HIGHLIGHT_BASE, TileZoningSpriteEvaluation(ti->tile, _local_company, _zoning.inner), ti);
	}
}

} // namespace citymania
