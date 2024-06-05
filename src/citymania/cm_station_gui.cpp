#include "../stdafx.h"

#include "cm_station_gui.hpp"

#include "cm_hotkeys.hpp"
#include "cm_commands.hpp"

#include "../core/math_func.hpp"
#include "../command_type.h"
#include "../command_func.h"
#include "../company_func.h"
#include "../industry_map.h"
#include "../industry.h"
#include "../landscape.h"
#include "../newgrf_station.h"  // StationClassID
#include "../newgrf_house.h"  // GetHouseCallback
#include "../newgrf_cargo.h"  // GetCargoTranslation
#include "../object_type.h"
#include "../object_map.h"
#include "../station_base.h"
#include "../station_cmd.h"
#include "../strings_func.h"  // GetString, SetDParam
#include "../tilehighlight_type.h"
#include "../town_map.h"
#include "../town.h"
#include "../viewport_func.h"
#include "../viewport_kdtree.h"
#include "../window_gui.h"
#include "../zoom_type.h"
#include "../zoom_func.h"

#include <sstream>

extern const Station *_viewport_highlight_station;
extern TileHighlightData _thd;

extern bool CheckClickOnViewportSign(const Viewport *vp, int x, int y, const ViewportSign *sign);
extern Rect ExpandRectWithViewportSignMargins(Rect r, ZoomLevel zoom);
extern RoadBits FindRailsToConnect(TileIndex tile);
extern ViewportSignKdtree _viewport_sign_kdtree;
extern AirportClassID _selected_airport_class;
extern int _selected_airport_index;
extern byte _selected_airport_layout;
extern RailType _cur_railtype;  // rail_gui.cpp

struct RailStationGUISettings {
    Axis orientation;                 ///< Currently selected rail station orientation

    bool newstations;                 ///< Are custom station definitions available?
    StationClassID station_class;     ///< Currently selected custom station class (if newstations is \c true )
    byte station_type;                ///< %Station type within the currently selected custom station class (if newstations is \c true )
    byte station_count;               ///< Number of custom stations (if newstations is \c true )
};
extern RailStationGUISettings _railstation; //rail_gui.cpp

struct RoadStopGUISettings {
    DiagDirection orientation;

    RoadStopClassID roadstop_class;
    uint16_t roadstop_type;
    uint16_t roadstop_count;
};
extern RoadStopGUISettings _roadstop_gui_settings;

