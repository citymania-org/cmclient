#ifndef CITYMANIA_HIGHLIGHT_HPP
#define CITYMANIA_HIGHLIGHT_HPP

#include "cm_highlight_type.hpp"

#include "../core/enum_type.hpp"
#include "../gfx_type.h"
#include "../industry_type.h"
#include "../tile_cmd.h"
#include "../tile_type.h"
#include "../tilehighlight_type.h"
#include "../town_type.h"

#include "../table/sprites.h"

enum TileHighlightType {
    THT_NONE,
    THT_WHITE,
    THT_BLUE,
    THT_RED,
};

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
    FULL = TOP_LEFT | TOP_RIGHT | BOTTOM_LEFT | BOTTOM_RIGHT,
};

class TileHighlight {
public:
    SpriteID ground_pal = PAL_NONE;
    SpriteID structure_pal = PAL_NONE;
    SpriteID sprite = 0;
    SpriteID selection = PAL_NONE;
    ZoningBorder border[4] = {};
    SpriteID border_color[4] = {};
    uint border_count = 0;

    void add_border(ZoningBorder border, SpriteID color) {
        if (border == ZoningBorder::NONE || !color) return;
        this->border[this->border_count] = border;
        this->border_color[this->border_count] = color;
        this->border_count++;
    }

    void tint_all(SpriteID color) {
        if (!color) return;
        this->ground_pal = this->structure_pal = color;
    }

    void clear_borders() {
        this->border_count = 0;
    }
};

DECLARE_ENUM_AS_BIT_SET(ZoningBorder);

// enum class AdvertisementZone: uint8 {
//     NONE = 0,
//     LARGE = 1,
//     MEDIUM = 2,
//     SMALL = 3,
// };


TileHighlight GetTileHighlight(const TileInfo *ti);
void DrawTileZoning(const TileInfo *ti, const TileHighlight &th);
bool DrawTileSelection(const TileInfo *ti, const TileHighlightType &tht);

void AllocateZoningMap(uint map_size);
void InitializeZoningMap();

void UpdateTownZoning(Town *town, uint32 prev_edge);
void UpdateZoningTownHouses(const Town *town, uint32 old_houses);
HighLightStyle UpdateTileSelection(HighLightStyle new_drawstyle);

std::pair<ZoningBorder, uint8> GetTownZoneBorder(TileIndex tile);
ZoningBorder GetAnyStationCatchmentBorder(TileIndex tlie);
// std::pair<ZoningBorder, uint8> GetTownAdvertisementBorder(TileIndex tile);
//
SpriteID GetTownTileZoningPalette(TileIndex tile);
SpriteID GetIndustryTileZoningPalette(TileIndex tile, Industry *ind);
void SetIndustryForbiddenTilesHighlight(IndustryType type);


PaletteID GetTreeShadePal(TileIndex tile);

}  // namespace citymania

#endif
