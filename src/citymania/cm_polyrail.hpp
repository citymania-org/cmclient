#ifndef CM_POLYRAIL_HPP
#define CM_POLYRAIL_HPP

#include "../tile_type.h"
#include "../tile_cmd.h"
#include "../core/geometry_type.hpp"
#include "../core/enum_type.hpp"
#include <vector>

// Moved from tilehighlight_type.h
/** Highlighting draw styles */
enum HighLightStyle {
    HT_NONE      = 0x000, ///< default
    HT_RECT      = 0x010, ///< rectangle (stations, depots, ...)
    HT_POINT     = 0x020, ///< point (lower land, raise land, level land, ...)
    HT_SPECIAL   = 0x030, ///< special mode used for highlighting while dragging (and for tunnels/docks)
    HT_DRAG      = 0x040, ///< dragging items in the depot windows
    HT_LINE      = 0x008, ///< used for autorail highlighting (longer stretches), lower bits: direction
    HT_RAIL      = 0x080, ///< autorail (one piece), lower bits: direction
    CM_HT_RAIL   = 0x090, ///< CityMania rail mode
    HT_VEHICLE   = 0x100, ///< vehicle is accepted as target as well (bitmask)
    HT_DIAGONAL  = 0x200, ///< Also allow 'diagonal rectangles'. Only usable in combination with #HT_RECT or #HT_POINT.
    HT_POLY      = 0x400, ///< polyline mode; connect highlighted track with previous one
    HT_DRAG_MASK = 0x0F8, ///< Mask for the tile drag-type modes.

    /* lower bits (used with HT_LINE and HT_RAIL):
     * (see ASCII art in table/autorail.h for a visual interpretation) */
    HT_DIR_X  = 0,     ///< X direction
    HT_DIR_Y  = 1,     ///< Y direction
    HT_DIR_HU = 2,     ///< horizontal upper
    HT_DIR_HL = 3,     ///< horizontal lower
    HT_DIR_VL = 4,     ///< vertical left
    HT_DIR_VR = 5,     ///< vertical right
    HT_DIR_END,        ///< end marker
    HT_DIR_MASK = 0x7, ///< masks the drag-direction
};
DECLARE_ENUM_AS_BIT_SET(HighLightStyle)

namespace citymania {

class PolyrailPoint {
public:
    TileIndex tile;
    DiagDirection side;
    PolyrailPoint(TileIndex tile, DiagDirection side): tile{tile}, side{side} {}
    PolyrailPoint(): PolyrailPoint{INVALID_TILE, INVALID_DIAGDIR} {}
    bool operator==(const PolyrailPoint &p) const { return this->tile == p.tile && this->side == p.side; }
    PolyrailPoint Reverse();
    PolyrailPoint Normalize();
    bool IsValid() { return tile != INVALID_TILE; }
};

class Polyrail {
public:
    PolyrailPoint start;
    PolyrailPoint end;
    std::vector<std::pair<TileIndex, Trackdir>> tiles;

    Polyrail() {}
    Polyrail(PolyrailPoint start, PolyrailPoint end): start{start}, end{end} {}
    bool IsValid() { return this->start.IsValid() && this->end.IsValid(); }

    HighLightStyle GetTileHighlightStyle(TileIndex tile);
};

Point SnapToTileEdge(Point p);
void SetPolyrailStart(TileIndex tile);

void SetPolyrailSelectionTilesDirty();
void UpdatePolyrailDrawstyle(Point pt);
void DrawPolyrailTileSelection(const TileInfo *ti);
void PlaceRail_Polyrail(Point pt, bool remove_mode);
HighLightStyle UpdatePolyrailTileSelection();
void SetPolyrailToPlace();

} // namespace citymania

#endif  /* CITYMANIA_POLYRAIL_HPP */
