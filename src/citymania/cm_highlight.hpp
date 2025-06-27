#ifndef CITYMANIA_HIGHLIGHT_HPP
#define CITYMANIA_HIGHLIGHT_HPP

#include "cm_commands.hpp"
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

// enum class AdvertisementZone: uint8 {
//     NONE = 0,
//     LARGE = 1,
//     MEDIUM = 2,
//     SMALL = 3,
// };

TileHighlight GetTileHighlight(const TileInfo *ti, TileType tile_type);
void DrawTileZoning(const TileInfo *ti, const TileHighlight &th, TileType tile_type);
bool DrawTileSelection(const TileInfo *ti, const TileHighlightType &tht);
void DrawSelectionOverlay(DrawPixelInfo *dpi);

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
void UpdateIndustryHighlight();
void SetIndustryForbiddenTilesHighlight(IndustryType type);


PaletteID GetTreeShadePal(TileIndex tile);

void RotateAutodetection();
void ResetRotateAutodetection();

void ResetActiveTool();
void SetActiveTool(up<Tool> &&tool);
void UpdateActiveTool();
const up<Tool> &GetActiveTool();


bool HandlePlacePushButton(Window *w, WidgetID widget, up<Tool> tool);
bool HandleMouseMove();
bool HandleMouseClick(Viewport *vp, bool double_click);

}  // namespace citymania

#endif
