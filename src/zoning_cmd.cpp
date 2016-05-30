/** @file zoning_cmd.cpp */
#include "stdafx.h"
#include "station_base.h"
#include "industry.h"
#include "viewport_func.h"
#include "town.h"
#include "zoning.h"

Zoning _zoning = {CHECKNOTHING, CHECKNOTHING};
static const SpriteID INVALID_SPRITE_ID = UINT_MAX;
//RED GREEN BLACK LIGHT_BLUE ORANGE WHITE YELLOW PURPLE

/**
 * Draw the zoning sprites.
 * @param SpriteID image
 *        the image
 * @param SpriteID colour
 *        the colour of the zoning
 * @param TileInfo ti
 *        the tile
 */
const byte _tileh_to_sprite[32] = {
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

	for (Station * const *st_iter = morestations.GetStations()->Begin(); st_iter != morestations.GetStations()->End(); ++st_iter) {
		Station *st = *st_iter;
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
	int catchment = _settings_game.station.station_spread + (_settings_game.station.modified_catchment ? MAX_CATCHMENT : CA_UNMODIFIED);

	StationFinder morestations(TileArea(TileXY(TileX(tile) - catchment / 2, TileY(tile) - catchment / 2),
		catchment, catchment));

	for (Station * const *st_iter = morestations.GetStations()->Begin(); st_iter != morestations.GetStations()->End(); ++st_iter) {
		Station *st = *st_iter;
		Rect rect = st->GetCatchmentRect();
		if ((uint)rect.left <= TileX(tile) && TileX(tile) <= (uint)rect.right
			&& (uint)rect.top <= TileY(tile) && TileY(tile) <= (uint)rect.bottom )
		{
			return true;
		}
	}
	return false;
}

//Check the opinion of the local authority in the tile.
SpriteID TileZoneCheckOpinionEvaluation(TileIndex tile, Owner owner) {
	Town *town = ClosestTownFromTile(tile, _settings_game.economy.dist_local_authority);

	if (town == NULL) return INVALID_SPRITE_ID; // no town
	else if (HasBit(town->have_ratings, owner)) {  // good : bad
		int16 rating = town->ratings[owner];
		if(rating <= RATING_APPALLING) return SPR_PALETTE_ZONING_RED;
		if(rating <= RATING_POOR) return SPR_PALETTE_ZONING_ORANGE;
		if(rating <= RATING_MEDIOCRE) return SPR_PALETTE_ZONING_YELLOW;
		if(rating <= RATING_GOOD) return SPR_PALETTE_ZONING_WHITE;
		if(rating <= RATING_VERYGOOD) return SPR_PALETTE_ZONING_PURPLE;
		if(rating <= RATING_EXCELLENT) return SPR_PALETTE_ZONING_LIGHT_BLUE;
		return SPR_PALETTE_ZONING_GREEN;
	}
	else {
		return SPR_PALETTE_ZONING_BLACK;      // no opinion
	}
}

//Check whether the player can build in tile.
SpriteID TileZoneCheckBuildEvaluation(TileIndex tile, Owner owner) {
	/* Let's first check for the obvious things you cannot build on */
	switch ( GetTileType(tile) ) {
		case MP_INDUSTRY:
		case MP_OBJECT:
		case MP_HOUSE:
			return SPR_PALETTE_ZONING_RED; //can't build
		case MP_STATION:
		case MP_TUNNELBRIDGE:
		case MP_ROAD:
		case MP_RAILWAY: {
			if (GetTileOwner(tile) != owner) return SPR_PALETTE_ZONING_RED; //can't build
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
	// For provided goods
	StationFinder stations(TileArea(tile, 1, 1));
	if (stations.GetStations()->Length() > 0) {
		return SPR_PALETTE_ZONING_GREEN;
	}
	// For accepted goods
	if (IsTileWithinAcceptanceZoneOfStation(tile)){
		return SPR_PALETTE_ZONING_LIGHT_BLUE;
	}
	return INVALID_SPRITE_ID;
}

//Detect whether a building is unserved by a station of owner.
//return red if unserved, orange if only accepting, nothing if served or not a building
SpriteID TileZoneCheckUnservedBuildingsEvaluation(TileIndex tile) {
	CargoArray dat;

	if (IsTileType (tile, MP_HOUSE))
		//&& ( ( memset(&dat, 0, sizeof(dat)), AddAcceptedCargo(tile, dat, NULL), (dat[CT_MAIL] + dat[CT_PASSENGERS] > 0) )
		//	|| ( memset(&dat, 0, sizeof(dat)), AddProducedCargo(tile, dat), (dat[CT_MAIL] + dat[CT_PASSENGERS] > 0) ) ) )
	{
		StationFinder stations(TileArea(tile, 1, 1));

		if (stations.GetStations()->Length() > 0) {
			return INVALID_SPRITE_ID;
		}
		// For accepted goods
		if (IsTileWithinAcceptanceZoneOfStation(tile)){
			return SPR_PALETTE_ZONING_ORANGE;
		}
		return SPR_PALETTE_ZONING_RED;
	}
	return INVALID_SPRITE_ID;
}

//Detect whether an industry is unserved by a station of owner.
//return red if unserved, orange if only accepting, nothing if served or not a building
SpriteID TileZoneCheckUnservedIndustriesEvaluation(TileIndex tile) {
	if (IsTileType(tile, MP_INDUSTRY)) {
		Industry *ind = Industry::GetByTile(tile);
		StationFinder stations(ind->location);

		if (stations.GetStations()->Length() > 0){
			return INVALID_SPRITE_ID;
		}

		// For accepted goods
		if (IsAreaWithinAcceptanceZoneOfStation(ind->location)){
			return SPR_PALETTE_ZONING_ORANGE;
		}
		return SPR_PALETTE_ZONING_RED;
	}
	return INVALID_SPRITE_ID;
}

//Check which town zone tile belongs to.
SpriteID TileZoneCheckTownZones(TileIndex tile) {
	HouseZonesBits next_zone = HZB_BEGIN, tz = HZB_END;

	Town *town;
	FOR_ALL_TOWNS(town) {
		// town code uses <= for checking town borders (tz0) but < for other zones
		while (next_zone < HZB_END
			&& (town->cache.squared_town_zone_radius[next_zone] == 0
				|| DistanceSquare(tile, town->xy) <= town->cache.squared_town_zone_radius[next_zone] + (next_zone == HZB_BEGIN ? 0 : 1))
		){
			if(town->cache.squared_town_zone_radius[next_zone] != 0)  tz = next_zone;
			next_zone++;
		}
	}

	switch (tz) {
		case HZB_TOWN_EDGE:         return SPR_PALETTE_ZONING_LIGHT_BLUE; // Tz0
		case HZB_TOWN_OUTSKIRT:     return SPR_PALETTE_ZONING_RED; // Tz1
		case HZB_TOWN_OUTER_SUBURB: return SPR_PALETTE_ZONING_YELLOW; // Tz2
		case HZB_TOWN_INNER_SUBURB: return SPR_PALETTE_ZONING_GREEN; // Tz3
		case HZB_TOWN_CENTRE:       return SPR_PALETTE_ZONING_WHITE; // Tz4 - center
		default:                    return INVALID_SPRITE_ID; // no town
	}
	return INVALID_SPRITE_ID;
}

//Check CB town acceptance area
SpriteID TileZoneCheckNewCBBorders(TileIndex tile) {
	Town *town;
	FOR_ALL_TOWNS(town) {
		if (DistanceSquare(tile, town->xy) <= town->cache.squared_town_zone_radius[0] + 30)
			return SPR_PALETTE_ZONING_BLACK;
	}
	return INVALID_SPRITE_ID;
}

//Check CB town acceptance area
SpriteID TileZoneCheckCBBorders(TileIndex tile) {
	Town *town = CalcClosestTownFromTile(tile);

	if (town != NULL) {
		if (DistanceManhattan(town->xy, tile) <= _settings_client.gui.cb_distance_check) {
			return SPR_PALETTE_ZONING_LIGHT_BLUE; //cb catchment
		}
	}
	return INVALID_SPRITE_ID; // no town
}

//Check whether the tile is within citybuilder server town border (where houses could be built)
SpriteID TileZoneCheckCBTownBorders(TileIndex tile) {
	Town *town;
	FOR_ALL_TOWNS(town) {
		uint32 distMax = DistanceMax(town->xy, tile);
		if (distMax * distMax < town->cache.squared_town_zone_radius[0]){
			return SPR_PALETTE_ZONING_GREEN;
		}
	}
	return INVALID_SPRITE_ID;
}

//Check which advertisement zone(small, medium, large) tile belongs to
SpriteID TileZoneCheckTownAdvertisementZones(TileIndex tile) {
	Town *town = CalcClosestTownFromTile(tile, 21U);
	if (town == NULL) return INVALID_SPRITE_ID; //nothing

	uint dist = DistanceManhattan(town->xy, tile);

	if (dist <= 10) return SPR_PALETTE_ZONING_GREEN;
	if (dist <= 15) return SPR_PALETTE_ZONING_YELLOW;
	if (dist <= 20) return SPR_PALETTE_ZONING_LIGHT_BLUE;
	return INVALID_SPRITE_ID;
}

//Checks for tile in growth tiles info
SpriteID TileZoneCheckTownsGrowthTiles(TileIndex tile) {
	switch (max(_towns_growth_tiles[tile], _towns_growth_tiles_last_month[tile])) {
		case TGTS_CB_HOUSE_REMOVED_NOGROW: return SPR_PALETTE_ZONING_LIGHT_BLUE;
		case TGTS_RH_REMOVED:              return SPR_PALETTE_ZONING_LIGHT_BLUE;
		case TGTS_RH_REBUILT:              return SPR_PALETTE_ZONING_WHITE;
		case TGTS_NEW_HOUSE:               return SPR_PALETTE_ZONING_GREEN;
		case TGTS_CYCLE_SKIPPED:           return SPR_PALETTE_ZONING_ORANGE;
		case TGTS_HOUSE_SKIPPED:           return SPR_PALETTE_ZONING_YELLOW;
		case TGTS_CB_HOUSE_REMOVED:        return SPR_PALETTE_ZONING_RED;
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
		return SPR_PALETTE_ZONING_GREEN;
	}

	return SPR_PALETTE_ZONING_RED;
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
		case CHECKOPINION:     return TileZoneCheckOpinionEvaluation(tile, owner);
		case CHECKBUILD:       return TileZoneCheckBuildEvaluation(tile, owner);
		case CHECKSTACATCH:    return TileZoneCheckStationCatchmentEvaluation(tile);
		case CHECKBULUNSER:    return TileZoneCheckUnservedBuildingsEvaluation(tile);
		case CHECKINDUNSER:    return TileZoneCheckUnservedIndustriesEvaluation(tile);
		case CHECKTOWNZONES:   return TileZoneCheckTownZones(tile);
		case CHECKCBBORDERS:   return TileZoneCheckCBBorders(tile);
		case CHECKNEWCBBORDERS:   return TileZoneCheckNewCBBorders(tile);
		case CHECKCBTOWNBORDERS: return TileZoneCheckCBTownBorders(tile);
		case CHECKTOWNADZONES: return TileZoneCheckTownAdvertisementZones(tile);
		case CHECKTOWNGROWTHTILES: return TileZoneCheckTownsGrowthTiles(tile);
		case CHECKACTIVESTATIONS: return TileZoneCheckActiveStations(tile);
		default:               return INVALID_SPRITE_ID;
	}
}

/**
 * Draw the the zoning on the tile.
 * @param TileInfo ti
 *        the tile to draw on.
 */
void DrawTileZoning(const TileInfo *ti) {
	if(_zoning.outer == CHECKNOTHING && _zoning.inner == CHECKNOTHING) return; //nothing to do
	if (_game_mode != GM_NORMAL || ti->tile >= MapSize() || IsTileType(ti->tile, MP_VOID)) return; //check invalid
	if (_zoning.outer != CHECKNOTHING){
		DrawZoningSprites(SPR_SELECT_TILE, TileZoningSpriteEvaluation(ti->tile, _local_company, _zoning.outer), ti);
	}
	if (_zoning.inner != CHECKNOTHING){
		DrawZoningSprites(SPR_INNER_HIGHLIGHT_BASE, TileZoningSpriteEvaluation(ti->tile, _local_company, _zoning.inner), ti);
	}
}
