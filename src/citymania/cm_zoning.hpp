#ifndef CM_ZONING_HPP
#define CM_ZONING_HPP

#include "../tile_cmd.h"

namespace citymania {

enum class EvaluationMode : uint8_t {
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
DECLARE_ENUM_AS_ADDABLE(EvaluationMode)

struct Zoning {
	EvaluationMode inner;
	EvaluationMode outer;
};

extern Zoning _zoning;

SpriteID TileZoningSpriteEvaluation(TileIndex tile, Owner owner, EvaluationMode ev_mode);
void DrawTileZoning(const TileInfo *ti);
void ShowZoningToolbar();

} // namespace citymania

#endif /*ZONING_H_*/
