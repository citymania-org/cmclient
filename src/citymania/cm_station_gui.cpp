#include "../stdafx.h"

#include "cm_station_gui.hpp"

#include "cm_highlight.hpp"
#include "cm_highlight_type.hpp"
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
#include "cm_type.hpp"
#include "generated/cm_gen_commands.hpp"

#include <sstream>
#include <unordered_set>

bool _remove_button_clicked;  // replace vanilla static vars

extern const Station *_viewport_highlight_station;
extern TileHighlightData _thd;

extern bool CheckClickOnViewportSign(const Viewport &vp, int x, int y, const ViewportSign *sign);
extern Rect ExpandRectWithViewportSignMargins(Rect r, ZoomLevel zoom);
extern RoadBits FindRailsToConnect(TileIndex tile);
extern ViewportSignKdtree _viewport_sign_kdtree;
extern AirportClassID _selected_airport_class;
extern int _selected_airport_index;
extern uint8_t _selected_airport_layout;
extern RailType _cur_railtype;  // rail_gui.cpp
extern RoadType _cur_roadtype;  // road_gui.cpp
extern void GetStationLayout(uint8_t *layout, uint numtracks, uint plat_len, const StationSpec *statspec);

struct StationPickerSelection {
    StationClassID sel_class; ///< Selected station class.
    uint16_t sel_type; ///< Selected station type within the class.
    Axis axis; ///< Selected orientation of the station.
};
extern StationPickerSelection _station_gui; ///< Settings of the station picker.

struct RoadStopPickerSelection {
    RoadStopClassID sel_class; ///< Selected road stop class.
    uint16_t sel_type; ///< Selected road stop type within the class.
    DiagDirection orientation; ///< Selected orientation of the road stop.
};
extern RoadStopPickerSelection _roadstop_gui;

namespace citymania {

StationBuildingStatus _station_building_status = StationBuildingStatus::NEW;
const Station *_highlight_station_to_join = nullptr;
TileArea _highlight_join_area;

bool UseImprovedStationJoin() {
    return _settings_client.gui.cm_use_improved_station_join && _settings_game.station.distant_join_stations;
}

void SetStationBiildingStatus(StationBuildingStatus status) {
    _station_building_status = status;
};

static const int MAX_TILE_EXTENT_LEFT   = ZOOM_BASE * TILE_PIXELS;                     ///< Maximum left   extent of tile relative to north corner.
static const int MAX_TILE_EXTENT_RIGHT  = ZOOM_BASE * TILE_PIXELS;                     ///< Maximum right  extent of tile relative to north corner.
static const int MAX_TILE_EXTENT_TOP    = ZOOM_BASE * MAX_BUILDING_PIXELS;             ///< Maximum top    extent of tile relative to north corner (not considering bridges).
static const int MAX_TILE_EXTENT_BOTTOM = ZOOM_BASE * (TILE_PIXELS + 2 * TILE_HEIGHT); ///< Maximum bottom extent of tile relative to north corner (worst case: #SLOPE_STEEP_N).

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
    // auto &r = _station_to_join->rect;
    // auto d = (int)_settings_game.station.station_spread - 1;
    // TileArea ta(
    //     TileXY(std::max<int>(r.right - d, 0),
    //            std::max<int>(r.bottom - d, 0)),
    //     TileXY(std::min<int>(r.left + d, Map::SizeX() - 1),
    //            std::min<int>(r.top + d, Map::SizeY() - 1))
    // );
    // if (_highlight_join_area.tile == ta.tile &&
    //     _highlight_join_area.w == ta.w &&
    //     _highlight_join_area.h == ta.h) return;
    MarkTileAreaDirty(_highlight_join_area);
    // _highlight_join_area = ta;
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
        // if (_highlight_join_area.tile != INVALID_TILE)
            // UpdateHiglightJoinArea(_station_to_join);
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
        auto st = Station::Get(std::get<StationID>(item.id));
        if (st->IsInUse()) return;
        if (_local_company != st->owner) return;
        if (CheckClickOnViewportSign(*vp, x, y, &st->sign)) last_st = st;
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
    command.station_to_join = StationID::Invalid();

    if (citymania::_fn_mod) command.station_to_join = NEW_STATION;
    else if (join_to) command.station_to_join = join_to->index;

