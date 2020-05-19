/** @file zoning.h */

#ifndef ZONING_H_
#define ZONING_H_

#include "tile_cmd.h"

enum EvaluationMode {
	CHECKNOTHING = 0,
	CHECKOPINION,  ///< Check the local authority's opinion.
	CHECKBUILD,    ///< Check wither or not the player can build.
	CHECKSTACATCH, ///< Check catchment area for stations
	CHECKACTIVESTATIONS,  ///< Active/inactive stations (serviced in 50 days)
	CHECKBULUNSER, ///< Check for unserved buildings
	CHECKINDUNSER, ///< Check for unserved industries
	CHECKTOWNZONES,  ///< Town zones (Tz*)
	CHECKCBACCEPTANCE,  ///< Citybuilder cargo acceptance zone
	CHECKCBTOWNLIMIT,  ///< Citybuilder cargo acceptment zone
	CHECKTOWNADZONES,  ///< Town advertisement zone
	CHECKTOWNGROWTHTILES, ///< Town growth tiles (new house, skipped/removed house)
};

struct Zoning {
	EvaluationMode inner;
	EvaluationMode outer;
};

extern Zoning _zoning;

SpriteID TileZoningSpriteEvaluation(TileIndex tile, Owner owner, EvaluationMode ev_mode);
void DrawTileZoning(const TileInfo *ti);
void ShowZoningToolbar();

#endif /*ZONING_H_*/
