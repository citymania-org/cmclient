#ifndef CITYMANIA_ZONING_HPP
#define CITYMANIA_ZONING_HPP

#include "../core/enum_type.hpp"
#include "../gfx_type.h"
#include "../tile_type.h"
#include "../town_type.h"

namespace citymania {

////enum class ZoningBorder : unt8 {
enum ZoningBorder: uint8 {
    NONE = 0,
    TOP_LEFT = 1,
    TOP_RIGHT = 2,
    BOTTOM_RIGHT = 4,
    BOTTOM_LEFT = 8,
    TOP_CORNER = 16,
    RIGHT_CORNER = 32,
    BOTTOM_CORNER = 64,
    LEFT_CORNER = 128,
};

DECLARE_ENUM_AS_BIT_SET(ZoningBorder);

// enum class AdvertisementZone: uint8 {
//     NONE = 0,
//     LARGE = 1,
//     MEDIUM = 2,
//     SMALL = 3,
// };


void AllocateZoningMap(uint map_size);
void InitializeZoningMap();

void UpdateTownZoning(Town *town, uint32 prev_edge);


std::pair<ZoningBorder, uint8> GetTownZoneBorder(TileIndex tile);
ZoningBorder GetAnyStationCatchmentBorder(TileIndex tlie);
// std::pair<ZoningBorder, uint8> GetTownAdvertisementBorder(TileIndex tile);
//
SpriteID GetTownTileZoningPalette(TileIndex tile);

}  // namespace citymania

#endif