    command.with_callback([] (bool res)->bool {
        if (!res) return false;
        // _station_to_join = _last_built_station;
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
    if (bits == ROAD_NONE) {
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
    RoadStopClassID spec_class = _roadstop_gui.sel_class;
    uint16_t spec_index = _roadstop_gui.sel_type;

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
        StationID::Invalid(),
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

    if (_station_gui.axis == AXIS_X) std::swap(numtracks, platlength);

    auto c = cmd::BuildRailStation(
        ta.tile,
        _cur_railtype,
        _station_gui.axis,
        numtracks,
        platlength,
        _station_gui.sel_class,
        _station_gui.sel_type,
        StationID::Invalid(),
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
        _station_gui.axis,
        _settings_client.gui.station_numtracks,
        _settings_client.gui.station_platlength,
        _station_gui.sel_class,
        _station_gui.sel_type,
        StationID::Invalid(),
        false
    );
    c.with_error(STR_ERROR_CAN_T_BUILD_RAILROAD_STATION);
    JoinAndBuild(c, CcStation);
}

void PlaceDock(TileIndex tile, TileIndex tile_to) {
    if (CheckStationJoin(tile, tile_to)) return;

    auto c = cmd::BuildDock(
        tile,
        StationID::Invalid(),
        false
    );
    c.with_error(STR_ERROR_CAN_T_BUILD_DOCK_HERE);
    JoinAndBuild(c, CcBuildDocks);
}

void PlaceAirport(TileIndex tile) {
    if (CheckStationJoin(tile, tile)) return;

    if (_selected_airport_index == -1) return;

    uint8_t airport_type = AirportClass::Get(_selected_airport_class)->GetSpec(_selected_airport_index)->GetIndex();
    uint8_t layout = _selected_airport_layout;

    auto c = cmd::BuildAirport(
        tile,
        airport_type,
        layout,
        StationID::Invalid(),
        false
    );
    c.with_error(STR_ERROR_CAN_T_BUILD_AIRPORT_HERE);
    JoinAndBuild(c, CcBuildAirport);
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

        // SetHighlightStationToJoin(st, _station_to_join && st == _station_to_join);
        _station_building_status = (st == nullptr ? StationBuildingStatus::NEW : StationBuildingStatus::JOIN);
    } else {
        // if (_station_to_join) {
        //     SetHighlightStationToJoin(_station_to_join, true);
        //     _station_building_status = StationBuildingStatus::JOIN;
        // } else {
        //     FindStationsAroundSelection(location);
        // }
    }
    return true;
}


void SelectStationToJoin(const Station *) {
    // if (_station_to_join == station)
    //     _station_to_join = nullptr;
    // else
    //     _station_to_join = station;
    CheckRedrawStationCoverage();
}

void AbortStationPlacement() {
    // _station_to_join = nullptr;
    SetHighlightStationToJoin(nullptr, false);
}


uint GetMonthlyFrom256Tick(uint amount) {
    return ((amount * Ticks::DAY_TICKS * EconomyTime::DAYS_IN_ECONOMY_MONTH) >> 8);
}

uint GetAverageHouseProduction(uint8_t amount) {
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

    if (hs->callback_mask.Test(HouseCallbackMask::ProduceCargo)) {
        for (uint i = 0; i < 256; i++) {
            uint16 callback = GetHouseCallback(CBID_HOUSE_PRODUCE_CARGO, i, 0, house_id, t, tile);

            if (callback == CALLBACK_FAILED || callback == CALLBACK_HOUSEPRODCARGO_END) break;

            CargoType cargo = GetCargoTranslation(GB(callback, 8, 7), hs->grf_prop.grffile);

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
            if (IsValidCargoType(p.cargo)) produced[p.cargo] += ((uint)p.history[LAST_MONTH].production) << 8;
        }
    }

    return produced;
}

std::string GetStationCoverageProductionText(TileIndex tile, int w, int h, int rad, StationCoverageType sct) {
    auto production = citymania::GetProductionAroundTiles(tile, w, h, rad);

    std::ostringstream s;
    s << GetString(CM_STR_STATION_BUILD_SUPPLIES);
    bool first = true;
    for (CargoType i = 0; i < NUM_CARGO; i++) {
        if (production[i] == 0) continue;
        switch (sct) {
            case SCT_PASSENGERS_ONLY: if (!IsCargoInClass(i, CargoClass::Passengers)) continue; break;
            case SCT_NON_PASSENGERS_ONLY: if (IsCargoInClass(i, CargoClass::Passengers)) continue; break;
            case SCT_ALL: break;
            default: NOT_REACHED();
        }
        if (!first) s << ", ";
        first = false;
        s << GetString(STR_JUST_CARGO, i, production[i] >> 8);
    }
    return s.str();
}


//  ---- NEw code

StationID _station_to_join = StationID::Invalid();
std::chrono::time_point<std::chrono::system_clock> _station_to_join_selected;

void OnStationRemoved(const Station *station) {
    if (_last_built_station == station) _last_built_station = nullptr;
    if (_station_to_join == station->index) {
        _station_to_join = StationID::Invalid();
    }
    if (_ap.preview != nullptr) _ap.preview->OnStationRemoved(station);
}

static void AddAreaRectTiles(Preview::TileMap &tiles, TileArea area, SpriteID palette) {
    if (area.w == 0 || area.h == 0) return;

    if (area.w == 1 && area.h == 1) {
        tiles[area.tile].push_back(ObjectTileHighlight::make_border(palette, ZoningBorder::FULL));
        return;
    }
    auto sx = TileX(area.tile), sy = TileY(area.tile);
    auto ex = sx + area.w - 1, ey = sy + area.h - 1;
    // NOTE: Doesn't handle one-tile width/height separately but relies on border overlapping
    tiles[TileXY(sx, sy)].push_back(ObjectTileHighlight::make_border(palette, ZoningBorder::TOP_LEFT | ZoningBorder::TOP_RIGHT));
    for (auto x = sx + 1; x < ex; x++)
        tiles[TileXY(x, sy)].push_back(ObjectTileHighlight::make_border(palette, ZoningBorder::TOP_LEFT));
    tiles[TileXY(ex, sy)].push_back(ObjectTileHighlight::make_border(palette, ZoningBorder::TOP_LEFT | ZoningBorder::BOTTOM_LEFT));
    for (auto y = sy + 1; y < ey; y++) {
        tiles[TileXY(sx, y)].push_back(ObjectTileHighlight::make_border(palette, ZoningBorder::TOP_RIGHT));
        for (auto x = sx + 1; x < ex; x++) {
            tiles[TileXY(x, y)].push_back(ObjectTileHighlight::make_border(palette, ZoningBorder::NONE));
        }
        tiles[TileXY(ex, y)].push_back(ObjectTileHighlight::make_border(palette, ZoningBorder::BOTTOM_LEFT));
    }
    tiles[TileXY(sx, ey)].push_back(ObjectTileHighlight::make_border(palette, ZoningBorder::TOP_RIGHT | ZoningBorder::BOTTOM_RIGHT));
    for (auto x = sx + 1; x < ex; x++)
        tiles[TileXY(x, ey)].push_back(ObjectTileHighlight::make_border(palette, ZoningBorder::BOTTOM_RIGHT));
    tiles[TileXY(ex, ey)].push_back(ObjectTileHighlight::make_border(palette, ZoningBorder::BOTTOM_LEFT | ZoningBorder::BOTTOM_RIGHT));
}

// copied from cm_blueprint.cpp
template<typename Func>
void IterateStation(TileIndex start_tile, Axis axis, uint8_t numtracks, uint8_t plat_len, Func visitor) {
    auto plat_delta = (axis == AXIS_X ? TileDiffXY(1, 0) : TileDiffXY(0, 1));
    auto track_delta = (axis == AXIS_Y ? TileDiffXY(1, 0) : TileDiffXY(0, 1));
    TileIndex tile_track = start_tile;
    do {
        TileIndex tile = tile_track;
        int w = plat_len;
        do {
            visitor(tile);
            tile += plat_delta;
        } while (--w);
        tile_track += track_delta;
    } while (--numtracks);
}

void AddJoinAreaTiles(Preview::TileMap &tiles, StationID station_id) {
    auto station = Station::GetIfValid(station_id);
    if (station == nullptr) return;

    auto &r = station->rect;
    auto d = (int)_settings_game.station.station_spread - 1;
    TileArea ta(
        TileXY(std::max<int>(r.right - d, 0),
               std::max<int>(r.bottom - d, 0)),
        TileXY(std::min<int>(r.left + d, Map::SizeX() - 1),
               std::min<int>(r.top + d, Map::SizeY() - 1))
    );

    AddAreaRectTiles(tiles, ta, CM_PALETTE_TINT_CYAN);
}

bool RailStationPreview::IsDragDrop() const {
    return _settings_client.gui.station_dragdrop;
}

CursorID RailStationPreview::GetCursor() const {
    return SPR_CURSOR_RAIL_STATION;
}

TileArea RailStationPreview::GetArea(bool remove_mode) const {
    if (this->IsDragDrop() || remove_mode) return {this->GetStartTile(), this->cur_tile};

    if (_station_gui.axis == AXIS_X) return {this->cur_tile, _settings_client.gui.station_platlength, _settings_client.gui.station_numtracks};
    return {this->cur_tile, _settings_client.gui.station_numtracks, _settings_client.gui.station_platlength};
}

up<Command> RailStationPreview::GetCommand(bool adjacent, StationID join_to) const {
    auto ta = this->GetArea(false);
    auto start_tile = ta.tile;
    auto numtracks = ta.w;
    auto platlength = ta.h;
    if (_station_gui.axis == AXIS_X) std::swap(numtracks, platlength);

    auto res = make_up<cmd::BuildRailStation>(
        start_tile,
        _cur_railtype,
        _station_gui.axis,
        numtracks,
        platlength,
        _station_gui.sel_class,
        _station_gui.sel_type,
        join_to,
        adjacent
    );
    res->with_error(STR_ERROR_CAN_T_BUILD_RAILROAD_STATION);
    return res;
}

up<Command> RailStationPreview::GetRemoveCommand() const {
    auto res = make_up<cmd::RemoveFromRailStation>(
        this->GetStartTile(),
        this->cur_tile,
        !citymania::_fn_mod
    );
    res->with_error(STR_ERROR_CAN_T_REMOVE_PART_OF_STATION);
    return res;
}

bool RailStationPreview::Execute(up<Command> cmd, bool remove_mode) const {
    if (remove_mode) return cmd->post(&CcPlaySound_CONSTRUCTION_RAIL);
    else return cmd->post(&CcStation);
}

void RailStationPreview::AddPreviewTiles(Preview::TileMap &tiles, SpriteID palette) const {
    auto cmd = this->GetCommand(true, NEW_STATION);
    auto cmdt = dynamic_cast<cmd::BuildRailStation*>(cmd.get());
    if (cmdt == nullptr) return;

    if (palette == PAL_NONE) palette = cmd->test().Succeeded() ? CM_PALETTE_TINT_WHITE : CM_PALETTE_TINT_RED_DEEP;

    std::vector<uint8_t> layouts(cmdt->numtracks * cmdt->plat_len);
    uint8_t *layout_ptr = layouts.data();
    GetStationLayout(layout_ptr, cmdt->numtracks, cmdt->plat_len, nullptr);
    IterateStation(cmdt->tile_org, cmdt->axis, cmdt->numtracks, cmdt->plat_len,
        [&](TileIndex t) {
            uint8_t layout = *layout_ptr++;
            tiles[t].push_back(ObjectTileHighlight::make_rail_station(palette, cmdt->axis, layout & ~1));
        }
    );
}

OverlayParams RailStationPreview::GetOverlayParams() const {
    return {this->GetArea(false), CA_TRAIN, SCT_ALL};
}

bool RoadStationPreview::IsDragDrop() const {
    return true;
}

CursorID RoadStationPreview::GetCursor() const {
    return SPR_CURSOR_BUS_STATION;
    // return SPR_CURSOR_TRUCK_STATION;
}

TileArea RoadStationPreview::GetArea(bool /* remove_mode */) const {
    return {this->GetStartTile(), this->cur_tile};
}

extern DiagDirection AddAutodetectionRotation(DiagDirection ddir);  // cm_highlight.cpp

void RoadStationPreview::Update(Point pt, TileIndex tile) {
    if (pt.x == -1) return;

    auto ddir = _roadstop_gui.orientation;
    auto area = this->GetArea(false);
    if (ddir >= DIAGDIR_END && ddir < STATIONDIR_AUTO) {
        // When placed on road autorotate anyway
        if (ddir == STATIONDIR_X) {
            if (!CheckDriveThroughRoadStopDirection(area, ROAD_X))
                ddir = STATIONDIR_Y;
        } else {
            if (!CheckDriveThroughRoadStopDirection(area, ROAD_Y))
                ddir = STATIONDIR_X;
        }
    } else if (ddir == STATIONDIR_AUTO) {
        ddir = AddAutodetectionRotation(AutodetectRoadObjectDirection(tile, pt, _cur_roadtype));
    } else if (ddir == STATIONDIR_AUTO_XY) {
        ddir = AddAutodetectionRotation(AutodetectDriveThroughRoadStopDirection(area, pt, _cur_roadtype));
    }
    this->ddir = ddir;
}

up<Command> RoadStationPreview::GetCommand(bool adjacent, StationID join_to) const {
    auto area = this->GetArea(false);
    DiagDirection ddir = this->ddir;
    bool drive_through = this->ddir >= DIAGDIR_END;
    if (drive_through) ddir = static_cast<DiagDirection>(this->ddir - DIAGDIR_END); // Adjust picker result to actual direction.
    RoadStopClassID spec_class = _roadstop_gui.sel_class;
    uint16_t spec_index = _roadstop_gui.sel_type;

    auto res = make_up<cmd::BuildRoadStop>(
        area.tile,
        area.w,
        area.h,
        this->stop_type,
        drive_through,
        ddir,
        _cur_roadtype,
        spec_class,
        spec_index,
        join_to,
        adjacent
    );

    return res;
}

up<Command> RoadStationPreview::GetRemoveCommand() const {
    auto area = this->GetArea(false);
    auto res = make_up<cmd::RemoveRoadStop>(
        area.tile,
        area.w,
        area.h,
        this->stop_type,
        citymania::_fn_mod
    );
    auto rti = GetRoadTypeInfo(_cur_roadtype);
    // res->with_error(rti->strings.err_remove_station[this->stop_type]);
    return res;
}

bool RoadStationPreview::Execute(up<Command> cmd, bool remove_mode) const {
    if (remove_mode) return cmd->post(&CcPlaySound_CONSTRUCTION_OTHER);
    else return cmd->post(&CcRoadStop);
}

void RoadStationPreview::AddPreviewTiles(Preview::TileMap &tiles, SpriteID palette) const {
    auto cmd = this->GetCommand(true, NEW_STATION);
    auto cmdt = dynamic_cast<cmd::BuildRoadStop*>(cmd.get());
    if (cmdt == nullptr) return;

    if (palette == PAL_NONE) palette = cmd->test().Succeeded() ? CM_PALETTE_TINT_WHITE : CM_PALETTE_TINT_RED_DEEP;

    for (TileIndex t : this->GetArea(false)) {
        auto ddir = cmdt->ddir;
        if (cmdt->is_drive_through) ddir = ddir + DIAGDIR_END;
        tiles[t].push_back(ObjectTileHighlight::make_road_stop(
            palette,
            cmdt->rt,
            ddir,
            cmdt->stop_type == RoadStopType::Truck,
            cmdt->spec_class,
            cmdt->spec_index
        ));
    }
}

OverlayParams RoadStationPreview::GetOverlayParams() const {
    return {
        this->GetArea(false),
        this->stop_type == RoadStopType::Truck ? CA_TRUCK : CA_BUS,
        this->stop_type == RoadStopType::Truck ? SCT_NON_PASSENGERS_ONLY : SCT_PASSENGERS_ONLY
    };
}

bool DockPreview::IsDragDrop() const {
    return false;
}

CursorID DockPreview::GetCursor() const {
    return SPR_CURSOR_DOCK;
}

TileArea DockPreview::GetArea(bool /* remove_mode */) const {
    auto tile = this->GetStartTile();
    TileIndex tile_to = (this->ddir != INVALID_DIAGDIR ? TileAddByDiagDir(tile, this->ddir) : tile);
    return {tile, tile_to};
}

void DockPreview::Update(Point pt, TileIndex tile) {
    if (pt.x == -1) return;
    this->ddir = GetInclinedSlopeDirection(GetTileSlope(tile));
    if (this->ddir == INVALID_DIAGDIR) this->ddir = DIAGDIR_NE;
    else this->ddir = ReverseDiagDir(this->ddir);
}

up<Command> DockPreview::GetCommand(bool adjacent, StationID join_to) const {
    // STR_ERROR_CAN_T_BUILD_DOCK_HERE
    return make_up<cmd::BuildDock>(
        this->GetStartTile(),
        join_to,
        adjacent
    );
}

up<Command> DockPreview::GetRemoveCommand() const {
    return nullptr;
}

bool DockPreview::Execute(up<Command> cmd, bool remove_mode) const {
    cmd->post(&CcBuildDocks);
}

void DockPreview::AddPreviewTiles(Preview::TileMap &tiles, SpriteID palette) const {
    auto t = this->GetStartTile();
    tiles[t].push_back(ObjectTileHighlight::make_dock_slope(CM_PALETTE_TINT_WHITE, this->ddir));
    t += TileOffsByDiagDir(this->ddir);
    tiles[t].push_back(ObjectTileHighlight::make_dock_flat(CM_PALETTE_TINT_WHITE, DiagDirToAxis(this->ddir)));
    // TODO
    // auto cmd = this->GetCommand(true, NEW_STATION);
    // auto cmdt = dynamic_cast<cmd::BuildRoadStop*>(cmd.get());
    // if (cmdt == nullptr) return;

    // if (palette == PAL_NONE) palette = cmd->test().Succeeded() ? CM_PALETTE_TINT_WHITE : CM_PALETTE_TINT_RED_DEEP;

    // for (TileIndex t : this->GetArea(false)) {
    //     auto ddir = cmdt->ddir;
    //     if (cmdt->is_drive_through) ddir = ddir + DIAGDIR_END;
    //     tiles[t].push_back(ObjectTileHighlight::make_road_stop(
    //         palette,
    //         cmdt->rt,
    //         ddir,
    //         cmdt->stop_type == ROADSTOP_TRUCK,
    //         cmdt->spec_class,
    //         cmdt->spec_index
    //     ));
    // }
}

OverlayParams DockPreview::GetOverlayParams() const {
    return {
        this->GetArea(false),
        CA_DOCK,
        SCT_ALL
    };
}


void StationPreviewBase::AddAreaTiles(Preview::TileMap &tiles, bool add_current, bool show_join_area) {
    Station *st_join = Station::GetIfValid(this->station_to_join);
    std::set<TileIndex> join_area;

    if (show_join_area && st_join != nullptr) {
        AddJoinAreaTiles(tiles, st_join->index);
        for (auto t : tiles) join_area.insert(t.first);
    }

    if (this->show_coverage && st_join != nullptr) {
        // Add joining station coverage
        for (auto t : st_join->catchment_tiles) {
            auto pal = join_area.find(t) != join_area.end() ? CM_PALETTE_TINT_CYAN_WHITE : CM_PALETTE_TINT_WHITE;
            tiles[t].push_back(ObjectTileHighlight::make_tint(pal));
        }
    }

    if (this->show_coverage && add_current) {
        // Add current station coverage
        auto rad = CA_UNMODIFIED;
        if (_settings_game.station.modified_catchment) rad = CA_TRAIN;
        auto area = this->type->GetArea(false);
        area.Expand(rad);
        area.ClampToMap();
        for (auto t : area) {
            auto pal = join_area.find(t) != join_area.end() ? CM_PALETTE_TINT_CYAN_WHITE : CM_PALETTE_TINT_WHITE;
            tiles[t].push_back(ObjectTileHighlight::make_tint(pal));
        }
    }

    if (st_join != nullptr) {
        TileArea ta(TileXY(st_join->rect.left, st_join->rect.top), TileXY(st_join->rect.right, st_join->rect.bottom));
        for (TileIndex t : ta) {
            if (!IsTileType(t, MP_STATION) || GetStationIndex(t) != st_join->index) continue;
            tiles[t].push_back(ObjectTileHighlight::make_struct_tint(CM_PALETTE_TINT_BLUE));
        }
    }
}

void StationPreviewBase::Update(Point pt, TileIndex tile) {
    if (tile != INVALID_TILE) this->type->cur_tile = tile;
    this->show_coverage = _settings_client.gui.station_show_coverage;
    this->remove_mode = false;
    if (_remove_button_clicked) {
        this->remove_mode = true;
        this->keep_rail = !_fn_mod;
    } else if (!this->type->IsDragDrop()) {
        this->type->start_tile = INVALID_TILE;
    }
    this->type->Update(pt, tile);
}

bool StationPreviewBase::HandleMousePress() {
    if (!IsValidTile(this->type->cur_tile)) return false;

    if (this->remove_mode || this->type->IsDragDrop()) {
        this->type->start_tile = this->type->cur_tile;
        return true;
    }

    this->Execute();
    return true;
}

void StationPreviewBase::HandleMouseRelease() {
    if (!IsValidTile(this->type->cur_tile)) return;

    if (this->type->start_tile != INVALID_TILE) {
        this->Execute();
        this->type->start_tile = INVALID_TILE;
    }
}

std::vector<std::pair<SpriteID, std::string>> StationPreviewBase::GetOverlayData() {
    if (this->remove_mode) return {};

    std::vector<std::pair<SpriteID, std::string>> res;
    auto params = this->type->GetOverlayParams();

    if (!_settings_game.station.modified_catchment) params.radius = CA_UNMODIFIED;
    auto production = citymania::GetProductionAroundTiles(params.area.tile, params.area.w, params.area.h, params.radius);
    bool has_header = false;
    for (CargoType i = 0; i < NUM_CARGO; i++) {
        if (production[i] == 0) continue;

        switch (params.coverage_type) {
            case SCT_PASSENGERS_ONLY: if (!IsCargoInClass(i, CargoClass::Passengers)) continue; break;
            case SCT_NON_PASSENGERS_ONLY: if (IsCargoInClass(i, CargoClass::Passengers)) continue; break;
            case SCT_ALL: break;
            default: NOT_REACHED();
        }

        const CargoSpec *cs = CargoSpec::Get(i);
        if (cs == nullptr) continue;

        if (!has_header) {
            res.emplace_back(PAL_NONE, GetString(CM_STR_BUILD_INFO_OVERLAY_STATION_SUPPLIES));
            has_header = true;
        }
        res.emplace_back(cs->GetCargoIcon(),
            GetString(CM_STR_BUILD_INFO_OVERLAY_STATION_CARGO, i, production[i] >> 8));
    }
    return res;
}

up<Command> StationPreviewBase::GetCommand(bool adjacent, StationID join_to) {
    if (this->remove_mode) return this->type->GetRemoveCommand();
    return this->type->GetCommand(adjacent, join_to);
}

Preview::TileMap VanillaStationPreview::GetTiles() {
    Preview::TileMap tiles;

    if (!IsValidTile(this->type->cur_tile)) return tiles;

    if (this->remove_mode) {
        AddAreaRectTiles(tiles, this->type->GetArea(true), CM_PALETTE_TINT_RED_DEEP);
        return tiles;
    }

    this->AddAreaTiles(tiles, true, false);
    this->type->AddPreviewTiles(tiles, this->palette);

    return tiles;
}

void VanillaStationPreview::Update(Point pt, TileIndex tile) {
    StationPreviewBase::Update(pt, tile);
    this->palette = CM_PALETTE_TINT_WHITE;

    if (this->remove_mode) return;
    if (this->selected_station_to_join != StationID::Invalid()) {
        this->station_to_join = this->selected_station_to_join;
        return;
    }

    if (!IsValidTile(this->type->cur_tile)) return;
    this->station_to_join = StationID::Invalid();
    auto area = this->type->GetArea(false);
    area.Expand(1);
    area.ClampToMap();
    for (auto tile : area) {
        if (IsTileType(tile, MP_STATION) && GetTileOwner(tile) == _local_company) {
            Station *st = Station::GetByTile(tile);
            if (st == nullptr || st->index == this->station_to_join) continue;
            if (this->station_to_join != StationID::Invalid()) {
                this->station_to_join = StationID::Invalid();
                this->palette = CM_PALETTE_TINT_YELLOW;
                break;
            }
            this->station_to_join = st->index;
            // TODO check for command to return multiple? but also check each to
            // see if they can be built
            // if (this->GetCommand(true, st->index)->test().Succeeded()) {
            //     if (this->station_to_join != StationID::Invalid()) {
            //         this->station_to_join = StationID::Invalid();
            //         this->palette = CM_PALETTE_TINT_YELLOW;
            //         break;
            //     } else this->station_to_join = st->index;
            // }
        }
    }
    if (this->station_to_join == StationID::Invalid() && !this->GetCommand(true, NEW_STATION)->test().Succeeded())
        this->palette = CM_PALETTE_TINT_RED_DEEP;
}

void VanillaStationPreview::Execute() {
    if (this->remove_mode) {
        this->type->Execute(this->type->GetRemoveCommand(), true);
        return;
    }
    auto proc = [type=this->type](bool test, StationID to_join) -> bool {
        auto cmd = type->GetCommand(_fn_mod, to_join);
        if (test) return cmd->test().Succeeded();
        return type->Execute(std::move(cmd), false);
    };
    ShowSelectStationIfNeeded(this->type->GetArea(false), proc);
}

void VanillaStationPreview::OnStationRemoved(const Station *station) {
    if (this->station_to_join == station->index) this->station_to_join = StationID::Invalid();
    if (this->selected_station_to_join == station->index) this->station_to_join = StationID::Invalid();
}

StationPreview::StationPreview(sp<PreviewStationType> type)
    :StationPreviewBase{type}
{
    auto seconds_since_selected = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - _station_to_join_selected).count();
    if (seconds_since_selected < 30) this->station_to_join = _station_to_join;
    else this->station_to_join = StationID::Invalid();
}

StationPreview::~StationPreview() {
    _station_to_join_selected = std::chrono::system_clock::now();
}

up<Command> StationPreview::GetCommand() {
    if (this->select_mode) return nullptr;

    auto res = StationPreviewBase::GetCommand(true, this->station_to_join);
    if (this->remove_mode) return res;

    res->with_callback([] (bool res) -> bool {
        if (!res) return false;
        if (_last_built_station == nullptr) return false;
        _station_to_join = _last_built_station->index;
        _station_to_join_selected = std::chrono::system_clock::now();
        auto p = dynamic_cast<StationPreview*>(_ap.preview.get());
        if (p == nullptr) return false;
        p->station_to_join = _last_built_station->index;
        return true;
    });
    return res;
}

Preview::TileMap StationPreview::GetTiles() {
    Preview::TileMap tiles;

    if (!IsValidTile(this->type->cur_tile)) return tiles;

    if (this->remove_mode) {
        AddAreaRectTiles(tiles, this->type->GetArea(true), CM_PALETTE_TINT_RED_DEEP);
        return tiles;
    }

    this->AddAreaTiles(tiles, !this->select_mode, true);

    if (this->select_mode) {
        tiles[this->type->cur_tile].push_back(ObjectTileHighlight::make_border(CM_PALETTE_TINT_BLUE, ZoningBorder::FULL));
        return tiles;
    }

    this->type->AddPreviewTiles(tiles, PAL_NONE);

    return tiles;
}

void StationPreview::Update(Point pt, TileIndex tile) {
    this->select_mode = false;
    StationPreviewBase::Update(pt, tile);
    if (!this->remove_mode && _fn_mod) {
        this->select_mode = true;
        this->type->start_tile = INVALID_TILE;
    }
}

bool StationPreview::HandleMousePress() {
    if (!IsValidTile(this->type->cur_tile)) return false;

    if (this->select_mode) {
        if (IsTileType(this->type->cur_tile, MP_STATION)) {
            auto st = Station::GetByTile(this->type->cur_tile);
            this->station_to_join = st->index;
            _station_to_join = this->station_to_join;
            _station_to_join_selected = std::chrono::system_clock::now();
        } else {
            this->station_to_join = StationID::Invalid();
            _station_to_join = StationID::Invalid();
        }
        return true;
    }

    return StationPreviewBase::HandleMousePress();
}

void StationPreview::Execute() {
    this->type->Execute(std::move(this->GetCommand()), this->remove_mode);
}

void StationPreview::OnStationRemoved(const Station *station) {
    if (this->station_to_join == station->index) this->station_to_join = StationID::Invalid();
}

void SetSelectedStationToJoin(StationID station_id) {
    auto p = dynamic_cast<VanillaStationPreview*>(_ap.preview.get());
    if (p == nullptr) return;
    p->selected_station_to_join = station_id;
    UpdateActivePreview();
}

bool HandleStationPlacePushButton(Window *w, WidgetID widget, sp<PreviewStationType> type) {
    up<Preview> preview;
    if (citymania::UseImprovedStationJoin()) {
        preview = make_up<StationPreview>(type);
    } else {
        preview = make_up<VanillaStationPreview>(type);
    }
    return citymania::HandlePlacePushButton(w, widget, std::move(preview));
}

} // namespace citymania