namespace citymania {

StationBuildingStatus _station_building_status = StationBuildingStatus::NEW;
const Station *_station_to_join = nullptr;
const Station *_highlight_station_to_join = nullptr;
TileArea _highlight_join_area;

// void SetStationTileSelectSize(int w, int h, int catchment) {
//     _station_select.w = w;
//     _station_select.h = h;
//     _station_select.catchment = catchment;
// }

bool UseImprovedStationJoin() {
    return _settings_client.gui.cm_use_improved_station_join && _settings_game.station.distant_join_stations;
}

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
        TileXY(std::max<int>(r.right - d, 0),
               std::max<int>(r.bottom - d, 0)),
        TileXY(std::min<int>(r.left + d, Map::SizeX() - 1),
               std::min<int>(r.top + d, Map::SizeY() - 1))
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

void OnStationTileSetChange(const Station *station, bool /* adding */, StationType /* type */) {
    if (station == _highlight_station_to_join) {
        if (_highlight_join_area.tile != INVALID_TILE)
            UpdateHiglightJoinArea(_station_to_join);
        if (_settings_client.gui.station_show_coverage)
            MarkCoverageAreaDirty(_highlight_station_to_join);
    }
    if (station == _viewport_highlight_station) MarkCoverageAreaDirty(_viewport_highlight_station);
}

void OnStationDeleted(const Station *station) {
    if (_highlight_station_to_join == station) {
        MarkCoverageAreaDirty(station);
        _highlight_station_to_join = nullptr;
    }
}

const Station *_last_built_station;
void OnStationPartBuilt(const Station *station) {
    _last_built_station = station;
    CheckRedrawStationCoverage();
}

void OnStationRemoved(const Station *station) {
    if (_last_built_station == station) _last_built_station = nullptr;
    if (_station_to_join == station) {
        // TODO MarkJoinHighlight
        MarkCoverageAreaDirty(station);
        _station_to_join = nullptr;
    }
}


const Station *CheckClickOnDeadStationSign() {
    int x = _cursor.pos.x;
    int y = _cursor.pos.y;
    Window *w = FindWindowFromPt(x, y);
    if (w == nullptr) return nullptr;
    Viewport *vp = IsPtInWindowViewport(w, x, y);

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

bool CheckStationJoin(TileIndex start_tile, TileIndex /* end_tile */) {
    if (citymania::_fn_mod) {
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

template <typename Tcommand, typename Tcallback>
void JoinAndBuild(Tcommand command, Tcallback *callback) {
    auto join_to = _highlight_station_to_join;
    command.adjacent = (citymania::_fn_mod || join_to);
    command.station_to_join = INVALID_STATION;

    if (citymania::_fn_mod) command.station_to_join = NEW_STATION;
    else if (join_to) command.station_to_join = join_to->index;

    command.with_callback([] (bool res)->bool {
        if (!res) return false;
        _station_to_join = _last_built_station;
        return true;
    }).post(callback);
}

static DiagDirection TileFractCoordsToDiagDir(Point pt) {
    auto x = pt.x & TILE_UNIT_MASK;
    auto y = pt.y & TILE_UNIT_MASK;
    bool diag = (x + y) < 16;
    if (x < y) {
        return diag ? DIAGDIR_NE : DIAGDIR_SE;
    }
    return diag ? DIAGDIR_NW : DIAGDIR_SW;
}

static DiagDirection RoadBitsToDiagDir(RoadBits bits) {
    if (bits < ROAD_SE) {
        return bits == ROAD_NW ? DIAGDIR_NW : DIAGDIR_SW;
    }
    return bits == ROAD_SE ? DIAGDIR_SE : DIAGDIR_NE;
}

DiagDirection AutodetectRailObjectDirection(TileIndex tile, Point pt) {
    RoadBits bits = FindRailsToConnect(tile);
    // FIXME after this point repeats road autodetection
    if (HasExactlyOneBit(bits)) return RoadBitsToDiagDir(bits);
    if (bits == ROAD_NONE) bits = ROAD_ALL;
    RoadBits frac_bits = DiagDirToRoadBits(TileFractCoordsToDiagDir(pt));
    if (HasExactlyOneBit(frac_bits & bits)) {
        return RoadBitsToDiagDir(frac_bits & bits);
    }
    frac_bits |= MirrorRoadBits(frac_bits);
    if (HasExactlyOneBit(frac_bits & bits)) {
        return RoadBitsToDiagDir(frac_bits & bits);
    }
    for (DiagDirection ddir = DIAGDIR_BEGIN; ddir < DIAGDIR_END; ddir++) {
        if (DiagDirToRoadBits(ddir) & bits) {
            return ddir;
        }
    }
    NOT_REACHED();
}

static RoadBits FindRoadsToConnect(TileIndex tile, RoadType roadtype) {
    RoadBits bits = ROAD_NONE;
    DiagDirection ddir;
    auto cur_rtt = GetRoadTramType(roadtype);
    // Prioritize roadbits that head in this direction
    for (ddir = DIAGDIR_BEGIN; ddir < DIAGDIR_END; ddir++) {
        TileIndex cur_tile = TileAddByDiagDir(tile, ddir);
        if (GetAnyRoadBits(cur_tile, cur_rtt, true) &
            DiagDirToRoadBits(ReverseDiagDir(ddir)))
        {
            bits |= DiagDirToRoadBits(ddir);
        }
    }
    if (bits != ROAD_NONE) {
        return bits;
    }
    // Try to connect to any road passing by
    for (ddir = DIAGDIR_BEGIN; ddir < DIAGDIR_END; ddir++) {
        TileIndex cur_tile = TileAddByDiagDir(tile,  ddir);
        if (GetTileType(cur_tile) == MP_ROAD && HasTileRoadType(cur_tile, cur_rtt) &&
                (GetRoadTileType(cur_tile) == ROAD_TILE_NORMAL)) {
            bits |= DiagDirToRoadBits(ddir);
        }
    }
    return bits;
}

bool CheckDriveThroughRoadStopDirection(TileArea area, RoadBits r) {
    for (TileIndex tile : area) {
        if (GetTileType(tile) != MP_ROAD) continue;
        if (GetRoadTileType(tile) != ROAD_TILE_NORMAL) continue;
        if (GetAllRoadBits(tile) & ~r) return false;
    }
    return true;
}

/*
 * Selects orientation for road object (depot, terminal station)
 */
DiagDirection AutodetectRoadObjectDirection(TileIndex tile, Point pt, RoadType roadtype) {
    RoadBits bits = FindRoadsToConnect(tile, roadtype);
    if (HasExactlyOneBit(bits)) {
        return RoadBitsToDiagDir(bits);
    }
    if (bits == ROAD_NONE){
        bits = ROAD_ALL;
    }
    RoadBits frac_bits = DiagDirToRoadBits(TileFractCoordsToDiagDir(pt));
    if (HasExactlyOneBit(frac_bits & bits)) {
        return RoadBitsToDiagDir(frac_bits & bits);
    }
    frac_bits |= MirrorRoadBits(frac_bits);
    if (HasExactlyOneBit(frac_bits & bits)) {
        return RoadBitsToDiagDir(frac_bits & bits);
    }
    for (DiagDirection ddir = DIAGDIR_BEGIN; ddir < DIAGDIR_END; ddir++) {
        if (DiagDirToRoadBits(ddir) & bits) {
            return ddir;
        }
    }
    NOT_REACHED();
}

/*
 * Automaticaly selects direction to use for road stop.
 * @param area road stop area
 * @return selected direction
 */
DiagDirection AutodetectDriveThroughRoadStopDirection(TileArea area, Point pt, RoadType roadtype) {
    bool se_suits, ne_suits;

    // Check which direction is available
    // If both are not use SE, building will fail anyway
    se_suits = CheckDriveThroughRoadStopDirection(area, ROAD_Y);
    ne_suits = CheckDriveThroughRoadStopDirection(area, ROAD_X);
    if (!ne_suits) return STATIONDIR_Y;
    if (!se_suits) return STATIONDIR_X;

    // Build station along the longer direction
    if (area.w > area.h) return STATIONDIR_X;
    if (area.w < area.h) return STATIONDIR_Y;

    return DiagDirToAxis(AutodetectRoadObjectDirection(area.tile, pt, roadtype)) == AXIS_X ? STATIONDIR_X : STATIONDIR_Y;
}

void PlaceRoadStop(TileIndex start_tile, TileIndex end_tile, RoadStopType stop_type, bool adjacent, RoadType rt, StringID err_msg) {
    assert(_thd.cm.type == citymania::ObjectHighlight::Type::ROAD_STOP);
    DiagDirection ddir = _thd.cm.ddir;

    TileArea ta(start_tile, end_tile);

    if (CheckStationJoin(start_tile, end_tile)) return;

    bool drive_through = (ddir >= DIAGDIR_END);
    if (drive_through) ddir = static_cast<DiagDirection>(ddir - DIAGDIR_END); // Adjust picker result to actual direction.
    RoadStopClassID spec_class = _roadstop_gui_settings.roadstop_class;
    uint16_t spec_index = _roadstop_gui_settings.roadstop_type;

    auto c = cmd::BuildRoadStop(
        ta.tile,
        ta.w,
        ta.h,
        stop_type,
        drive_through,
        static_cast<DiagDirection>(ddir),
        rt,
        spec_class,
        spec_index,
        INVALID_STATION,
        adjacent
    );
    c.with_error(err_msg);
    JoinAndBuild(c, CcRoadStop);
}

void HandleStationPlacement(TileIndex start, TileIndex end)
{
    if (CheckStationJoin(start, end)) return;

    TileArea ta(start, end);
    uint numtracks = ta.w;
    uint platlength = ta.h;

    if (_railstation.orientation == AXIS_X) Swap(numtracks, platlength);

    auto c = cmd::BuildRailStation(
        ta.tile,
        _cur_railtype,
        _railstation.orientation,
        numtracks,
        platlength,
        _railstation.station_class,
        _railstation.station_type,
        INVALID_STATION,
        false
    );
    c.with_error(STR_ERROR_CAN_T_BUILD_RAILROAD_STATION);
    JoinAndBuild(c, CcStation);
}

void PlaceRail_Station(TileIndex tile) {
    if (CheckStationJoin(tile, tile)) return;
    auto c = cmd::BuildRailStation(
        tile,
        _cur_railtype,
        _railstation.orientation,
        _settings_client.gui.station_numtracks,
        _settings_client.gui.station_platlength,
        _railstation.station_class,
        _railstation.station_type,
        INVALID_STATION,
        false
    );
    c.with_error(STR_ERROR_CAN_T_BUILD_RAILROAD_STATION);
    JoinAndBuild(c, CcStation);
}

void PlaceDock(TileIndex tile, TileIndex tile_to) {
    if (CheckStationJoin(tile, tile_to)) return;

    auto c = cmd::BuildDock(
        tile,
        INVALID_STATION,
        false
    );
    c.with_error(STR_ERROR_CAN_T_BUILD_DOCK_HERE);
    JoinAndBuild(c, CcBuildDocks);
}

void PlaceAirport(TileIndex tile) {
    if (CheckStationJoin(tile, tile)) return;

    if (_selected_airport_index == -1) return;

    byte airport_type = AirportClass::Get(_selected_airport_class)->GetSpec(_selected_airport_index)->GetIndex();
    byte layout = _selected_airport_layout;

    auto c = cmd::BuildAirport(
        tile,
        airport_type,
        layout,
        INVALID_STATION,
        false
    );
    c.with_error(STR_ERROR_CAN_T_BUILD_AIRPORT_HERE);
    JoinAndBuild(c, CcBuildAirport);
}

static void FindStationsAroundSelection(const TileArea &location)
{
    /* Extended area by one tile */
    int x = TileX(location.tile);
    int y = TileY(location.tile);

    TileArea ta(TileXY(std::max<int>(0, x - 1), std::max<int>(0, y - 1)), TileXY(std::min<int>(Map::MaxX() - 1, x + location.w + 1), std::min<int>(Map::MaxY() - 1, y + location.h + 1)));

    Station *adjacent = nullptr;

    /* Direct loop instead of FindStationsAroundTiles as we are not interested in catchment area */
    for (auto tile : ta) {
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

bool CheckRedrawStationCoverage() {
    // static bool last_ctrl_pressed = false;
    static TileArea last_location;
    static bool last_station_mode = false;
    static bool last_fn_mod = false;
    TileArea location(TileVirtXY(_thd.pos.x, _thd.pos.y), _thd.size.x / TILE_SIZE - 1, _thd.size.y / TILE_SIZE - 1);
    bool station_mode = ((_thd.drawstyle & HT_DRAG_MASK) == HT_RECT && _thd.outersize.x > 0);
    bool location_changed = (location.tile != last_location.tile ||  location.w != last_location.w || location.h != last_location.h);
    bool mode_changed = (last_station_mode != station_mode);
    if (!location_changed && citymania::_fn_mod == last_fn_mod && !mode_changed)
        return false;

    last_fn_mod = citymania::_fn_mod;
    last_location = location;
    last_station_mode = station_mode;

    if (citymania::_fn_mod) {
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
    return true;
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


uint GetMonthlyFrom256Tick(uint amount) {
    return ((amount * Ticks::DAY_TICKS * EconomyTime::DAYS_IN_ECONOMY_MONTH) >> 8);
}

uint GetAverageHouseProduction(byte amount) {
    static const uint AVG[2][256] = {
        {0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14, 16, 18, 20, 22, 24, 27, 30, 33, 36, 39, 42, 45, 48, 52, 56, 60, 64, 68, 72, 76, 80, 85, 90, 95, 100, 105, 110, 115, 120, 126, 132, 138, 144, 150, 156, 162, 168, 175, 182, 189, 196, 203, 210, 217, 224, 232, 240, 248, 256, 264, 272, 280, 288, 297, 306, 315, 324, 333, 342, 351, 360, 370, 380, 390, 400, 410, 420, 430, 440, 451, 462, 473, 484, 495, 506, 517, 528, 540, 552, 564, 576, 588, 600, 612, 624, 637, 650, 663, 676, 689, 702, 715, 728, 742, 756, 770, 784, 798, 812, 826, 840, 855, 870, 885, 900, 915, 930, 945, 960, 976, 992, 1008, 1024, 1040, 1056, 1072, 1088, 1105, 1122, 1139, 1156, 1173, 1190, 1207, 1224, 1242, 1260, 1278, 1296, 1314, 1332, 1350, 1368, 1387, 1406, 1425, 1444, 1463, 1482, 1501, 1520, 1540, 1560, 1580, 1600, 1620, 1640, 1660, 1680, 1701, 1722, 1743, 1764, 1785, 1806, 1827, 1848, 1870, 1892, 1914, 1936, 1958, 1980, 2002, 2024, 2047, 2070, 2093, 2116, 2139, 2162, 2185, 2208, 2232, 2256, 2280, 2304, 2328, 2352, 2376, 2400, 2425, 2450, 2475, 2500, 2525, 2550, 2575, 2600, 2626, 2652, 2678, 2704, 2730, 2756, 2782, 2808, 2835, 2862, 2889, 2916, 2943, 2970, 2997, 3024, 3052, 3080, 3108, 3136, 3164, 3192, 3220, 3248, 3277, 3306, 3335, 3364, 3393, 3422, 3451, 3480, 3510, 3540, 3570, 3600, 3630, 3660, 3690, 3720, 3751, 3782, 3813, 3844, 3875, 3906, 3937, 3968, 4000, 4032, 4064, 4096, 4128, 4160, 4192},
        {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 51, 54, 57, 60, 63, 66, 69, 72, 75, 78, 81, 84, 87, 90, 93, 96, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144, 148, 152, 156, 160, 165, 170, 175, 180, 185, 190, 195, 200, 205, 210, 215, 220, 225, 230, 235, 240, 246, 252, 258, 264, 270, 276, 282, 288, 294, 300, 306, 312, 318, 324, 330, 336, 343, 350, 357, 364, 371, 378, 385, 392, 399, 406, 413, 420, 427, 434, 441, 448, 456, 464, 472, 480, 488, 496, 504, 512, 520, 528, 536, 544, 552, 560, 568, 576, 585, 594, 603, 612, 621, 630, 639, 648, 657, 666, 675, 684, 693, 702, 711, 720, 730, 740, 750, 760, 770, 780, 790, 800, 810, 820, 830, 840, 850, 860, 870, 880, 891, 902, 913, 924, 935, 946, 957, 968, 979, 990, 1001, 1012, 1023, 1034, 1045, 1056, 1068, 1080, 1092, 1104, 1116, 1128, 1140, 1152, 1164, 1176, 1188, 1200, 1212, 1224, 1236, 1248, 1261, 1274, 1287, 1300, 1313, 1326, 1339, 1352, 1365, 1378, 1391, 1404, 1417, 1430, 1443, 1456, 1470, 1484, 1498, 1512, 1526, 1540, 1554, 1568, 1582, 1596, 1610, 1624, 1638, 1652, 1666, 1680, 1695, 1710, 1725, 1740, 1755, 1770, 1785, 1800, 1815, 1830, 1845, 1860, 1875, 1890, 1905, 1920, 1936, 1952, 1968, 1984, 2000, 2016, 2032, 2048, 2064, 2080, 2096, 2112, 2128, 2144, 2160}
    };
    if (amount == 0) return 0;
    switch (_settings_game.economy.town_cargogen_mode) {
        case TCGM_ORIGINAL:
            return GetMonthlyFrom256Tick(AVG[EconomyIsInRecession() ? 1 : 0][amount]);
        case TCGM_BITCOUNT: {
            uint amt = (amount + 7) / 8;
            if (EconomyIsInRecession()) amt += 2;
            else amt *= 2;
            return GetMonthlyFrom256Tick(amt * 16);
        }
        default:
            NOT_REACHED();
    }
    return 0;
}

static void AddProducedCargo_Town(TileIndex tile, CargoArray &produced)
{
    if (!IsHouseCompleted(tile)) return;

    HouseID house_id = GetHouseType(tile);
    const HouseSpec *hs = HouseSpec::Get(house_id);
    Town *t = Town::GetByTile(tile);

    if (HasBit(hs->callback_mask, CBM_HOUSE_PRODUCE_CARGO)) {
        for (uint i = 0; i < 256; i++) {
            uint16 callback = GetHouseCallback(CBID_HOUSE_PRODUCE_CARGO, i, 0, house_id, t, tile);

            if (callback == CALLBACK_FAILED || callback == CALLBACK_HOUSEPRODCARGO_END) break;

            CargoID cargo = GetCargoTranslation(GB(callback, 8, 7), hs->grf_prop.grffile);

            if (cargo == CT_INVALID) continue;
            produced[cargo] += GetMonthlyFrom256Tick((uint)GB(callback, 0, 8)) ;
        }
    } else {
        if (hs->population > 0) {
            auto avg = GetAverageHouseProduction(hs->population);
            for (const CargoSpec *cs : CargoSpec::town_production_cargoes[TPE_PASSENGERS]) {
                produced[cs->Index()] += avg;
            }
        }
        if (hs->mail_generation > 0) {
            auto avg = GetAverageHouseProduction(hs->mail_generation);
            for (const CargoSpec *cs : CargoSpec::town_production_cargoes[TPE_MAIL]) {
                produced[cs->Index()] += avg;
            }
        }
    }
}

// Similar to ::GetProductionAroundTiles but counts production total
CargoArray GetProductionAroundTiles(TileIndex tile, int w, int h, int rad)
{
    static const uint HQ_AVG_POP[2][5] = {
        {48, 64, 84, 128, 384},
        {48, 64, 84, 128, 256},
    };
    static const uint HQ_AVG_MAIL[2][5] = {
        {36, 48, 64, 96, 264},
        {36, 48, 64, 96, 196}
    };

    CargoArray produced{};
    std::set<IndustryID> industries;
    TileArea ta = TileArea(tile, w, h).Expand(rad);

    /* Loop over all tiles to get the produced cargo of
     * everything except industries */
    for(auto tile : ta) {
        switch (GetTileType(tile)) {
            case MP_INDUSTRY:
                industries.insert(GetIndustryIndex(tile));
                break;
            case MP_HOUSE:
                AddProducedCargo_Town(tile, produced);
                break;
            case MP_OBJECT:
                if (IsObjectType(tile, OBJECT_HQ)) {
                    auto pax_avg = GetMonthlyFrom256Tick(HQ_AVG_POP[EconomyIsInRecession() ? 1 : 0][GetAnimationFrame(tile)]);
                    auto mail_avg = GetMonthlyFrom256Tick(HQ_AVG_MAIL[EconomyIsInRecession() ? 1 : 0][GetAnimationFrame(tile)]);
                    for (const CargoSpec *cs : CargoSpec::town_production_cargoes[TPE_PASSENGERS])
                        produced[cs->Index()] += pax_avg;
                    for (const CargoSpec *cs : CargoSpec::town_production_cargoes[TPE_MAIL])
                        produced[cs->Index()] += mail_avg;
                }
            default: break;
        }
    }

    /* Loop over the seen industries. They produce cargo for
     * anything that is within 'rad' of any one of their tiles.
     */
    for (IndustryID industry : industries) {
        const Industry *i = Industry::Get(industry);
        /* Skip industry with neutral station */
        if (i->neutral_station != nullptr && !_settings_game.station.serve_neutral_industries) continue;

        for (const auto &p : i->produced) {
            if (IsValidCargoID(p.cargo)) produced[p.cargo] += ((uint)p.history[LAST_MONTH].production) << 8;
        }
    }

    return produced;
}

std::string GetStationCoverageProductionText(TileIndex tile, int w, int h, int rad, StationCoverageType sct) {
    auto production = citymania::GetProductionAroundTiles(tile, w, h, rad);

    std::ostringstream s;
    s << GetString(CM_STR_STATION_BUILD_SUPPLIES);
    bool first = true;
    for (CargoID i = 0; i < NUM_CARGO; i++) {
        if (production[i] == 0) continue;
        switch (sct) {
            case SCT_PASSENGERS_ONLY: if (!IsCargoInClass(i, CC_PASSENGERS)) continue; break;
            case SCT_NON_PASSENGERS_ONLY: if (IsCargoInClass(i, CC_PASSENGERS)) continue; break;
            case SCT_ALL: break;
            default: NOT_REACHED();
        }
        if (!first) s << ", ";
        first = false;
        SetDParam(0, i);
        SetDParam(1, production[i] >> 8);
        s << GetString(STR_JUST_CARGO);
    }
    return s.str();
}

/* enable/disable catchment area with ctrl+click on a station */
void ShowCatchmentByClick(StationID station)
{
	if (_viewport_highlight_station != nullptr) {
		if (_viewport_highlight_station == Station::Get(station))
			SetViewportCatchmentStation(Station::Get(station), false);
		else SetViewportCatchmentStation(Station::Get(station), true);
	}
	else SetViewportCatchmentStation(Station::Get(station), true);
}

} // namespace citymania
