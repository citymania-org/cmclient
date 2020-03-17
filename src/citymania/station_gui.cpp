#include "../stdafx.h"

#include "station_gui.hpp"

#include "../core/math_func.hpp"
#include "../command_type.h"
#include "../command_func.h"
#include "../company_func.h"
#include "../landscape.h"
#include "../newgrf_station.h"  // StationClassID
#include "../station_base.h"
#include "../tilehighlight_type.h"
#include "../viewport_func.h"
#include "../viewport_kdtree.h"
#include "../window_gui.h"
#include "../zoom_type.h"
#include "../zoom_func.h"

extern const Station *_viewport_highlight_station;
extern TileHighlightData _thd;
extern void MarkCatchmentTilesDirty();

extern DiagDirection _road_station_picker_orientation;
extern bool CheckDriveThroughRoadStopDirection(TileArea area, RoadBits r);
extern DiagDirection AutodetectRoadObjectDirection(TileIndex tile);
extern DiagDirection AutodetectDriveThroughRoadStopDirection(TileArea area);
extern bool CheckClickOnViewportSign(const ViewPort *vp, int x, int y, const ViewportSign *sign);
extern Rect ExpandRectWithViewportSignMargins(Rect r, ZoomLevel zoom);
extern ViewportSignKdtree _viewport_sign_kdtree;
AirportClassID _selected_airport_class;
extern int _selected_airport_index;
byte _selected_airport_layout;

extern RailType _cur_railtype;  // rail_gui.cpp

struct RailStationGUISettings {
    Axis orientation;                 ///< Currently selected rail station orientation

    bool newstations;                 ///< Are custom station definitions available?
    StationClassID station_class;     ///< Currently selected custom station class (if newstations is \c true )
    byte station_type;                ///< %Station type within the currently selected custom station class (if newstations is \c true )
    byte station_count;               ///< Number of custom stations (if newstations is \c true )
};
extern RailStationGUISettings _railstation; //rail_gui.cpp

