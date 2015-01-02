/** @file zoning.h */

#ifndef ZONING_H_
#define ZONING_H_

#include "tile_cmd.h"

enum EvaluationMode {
	CHECKNOTHING = 0,
	CHECKOPINION = 1,  ///< Check the local authority's opinion.
	CHECKBUILD = 2,    ///< Check wither or not the player can build.
	CHECKSTACATCH = 3, ///< Check catchment area for stations
	CHECKBULUNSER = 4, ///< Check for unserved buildings
	CHECKINDUNSER = 5, ///< Check for unserved industries
	CHECKTOWNZONES = 6,  ///< Town zones (Tz*)
	CHECKCBBORDERS = 7,  ///< Citybuilder cargo acceptment zone
	CHECKCBTOWNBORDERS = 8,  ///< Citybuilder server town borders
	CHECKTOWNADZONES = 9,  ///< Town advertisement zone
	CHECKTOWNGROWTHTILES = 10 ///< Town growth tiles (new house, skipped/removed house)
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