namespace citymania {

StationBuildingStatus _station_building_status = StationBuildingStatus::NEW;
const Station *_station_to_join = nullptr;
const Station *_highlight_station_to_join = nullptr;
TileArea _highlight_join_area;

void CheckRedrawStationCoverage();

// void SetStationTileSelectSize(int w, int h, int catchment) {
//     _station_select.w = w;
//     _station_select.h = h;
//     _station_select.catchment = catchment;
// }

void SetStationBiildingStatus(StationBuildingStatus status) {
    _station_building_status = status;
};

static const int MAX_TILE_EXTENT_LEFT   = ZOOM_LVL_BASE * TILE_PIXELS;                     ///< Maximum left   extent of tile relative to north corner.
static const int MAX_TILE_EXTENT_RIGHT  = ZOOM_LVL_BASE * TILE_PIXELS;                     ///< Maximum right  extent of tile relative to north corner.
static const int MAX_TILE_EXTENT_TOP    = ZOOM_LVL_BASE * MAX_BUILDING_PIXELS;             ///< Maximum top    extent of tile relative to north corner (not considering bridges).
static const int MAX_TILE_EXTENT_BOTTOM = ZOOM_LVL_BASE * (TILE_PIXELS + 2 * TILE_HEIGHT); ///< Maximum bottom extent of tile relative to north corner (worst case: #SLOPE_STEEP_N).

void MarkTileAreaDirty(const TileArea &ta) {
    if (ta.tile == INVALID_TILE) return;
    auto x = TileX(ta.tile);
    auto y = TileY(ta.tile);
    Point p1 = RemapCoords(x * TILE_SIZE, y * TILE_SIZE, TileHeight(ta.tile) * TILE_HEIGHT);
    Point p2 = RemapCoords((x + ta.w)  * TILE_SIZE, (y + ta.h) * TILE_SIZE, TileHeight(TileXY(x + ta.w - 1, y + ta.h - 1)) * TILE_HEIGHT);
    Point p3 = RemapCoords((x + ta.w)  * TILE_SIZE, y * TILE_SIZE, TileHeight(TileXY(x + ta.w - 1, y)) * TILE_HEIGHT);
    Point p4 = RemapCoords(x * TILE_SIZE, (y + ta.h) * TILE_SIZE, TileHeight(TileXY(x, y + ta.h - 1)) * TILE_HEIGHT);
    MarkAllViewportsDirty(
            p3.x - MAX_TILE_EXTENT_LEFT,
            p4.x - MAX_TILE_EXTENT_TOP,
            p1.y + MAX_TILE_EXTENT_RIGHT,
            p2.y + MAX_TILE_EXTENT_BOTTOM);
}

static void UpdateHiglightJoinArea(const Station *station) {
    if (!station) {
        MarkTileAreaDirty(_highlight_join_area);
        _highlight_join_area.tile = INVALID_TILE;
        return;
    }
    auto &r = _station_to_join->rect;
    auto d = (int)_settings_game.station.station_spread - 1;
    TileArea ta(
        TileXY(max(r.right - d, 0),
               max(r.bottom - d, 0)),
        TileXY(min(r.left + d, MapSizeX() - 1),
               min(r.top + d, MapSizeY() - 1))
    );
    if (_highlight_join_area.tile == ta.tile &&
        _highlight_join_area.w == ta.w &&
        _highlight_join_area.h == ta.h) return;
    MarkTileAreaDirty(_highlight_join_area);
    _highlight_join_area = ta;
    MarkTileAreaDirty(_highlight_join_area);
}

static void MarkCoverageAreaDirty(const Station *station) {
    MarkTileAreaDirty(station->catchment_tiles);
}

void MarkCoverageHighlightDirty() {
    MarkCatchmentTilesDirty();
}

void SetHighlightStationToJoin(const Station *station, bool with_area) {
    UpdateHiglightJoinArea(with_area ? station : nullptr);

    if (_highlight_station_to_join == station) return;

    if (_highlight_station_to_join && _settings_client.gui.station_show_coverage)
        MarkCoverageAreaDirty(_highlight_station_to_join);
    _highlight_station_to_join = station;
    if (_highlight_station_to_join && _settings_client.gui.station_show_coverage)
        MarkCoverageAreaDirty(_highlight_station_to_join);
}

void OnStationTileSetChange(const Station *station, bool adding, StationType type) {
    if (station == _highlight_station_to_join) {
        if (_highlight_join_area.tile != INVALID_TILE)
            UpdateHiglightJoinArea(_station_to_join);
        if (_settings_client.gui.station_show_coverage)
            MarkCoverageAreaDirty(_highlight_station_to_join);
    }
    if (station == _viewport_highlight_station) MarkCoverageAreaDirty(_viewport_highlight_station);
}

CommandContainer _last_station_bulid_cmd;

void OnStationPartBuilt(const Station *station, TileIndex tile, uint32 p1, uint32 p2) {
    if (_current_company != _local_company) return;
    if (tile != _last_station_bulid_cmd.tile ||
        p1 != _last_station_bulid_cmd.p1 ||
        p2 != _last_station_bulid_cmd.p2) return;
    _station_to_join = station;
    CheckRedrawStationCoverage();
}

const Station *CheckClickOnDeadStationSign() {
    int x = _cursor.pos.x;
    int y = _cursor.pos.y;
    Window *w = FindWindowFromPt(x, y);
    if (w == nullptr) return nullptr;
    ViewPort *vp = IsPtInWindowViewport(w, x, y);

    if (!HasBit(_display_opt, DO_SHOW_STATION_NAMES) && !IsInvisibilitySet(TO_SIGNS))
        return nullptr;

    x = ScaleByZoom(x - vp->left, vp->zoom) + vp->virtual_left;
    y = ScaleByZoom(y - vp->top, vp->zoom) + vp->virtual_top;

    Rect search_rect{ x - 1, y - 1, x + 1, y + 1 };
    search_rect = ExpandRectWithViewportSignMargins(search_rect, vp->zoom);

    const Station *last_st = nullptr;
    _viewport_sign_kdtree.FindContained(search_rect.left, search_rect.top, search_rect.right, search_rect.bottom, [&](const ViewportSignKdtreeItem & item) {
        if (item.type != ViewportSignKdtreeItem::VKI_STATION) return;
        auto st = Station::Get(item.id.station);
        if (st->IsInUse()) return;
        if (_local_company != st->owner) return;
        if (CheckClickOnViewportSign(vp, x, y, &st->sign)) last_st = st;
    });
    return last_st;
}

bool CheckStationJoin(TileIndex start_tile, TileIndex end_tile) {
    // if (_ctrl_pressed && start_tile == end_tile) {
    if (_ctrl_pressed) {
        if (IsTileType (start_tile, MP_STATION)) {
            citymania::SelectStationToJoin(Station::GetByTile(start_tile));
            return true;
        }
        auto st = CheckClickOnDeadStationSign();
        if (st) {
            citymania::SelectStationToJoin(st);
            return true;
        }
    }
    return false;
}

void JoinAndBuild(CommandContainer cmdcont) {
    auto join_to = _highlight_station_to_join;
    uint32 adj_bit = ((_ctrl_pressed || join_to) ? 1 : 0);
    auto cmd = (cmdcont.cmd & CMD_ID_MASK);
    if (cmd == CMD_BUILD_RAIL_STATION) {
        SB(cmdcont.p1, 24, 1, adj_bit);
    } else if (cmd == CMD_BUILD_ROAD_STOP) {
        SB(cmdcont.p2, 2, 1, adj_bit);
    } else if (cmd == CMD_BUILD_DOCK) {
        SB(cmdcont.p1, 0, 1, adj_bit);
    } else if (cmd == CMD_BUILD_AIRPORT) {
        SB(cmdcont.p2, 0, 1, adj_bit);
    }
    if (_ctrl_pressed) SB(cmdcont.p2, 16, 16, NEW_STATION);
    else if (join_to) SB(cmdcont.p2, 16, 16, join_to->index);
    else SB(cmdcont.p2, 16, 16, INVALID_STATION);

    _last_station_bulid_cmd = cmdcont;
    DoCommandP(&cmdcont);
}

void PlaceRoadStop(TileIndex start_tile, TileIndex end_tile, uint32 p2, uint32 cmd) {
    uint8 ddir = _road_station_picker_orientation;
    SB(p2, 16, 16, INVALID_STATION); // no station to join
    TileArea ta(start_tile, end_tile);

    if (CheckStationJoin(start_tile, end_tile)) return;

    if (ddir >= DIAGDIR_END) {
        if (ddir < DIAGDIR_END + 2) {
            SetBit(p2, 1); // It's a drive-through stop.
            ddir -= DIAGDIR_END; // Adjust picker result to actual direction.
            // When placed on road autorotate anyway
            if (ddir == DIAGDIR_SE) {
                if (!CheckDriveThroughRoadStopDirection(ta, ROAD_Y))
                    ddir = DIAGDIR_NE;
            } else {
                if (!CheckDriveThroughRoadStopDirection(ta, ROAD_X))
                    ddir = DIAGDIR_SE;
            }
        }
        else if (ddir == DIAGDIR_END + 2) {
            ddir = AutodetectRoadObjectDirection(start_tile);
        }
        else if (ddir == DIAGDIR_END + 3) {
            SetBit(p2, 1); // It's a drive-through stop.
            ddir = AutodetectDriveThroughRoadStopDirection(ta);
        }
    }
    p2 |= ddir << 3; // Set the DiagDirecion into p2 bits 3 and 4.

    CommandContainer cmdcont = { ta.tile, (uint32)(ta.w | ta.h << 8), p2, cmd, CcRoadStop, "" };
    JoinAndBuild(cmdcont);
}

void HandleStationPlacement(TileIndex start, TileIndex end)
{
    if (CheckStationJoin(start, end)) return;

    TileArea ta(start, end);
    uint numtracks = ta.w;
    uint platlength = ta.h;

    if (_railstation.orientation == AXIS_X) Swap(numtracks, platlength);

    uint32 p1 = _cur_railtype | _railstation.orientation << 6 | numtracks << 8 | platlength << 16 | _ctrl_pressed << 24;
    uint32 p2 = _railstation.station_class | _railstation.station_type << 8 | INVALID_STATION << 16;

    CommandContainer cmdcont = { ta.tile, p1, p2, CMD_BUILD_RAIL_STATION | CMD_MSG(STR_ERROR_CAN_T_BUILD_RAILROAD_STATION), CcStation, "" };
    JoinAndBuild(cmdcont);
}

void PlaceDock(TileIndex tile) {
    if (CheckStationJoin(tile, tile)) return;

    uint32 p2 = (uint32)INVALID_STATION << 16; // no station to join

    /* tile is always the land tile, so need to evaluate _thd.pos */
    CommandContainer cmdcont = { tile, _ctrl_pressed, p2, CMD_BUILD_DOCK | CMD_MSG(STR_ERROR_CAN_T_BUILD_DOCK_HERE), CcBuildDocks, "" };

    /* Determine the watery part of the dock. */
    // DiagDirection dir = GetInclinedSlopeDirection(GetTileSlope(tile));
    // TileIndex tile_to = (dir != INVALID_DIAGDIR ? TileAddByDiagDir(tile, ReverseDiagDir(dir)) : tile);

    JoinAndBuild(cmdcont);
}

void PlaceAirport(TileIndex tile) {
    if (CheckStationJoin(tile, tile)) return;

    if (_selected_airport_index == -1) return;

    uint32 p2 = _ctrl_pressed;
    SB(p2, 16, 16, INVALID_STATION); // no station to join

    uint32 p1 = AirportClass::Get(_selected_airport_class)->GetSpec(_selected_airport_index)->GetIndex();
    p1 |= _selected_airport_layout << 8;
    CommandContainer cmdcont = { tile, p1, p2, CMD_BUILD_AIRPORT | CMD_MSG(STR_ERROR_CAN_T_BUILD_AIRPORT_HERE), CcBuildAirport, "" };
    JoinAndBuild(cmdcont);
}

static void FindStationsAroundSelection(const TileArea &location)
{
    /* Extended area by one tile */
    int x = TileX(location.tile);
    int y = TileY(location.tile);

    TileArea ta(TileXY(max<int>(0, x - 1), max<int>(0, y - 1)), TileXY(min<int>(MapMaxX() - 1, x + location.w + 1), min<int>(MapMaxY() - 1, y + location.h + 1)));

    Station *adjacent = nullptr;

    /* Direct loop instead of FindStationsAroundTiles as we are not interested in catchment area */
    TILE_AREA_LOOP(tile, ta) {
        if (IsTileType(tile, MP_STATION) && GetTileOwner(tile) == _local_company) {
            Station *st = Station::GetByTile(tile);
            if (st == nullptr) continue;

            int tx = TileX(tile);
            int ty = TileY(tile);
            bool is_corner = ((tx == x - 1 || tx == x + location.w + 1) && (ty == y - 1 || ty == y + location.h + 1));

            if (adjacent && is_corner) continue;
            adjacent = st;
            if (!is_corner) break;
        }
    }
    SetHighlightStationToJoin(adjacent, false);
    _station_building_status = (adjacent == nullptr ? StationBuildingStatus::NEW : StationBuildingStatus::JOIN);
}

void CheckRedrawStationCoverage() {
    static bool last_ctrl_pressed = false;
    static TileArea last_location;
    static bool last_station_mode = false;
    static bool last_coverage = false;
    TileArea location(TileVirtXY(_thd.pos.x, _thd.pos.y), _thd.size.x / TILE_SIZE - 1, _thd.size.y / TILE_SIZE - 1);
    bool station_mode = ((_thd.drawstyle & HT_DRAG_MASK) == HT_RECT && _thd.outersize.x > 0);
    bool location_changed = (location.tile != last_location.tile ||  location.w != last_location.w || location.h != last_location.h);
    bool mode_changed = (last_station_mode != station_mode);
    // if (!location_changed && _ctrl_pressed == last_ctrl_pressed && !mode_changed)
    //     return;

    // last_ctrl_pressed = _ctrl_pressed;
    // last_location = location;
    // last_station_mode = station_mode;

    if (_ctrl_pressed) {
        Station *st = nullptr;
        if (IsTileType(location.tile, MP_STATION) && GetTileOwner(location.tile) == _local_company)
            st = Station::GetByTile(location.tile);

        SetHighlightStationToJoin(st, _station_to_join && st == _station_to_join);
        _station_building_status = (st == nullptr ? StationBuildingStatus::NEW : StationBuildingStatus::JOIN);
    } else {
        if (_station_to_join) {
            SetHighlightStationToJoin(_station_to_join, true);
            _station_building_status = StationBuildingStatus::JOIN;
        } else {
            FindStationsAroundSelection(location);
        }
    }
}


void SelectStationToJoin(const Station *station) {
    if (_station_to_join == station)
        _station_to_join = nullptr;
    else
        _station_to_join = station;
    CheckRedrawStationCoverage();
}

void AbortStationPlacement() {
    _station_to_join = nullptr;
    SetHighlightStationToJoin(nullptr, false);
}


} // namespace citymania
