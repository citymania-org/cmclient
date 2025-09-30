#include "../stdafx.h"

#include "cm_station_gui.hpp"

#include "cm_command_type.hpp"
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
#include "../newgrf_airport.h"  // AirportClassID, AirportClass
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

#include <cassert>
#include <cstdio>
#include <optional>
#include <sstream>
#include <unordered_set>

bool _remove_button_clicked;  // replace vanilla static vars

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
extern RoadType _cur_roadtype;  // road_gui.cpp
extern void GetStationLayout(byte *layout, uint numtracks, uint plat_len, const StationSpec *statspec);
extern citymania::RailStationGUISettings _railstation; ///< Settings of the station builder GUI

struct RoadStopGUISettings {
    DiagDirection orientation;

    RoadStopClassID roadstop_class;
    uint16_t roadstop_type;
    uint16_t roadstop_count;
};
extern RoadStopGUISettings _roadstop_gui_settings;

extern AirportClassID _selected_airport_class; ///< the currently visible airport class
extern int _selected_airport_index;            ///< the index of the selected airport in the current class or -1
extern byte _selected_airport_layout;          ///< selected airport layout number.

namespace citymania {

const Station *_highlight_station_to_join = nullptr;
TileArea _highlight_join_area;

bool UseImprovedStationJoin() {
    return _settings_client.gui.cm_use_improved_station_join && _settings_game.station.distant_join_stations && _settings_game.station.adjacent_stations;
}

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
    // _highlight_join_area = ta;
    MarkTileAreaDirty(_highlight_join_area);
}

static void MarkCoverageAreaDirty(const Station *station) {
    MarkTileAreaDirty(station->catchment_tiles);
}

void MarkCoverageHighlightDirty() {
    MarkCatchmentTilesDirty();
}

void OnStationTileSetChange(const Station *station, bool /* adding */, StationType /* type */) {
    // TODO
    // if (station == _highlight_station_to_join) {
    //     // if (_highlight_join_area.tile != INVALID_TILE)
    //         // UpdateHiglightJoinArea(_station_to_join);
    //     if (_settings_client.gui.station_show_coverage)
    //         MarkCoverageAreaDirty(_highlight_station_to_join);
    // }
    // if (station == _viewport_highlight_station) MarkCoverageAreaDirty(_viewport_highlight_station);
}

void OnStationDeleted(const Station *station) {
    // TODO
    // if (_highlight_station_to_join == station) {
    //     MarkCoverageAreaDirty(station);
    //     _highlight_station_to_join = nullptr;
    // }
}

// const Station *_last_built_station;
void OnStationPartBuilt(const Station *station) {
    // _last_built_station = station;
    // CheckRedrawStationCoverage();
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

// bool CheckStationJoin(TileIndex start_tile, TileIndex /* end_tile */) {
//     if (citymania::_fn_mod) {
//         if (IsTileType (start_tile, MP_STATION)) {
//             citymania::SelectStationToJoin(Station::GetByTile(start_tile));
//             return true;
//         }
//         auto st = CheckClickOnDeadStationSign();
//         if (st) {
//             citymania::SelectStationToJoin(st);
//             return true;
//         }
//     }
//     return false;
// }

// template <typename Tcommand, typename Tcallback>
// void JoinAndBuild(Tcommand command, Tcallback *callback) {
//     auto join_to = _highlight_station_to_join;
//     command.adjacent = (citymania::_fn_mod || join_to);
//     command.station_to_join = INVALID_STATION;

//     if (citymania::_fn_mod) command.station_to_join = NEW_STATION;
//     else if (join_to) command.station_to_join = join_to->index;

//     command.with_callback([] (bool res)->bool {
//         if (!res) return false;
//         // _station_to_join = _last_built_station;
//         return true;
//     }).post(callback);
// }

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

//  ---- NEw code

static TileArea GetStationJoinArea(StationID station_id) {
    auto station = Station::GetIfValid(station_id);
    if (station == nullptr) return {};

    auto &r = station->rect;
    auto d = (int)_settings_game.station.station_spread - 1;
    TileArea ta(
        TileXY(std::max<int>(r.right - d, 0),
               std::max<int>(r.bottom - d, 0)),
        TileXY(std::min<int>(r.left + d, Map::SizeX() - 1),
               std::min<int>(r.top + d, Map::SizeY() - 1))
    );

    return ta;
}

extern DiagDirection AddAutodetectionRotation(DiagDirection ddir);  // cm_highlight.cpp

// void VanillaStationPreview::Update(Point pt, TileIndex tile) {
//     StationPreviewBase::Update(pt, tile);
//     this->palette = CM_PALETTE_TINT_WHITE;

//     if (this->remove_mode) return;
//     if (this->selected_station_to_join != INVALID_STATION) {
//         this->station_to_join = this->selected_station_to_join;
//         return;
//     }

//     if (!IsValidTile(this->type->cur_tile)) return;
//     this->station_to_join = INVALID_STATION;
//     auto area = this->type->GetArea(false);
//     area.Expand(1);
//     area.ClampToMap();
//     for (auto tile : area) {
//         if (IsTileType(tile, MP_STATION) && GetTileOwner(tile) == _local_company) {
//             Station *st = Station::GetByTile(tile);
//             if (st == nullptr || st->index == this->station_to_join) continue;
//             if (this->station_to_join != INVALID_STATION) {
//                 this->station_to_join = INVALID_STATION;
//                 this->palette = CM_PALETTE_TINT_YELLOW;
//                 break;
//             }
//             this->station_to_join = st->index;
//             // TODO check for command to return multiple? but also check each to
//             // see if they can be built
//             // if (this->GetCommand(true, st->index)->test().Succeeded()) {
//             //     if (this->station_to_join != INVALID_STATION) {
//             //         this->station_to_join = INVALID_STATION;
//             //         this->palette = CM_PALETTE_TINT_YELLOW;
//             //         break;
//             //     } else this->station_to_join = st->index;
//             // }
//         }
//     }
//     if (this->station_to_join == INVALID_STATION && !this->GetCommand(true, NEW_STATION)->test().Succeeded())
//         this->palette = CM_PALETTE_TINT_RED_DEEP;
// }

// void VanillaStationPreview::Execute() {
//     if (this->remove_mode) {
//         this->type->Execute(this->type->GetRemoveCommand(), true);
//         return;
//     }
//     auto proc = [type=this->type](bool test, StationID to_join) -> bool {
//         auto cmd = type->GetCommand(_fn_mod, to_join);
//         if (test) return cmd->test().Succeeded();
//         return type->Execute(std::move(cmd), false);
//     };
//     ShowSelectStationIfNeeded(this->type->GetArea(false), proc);
// }

// void VanillaStationPreview::OnStationRemoved(const Station *station) {
//     if (this->station_to_join == station->index) this->station_to_join = INVALID_STATION;
//     if (this->selected_station_to_join == station->index) this->station_to_join = INVALID_STATION;
// }

// StationPreview::StationPreview(sp<PreviewStationType> type)
//     :StationPreviewBase{type}
// {
//     auto seconds_since_selected = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - _station_to_join_selected).count();
//     if (seconds_since_selected < 30) this->station_to_join = _station_to_join;
//     else this->station_to_join = INVALID_STATION;
// }

// StationPreview::~StationPreview() {
//     _station_to_join_selected = std::chrono::system_clock::now();
// }

// up<Command> StationPreview::GetCommand() {
//     if (this->select_mode) return nullptr;

//     auto res = StationPreviewBase::GetCommand(true, this->station_to_join);
//     if (this->remove_mode) return res;

//     res->with_callback([] (bool res) -> bool {
//         if (!res) return false;
//         if (_last_built_station == nullptr) return false;
//         _station_to_join = _last_built_station->index;
//         _station_to_join_selected = std::chrono::system_clock::now();
//         // auto p = dynamic_cast<StationPreview*>(_ap.preview.get());
//         // if (p == nullptr) return false;
//         // p->station_to_join = _last_built_station->index;
//         return true;
//     });
//     return res;
// }

// void StationPreview::OnStationRemoved(const Station *station) {
//     if (this->station_to_join == station->index) this->station_to_join = INVALID_STATION;
// }

void SetSelectedStationToJoin(StationID station_id) {
    StationBuildTool::current_selected_station = station_id;
    UpdateActiveTool();
}

void ResetJoinStationHighlight() {
    StationBuildTool::active_highlight = std::nullopt;
    SetSelectedStationToJoin(INVALID_STATION);
}


void OnStationRemoved(const Station *station) {
    // if (_last_built_station == station) _last_built_station = nullptr;
    if (StationBuildTool::station_to_join == station->index) {
        StationBuildTool::station_to_join = INVALID_STATION;
        UpdateActiveTool();
    }
    if (StationBuildTool::current_selected_station == station->index) {
        StationBuildTool::current_selected_station = INVALID_STATION;
        UpdateActiveTool();
    }
    // TODO?
    // if (GetActiveTool() != nullptr) GetActiveTool()->OnStationRemoved(station);
}

void AbortStationPlacement() {
    // TODO is it necessary?
    // SetHighlightStationToJoin(station=nullptr, with_area=false);
}


// --- Action base class ---
void Action::OnStationRemoved(const Station *) {}

// --- RemoveAction ---
template <ImplementsRemoveHandler Handler>
void RemoveAction<Handler>::Update(Point, TileIndex tile) {
    this->cur_tile = tile;
}

template <ImplementsRemoveHandler Handler>
bool RemoveAction<Handler>::HandleMousePress() {
    if (!IsValidTile(this->cur_tile)) return false;
    this->start_tile = this->cur_tile;
    return true;
}

template <ImplementsRemoveHandler Handler>
void RemoveAction<Handler>::HandleMouseRelease() {
    auto area = this->GetArea();
    if (!area.has_value()) return;
    this->handler.Execute(area.value());
    this->start_tile = INVALID_TILE;
}

template <ImplementsRemoveHandler Handler>
std::optional<TileArea> RemoveAction<Handler>::GetArea() const {
    if (!IsValidTile(this->cur_tile)) return std::nullopt;
    if (!IsValidTile(this->start_tile)) return TileArea{this->cur_tile, this->cur_tile};
    return TileArea{this->start_tile, this->cur_tile};
}

template <ImplementsRemoveHandler Handler>
ToolGUIInfo RemoveAction<Handler>::GetGUIInfo() {
    HighlightMap hlmap;
    BuildInfoOverlayData data;
    auto area = this->GetArea();
    CommandCost cost;
    if (area.has_value()) {
        hlmap.AddTileAreaWithBorder(area.value(), CM_PALETTE_TINT_RED_DEEP);
        auto cmd = this->handler.GetCommand(area.value());
        if (cmd) cost = cmd->test();
    }
    return {hlmap, data, cost};
}

template <ImplementsRemoveHandler Handler>
void RemoveAction<Handler>::OnStationRemoved(const Station *) {}

static HighlightMap PrepareHighilighMap(Station *st_join, ObjectHighlight &ohl, SpriteID pal, bool show_join_area, StationCoverageType sct, uint rad) {
    bool add_current = true;  // FIXME
    bool show_coverage = (rad > 0);

    auto hlmap = ohl.GetHighlightMap(pal);
    TileArea join_area;
    std::set<TileIndex> coverage_area;

    if (show_join_area && st_join != nullptr) {
        join_area = GetStationJoinArea(st_join->index);
        hlmap.AddTileArea(join_area, CM_PALETTE_TINT_CYAN);
    }

    if (show_coverage && st_join != nullptr) {
        // Add joining station coverage
        for (auto t : st_join->catchment_tiles) {
            auto pal = join_area.Contains(t) ? CM_PALETTE_TINT_CYAN_WHITE : CM_PALETTE_TINT_WHITE;
            hlmap.Add(t, ObjectTileHighlight::make_tint(pal));
            coverage_area.insert(t);
        }
    }

    auto area = ohl.GetArea();
    if (!_settings_game.station.modified_catchment) rad = CA_UNMODIFIED;
    std::optional<TileArea> rad_area = std::nullopt;
    if (area.has_value()) {
        auto xarea = area.value();
        xarea.Expand(rad);
        xarea.ClampToMap();
        rad_area = xarea;
    }
    if (show_coverage && add_current && rad_area.has_value()) {
        // Add current station coverage
        for (auto t : rad_area.value()) {
            auto pal = join_area.Contains(t) ? CM_PALETTE_TINT_CYAN_WHITE : CM_PALETTE_TINT_WHITE;
            hlmap.Add(t, ObjectTileHighlight::make_tint(pal));
            coverage_area.insert(t);
        }
    }

    if (show_coverage) {
        hlmap.AddTilesBorder(coverage_area, CM_PALETTE_TINT_WHITE);
    }

    if (st_join != nullptr) {
        // Highlight joining station blue
        TileArea ta(TileXY(st_join->rect.left, st_join->rect.top), TileXY(st_join->rect.right, st_join->rect.bottom));
        for (TileIndex t : ta) {
            if (!IsTileType(t, MP_STATION) || GetStationIndex(t) != st_join->index) continue;
            hlmap.Add(t, ObjectTileHighlight::make_struct_tint(CM_PALETTE_TINT_BLUE));
        }
    }

    return hlmap;
}

ToolGUIInfo PlacementAction::PrepareGUIInfo(std::optional<ObjectHighlight> ohl, up<Command> cmd, StationCoverageType sct, uint rad) {
    if (!cmd || !ohl.has_value()) return {};
    ohl.value().UpdateTiles();

    auto cost = cmd->test();

    bool show_coverage = _settings_client.gui.station_show_coverage;

    auto hlmap = PrepareHighilighMap(
        Station::GetIfValid(StationBuildTool::station_to_join),
        ohl.value(),
        cost.Succeeded() ? CM_PALETTE_TINT_WHITE : CM_PALETTE_TINT_RED_DEEP,
        true,
        sct,
        show_coverage ? rad : 0
    );

    // Prepare build overlay

    BuildInfoOverlayData data;

    if (StationBuildTool::station_to_join != INVALID_STATION) {
        SetDParam(0, StationBuildTool::station_to_join);
        data.emplace_back(0, PAL_NONE, GetString(CM_STR_BULID_INFO_OVERLAY_JOIN_STATION));
    } else {
        data.emplace_back(0, PAL_NONE, GetString(CM_STR_BULID_INFO_OVERLAY_NEW_STATION));
    }

    auto area = ohl.value().GetArea();
    if (area.has_value()) {
        // Add supplied cargo information
        // TODO can we use rad_area since we already have it?
        auto production = citymania::GetProductionAroundTiles(area->tile, area->w, area->h, rad);
        bool has_header = false;
        for (CargoID i = 0; i < NUM_CARGO; i++) {
            if (production[i] == 0) continue;

            switch (sct) {
                case SCT_PASSENGERS_ONLY: if (!IsCargoInClass(i, CC_PASSENGERS)) continue; break;
                case SCT_NON_PASSENGERS_ONLY: if (IsCargoInClass(i, CC_PASSENGERS)) continue; break;
                case SCT_ALL: break;
                default: NOT_REACHED();
            }

            const CargoSpec *cs = CargoSpec::Get(i);
            if (cs == nullptr) continue;

            if (!has_header) {
                data.emplace_back(0, PAL_NONE, GetString(CM_STR_BUILD_INFO_OVERLAY_STATION_SUPPLIES));
                has_header = true;
            }
            SetDParam(0, i);
            SetDParam(1, production[i] >> 8);
            data.emplace_back(1, cs->GetCargoIcon(), GetString(CM_STR_BUILD_INFO_OVERLAY_STATION_CARGO));
        }

        // Add accepted cargo information
        CargoTypes always_accepted;
        auto cargoes = ::GetAcceptanceAroundTiles(area->tile, area->w, area->h, rad, &always_accepted);
        /* Convert cargo counts to a set of cargo bits, and draw the result. */
        std::vector<std::pair<uint, std::string>> cargostr;
        for (CargoID i = 0; i < NUM_CARGO; i++) {
            switch (sct) {
                case SCT_PASSENGERS_ONLY: if (!IsCargoInClass(i, CC_PASSENGERS)) continue; break;
                case SCT_NON_PASSENGERS_ONLY: if (IsCargoInClass(i, CC_PASSENGERS)) continue; break;
                case SCT_ALL: break;
                default: NOT_REACHED();
            }
            if (cargoes[i] > 0) {
                SetDParam(0, 1 << i);
                if(cargoes[i] < 8) {
                    SetDParam(1, cargoes[i]);
                    cargostr.emplace_back(2, GetString(CM_STR_BULID_INFO_OVERLAY_ACCEPTS_CARGO_PARTIAL));
                } else if (HasBit(always_accepted, i)) {
                    SetDParam(1, cargoes[i]);
                    cargostr.emplace_back(1, GetString(CM_STR_BULID_INFO_OVERLAY_ACCEPTS_CARGO_FULL));
                } else {
                    cargostr.emplace_back(0, GetString(CM_STR_BULID_INFO_OVERLAY_ACCEPTS_CARGO));
                }
            }
        }

        std::string cargoliststr;
        if  (cargostr.size() > 0) {
            // Sort by first element (priority)
            std::stable_sort(cargostr.begin(), cargostr.end(), [](const auto& a, const auto& b) {
                return a.first < b.first;
            });
            std::vector<std::string> cargolistsorted;
            cargolistsorted.reserve((cargostr.size()));
            for (auto [_, s] : cargostr) cargolistsorted.push_back(s);
            cargoliststr = string::join(cargolistsorted, ", ");
        } else {
            cargoliststr = GetString(STR_JUST_NOTHING);
        }
        SetDParamStr(0, cargoliststr);
        data.emplace_back(0, PAL_NONE, GetString(CM_STR_BULID_INFO_OVERLAY_ACCEPTS));

        Town *t = ClosestTownFromTile(area->tile, _settings_game.economy.dist_local_authority);
        if (t) {
            auto town_allowed = CheckIfAuthorityAllowsNewStation(area->tile, DC_NONE).Succeeded();
            auto rating = t->ratings[_current_company];
            auto dist = DistanceManhattan(t->xy, area->tile);
            SetDParam(0, t->index);
            SetDParam(1, rating);
            if (dist <= 10) {
                SetDParam(2, CM_STR_BULID_INFO_OVERLAY_TOWN_S_ADS);
            } else if (dist <= 15) {
                SetDParam(2, CM_STR_BULID_INFO_OVERLAY_TOWN_M_ADS);
            } else if (dist <= 20) {
                SetDParam(2, CM_STR_BULID_INFO_OVERLAY_TOWN_L_ADS);
            } else {
                SetDParam(2, CM_STR_BULID_INFO_OVERLAY_TOWN_NO_ADS);
            }
            data.emplace_back(0, PAL_NONE, GetString(town_allowed ? CM_STR_BULID_INFO_OVERLAY_TOWN_ALLOWS : CM_STR_BULID_INFO_OVERLAY_TOWN_DENIES));
        } else {
            data.emplace_back(0, PAL_NONE, GetString(CM_STR_BULID_INFO_OVERLAY_TOWN_NONE));
        }

        SetDParam(0, area->w);
        SetDParam(1, area->h);
        data.emplace_back(0, PAL_NONE, GetString(CM_STR_BULID_INFO_OVERLAY_STATION_SIZE));
    }

    return {hlmap, data, cost};
}

ToolGUIInfo GetSelectedStationGUIInfo() {
    if (!StationBuildTool::active_highlight.has_value()) return {};
    auto &ohl = StationBuildTool::active_highlight.value();
    // TODO maybe update or none at all?
    ohl.UpdateTiles();
    auto hlmap = PrepareHighilighMap(
        Station::GetIfValid(StationBuildTool::current_selected_station),
        ohl,
        CM_PALETTE_TINT_WHITE,
        false,
        SCT_ALL,
        0
    );
    return {hlmap, {}, {}};
}

// --- SizedPlacementAction ---
template <ImplementsSizedPlacementHandler Handler>
void SizedPlacementAction<Handler>::Update(Point, TileIndex tile) {
    this->cur_tile = tile;
}

template <ImplementsSizedPlacementHandler Handler>
bool SizedPlacementAction<Handler>::HandleMousePress() {
    return IsValidTile(this->cur_tile);
}

template <ImplementsSizedPlacementHandler Handler>
void SizedPlacementAction<Handler>::HandleMouseRelease() {
    if (!IsValidTile(this->cur_tile)) return;
    this->handler.Execute(this->cur_tile);
}

template <ImplementsSizedPlacementHandler Handler>
ToolGUIInfo SizedPlacementAction<Handler>::GetGUIInfo() {
    if (!IsValidTile(this->cur_tile)) return {};
    auto [sct, rad] = this->handler.GetCatchmentParams();
    return this->PrepareGUIInfo(
        this->handler.GetObjectHighlight(this->cur_tile),
        this->handler.GetCommand(this->cur_tile, INVALID_STATION),
        sct,
        rad
    );
}

template <ImplementsSizedPlacementHandler Handler>
void SizedPlacementAction<Handler>::OnStationRemoved(const Station *) {}

// --- DragNDropPlacementAction ---

template <ImplementsDragNDropPlacementHandler Handler>
std::optional<TileArea> DragNDropPlacementAction<Handler>::GetArea() const {
    // TODO separate common fuctions with RemoveAction into base class
    if (!IsValidTile(this->cur_tile)) return std::nullopt;
    if (!IsValidTile(this->start_tile)) return TileArea{this->cur_tile, this->cur_tile};
    return TileArea{this->start_tile, this->cur_tile};
}

template <ImplementsDragNDropPlacementHandler Handler>
void DragNDropPlacementAction<Handler>::Update(Point, TileIndex tile) {
    this->cur_tile = tile;
}

template <ImplementsDragNDropPlacementHandler Handler>
bool DragNDropPlacementAction<Handler>::HandleMousePress() {
    if (!IsValidTile(this->cur_tile)) return false;
    this->start_tile = this->cur_tile;
    return true;
}

template <ImplementsDragNDropPlacementHandler Handler>
void DragNDropPlacementAction<Handler>::HandleMouseRelease() {
    auto area = this->GetArea();
    if (!area.has_value()) return;
    this->handler.Execute(area.value());
    this->start_tile = INVALID_TILE;
}

template <ImplementsDragNDropPlacementHandler Handler>
ToolGUIInfo DragNDropPlacementAction<Handler>::GetGUIInfo() {
    auto area = this->GetArea();
    if (!area.has_value()) return {};
    auto ohl = this->handler.GetObjectHighlight(area.value());
    auto [sct, rad] = this->handler.GetCatchmentParams();
    return this->PrepareGUIInfo(
        this->handler.GetObjectHighlight(area.value()),
        this->handler.GetCommand(area.value(), INVALID_STATION),
        sct,
        rad
    );
}

template <ImplementsDragNDropPlacementHandler Handler>
void DragNDropPlacementAction<Handler>::OnStationRemoved(const Station *) {}

// --- StationSelectAction ---
template <ImplementsStationSelectHandler Handler>
void StationSelectAction<Handler>::Update(Point, TileIndex tile) { this->cur_tile = tile; }

template <ImplementsStationSelectHandler Handler>
bool StationSelectAction<Handler>::HandleMousePress() { return true; }

template <ImplementsStationSelectHandler Handler>
void StationSelectAction<Handler>::HandleMouseRelease() {
    // TODO station sign click
    if (!IsValidTile(this->cur_tile)) return;
    this->selected_station = INVALID_STATION;
    if (IsTileType(this->cur_tile, MP_STATION)) {
        auto st = Station::GetByTile(this->cur_tile);
        if (st) this->selected_station = st->index;
    }
    this->handler.SelectStationToJoin(this->selected_station);
}

template <ImplementsStationSelectHandler Handler>
ToolGUIInfo StationSelectAction<Handler>::GetGUIInfo() {
    if (!IsValidTile(this->cur_tile)) return {};
    HighlightMap hlmap;
    hlmap.Add(this->cur_tile, ObjectTileHighlight::make_border(CM_PALETTE_TINT_BLUE, ZoningBorder::FULL));
    BuildInfoOverlayData data;
    Station *st = IsTileType(this->cur_tile, MP_STATION) ? Station::GetByTile(this->cur_tile) : nullptr;
    if (st) {
        SetDParam(0, st->index);
        data.emplace_back(0, PAL_NONE, GetString(CM_STR_BULID_INFO_OVERLAY_JOIN_STATION));
    } else {
        data.emplace_back(0, PAL_NONE, GetString(CM_STR_BULID_INFO_OVERLAY_NEW_STATION));
    }
    return {hlmap, data, {}};
}

template <ImplementsStationSelectHandler Handler>
void StationSelectAction<Handler>::OnStationRemoved(const Station *station) {
    if (this->selected_station == station->index) this->selected_station = INVALID_STATION;
}

// --- StationBuildTool ---

StationID StationBuildTool::station_to_join = INVALID_STATION;
StationID StationBuildTool::current_selected_station = INVALID_STATION;
std::optional<ObjectHighlight> StationBuildTool::active_highlight = std::nullopt;

TileArea GetCommandArea(const up<Command> &cmd) {
    if (auto rail_cmd = dynamic_cast<cmd::BuildRailStation *>(cmd.get())) {
        auto w = rail_cmd->numtracks;
        auto h = rail_cmd->plat_len;
        if (!rail_cmd->axis) Swap(w, h);
        return {rail_cmd->tile_org, w, h};
    } else if (auto road_cmd = dynamic_cast<cmd::BuildRoadStop *>(cmd.get())) {
        return {road_cmd->tile, road_cmd->width, road_cmd->length};
    } else if (auto dock_cmd = dynamic_cast<cmd::BuildDock *>(cmd.get())) {
        DiagDirection dir = GetInclinedSlopeDirection(GetTileSlope(dock_cmd->tile));
        TileIndex tile_to = (dir != INVALID_DIAGDIR ? TileAddByDiagDir(dock_cmd->tile, ReverseDiagDir(dir)) : dock_cmd->tile);
        return {dock_cmd->tile, tile_to};
    } else if (auto airport_cmd = dynamic_cast<cmd::BuildAirport *>(cmd.get())) {
        const AirportSpec *as = AirportSpec::Get(airport_cmd->airport_type);
        return {airport_cmd->tile, as->size_x, as->size_y};
    }
    NOT_REACHED();
}

template<typename Thandler, typename Tcallback, typename Targ>
bool StationBuildTool::ExecuteBuildCommand(Thandler *handler, Tcallback callback, Targ arg) {
    if (UseImprovedStationJoin()) {
        auto cmd = handler->GetCommand(arg, StationBuildTool::station_to_join);
        StationBuildTool::active_highlight = std::nullopt;
        return cmd ? cmd->post(callback) : false;
    }

    // Vanilla joining behaviour
    auto cmd = handler->GetCommand(arg, INVALID_STATION);
    auto proc = [cmd=sp<Command>{std::move(cmd)}, callback](bool test, StationID to_join) -> bool {
        StationBuildTool::station_to_join = to_join;
        if (!cmd) return false;
        auto station_cmd = dynamic_cast<StationBuildCommand *>(cmd.get());
        if (station_cmd == nullptr) return false;
        station_cmd->station_to_join = to_join;
        if (test) {
            return cmd->test().Succeeded();
        } else {
            StationBuildTool::active_highlight = std::nullopt;
            return cmd->post(callback);
        }
    };

    auto ohl = handler->GetObjectHighlight(arg);
    if (!ohl.has_value()) return false;
    StationBuildTool::active_highlight = ohl;
    auto area = ohl->GetArea();
    if (!area.has_value()) return false;
    ShowSelectStationIfNeeded(area.value(), proc);
    return true;
}


// --- RailStationBuildTool ---

up<Command> RailStationBuildTool::RemoveHandler::GetCommand(TileArea area) {
    auto cmd = make_up<cmd::RemoveFromRailStation>(
        area.tile,
        area.CMGetEndTile(),
        !citymania::_fn_mod
    );
    cmd->with_error(STR_ERROR_CAN_T_REMOVE_PART_OF_STATION);
    return cmd;
}

bool RailStationBuildTool::RemoveHandler::Execute(TileArea area) {
    auto cmd = this->GetCommand(area);
    return cmd->post(&CcPlaySound_CONSTRUCTION_RAIL);
}

up<Command> RailStationBuildTool::SizedPlacementHandler::GetCommand(TileIndex tile, StationID to_join) {
    // TODO mostly same as DragNDropPlacement
    auto cmd = make_up<cmd::BuildRailStation>(
        tile,
        _cur_railtype,
        _railstation.orientation,
        _settings_client.gui.station_numtracks,
        _settings_client.gui.station_platlength,
        _railstation.station_class,
        _railstation.station_type,
        to_join,
        true
    );
    cmd->with_error(STR_ERROR_CAN_T_BUILD_RAILROAD_STATION);
    return cmd;
}

bool RailStationBuildTool::SizedPlacementHandler::Execute(TileIndex tile) {
    return this->tool.ExecuteBuildCommand(this, &CcStation, tile);
}

up<Command> RailStationBuildTool::DragNDropPlacementHandler::GetCommand(TileArea area, StationID to_join) {
    uint numtracks = area.w;
    uint platlength = area.h;

    if (_railstation.orientation == AXIS_X) Swap(numtracks, platlength);

    auto cmd = make_up<cmd::BuildRailStation>(
        area.tile,
        _cur_railtype,
        _railstation.orientation,
        numtracks,
        platlength,
        _railstation.station_class,
        _railstation.station_type,
        to_join,
        true
    );
    cmd->with_error(STR_ERROR_CAN_T_BUILD_RAILROAD_STATION);
    return cmd;
}

bool RailStationBuildTool::DragNDropPlacementHandler::Execute(TileArea area) {
    return this->tool.ExecuteBuildCommand(this, &CcStation, area);
}

std::optional<ObjectHighlight> RailStationBuildTool::DragNDropPlacementHandler::GetObjectHighlight(TileArea area) {
    return this->tool.GetStationObjectHighlight(area.tile, area.CMGetEndTile());
}

std::optional<ObjectHighlight> RailStationBuildTool::SizedPlacementHandler::GetObjectHighlight(TileIndex tile) {
    return this->tool.GetStationObjectHighlight(tile, INVALID_TILE);
}

RailStationBuildTool::RailStationBuildTool() : mode(Mode::SIZED) {
    this->action = make_up<SizedPlacementAction<SizedPlacementHandler>>(SizedPlacementHandler(*this));
}

void RailStationBuildTool::Update(Point pt, TileIndex tile) {
    Mode new_mode;
    if (_remove_button_clicked) {
        new_mode = Mode::REMOVE;
    } else if (citymania::UseImprovedStationJoin() && _fn_mod) {
        new_mode = Mode::SELECT;
    } else if (_settings_client.gui.station_dragdrop) {
        new_mode = Mode::DRAGDROP;
    } else {
        new_mode = Mode::SIZED;
    }
    if (new_mode != this->mode) {
        switch (new_mode) {
            case Mode::REMOVE:
                this->action = make_up<RemoveAction<RailStationBuildTool::RemoveHandler>>(*this);
                break;
            case Mode::SELECT:
                this->action = make_up<StationSelectAction<StationBuildTool::StationSelectHandler>>(*this);
                break;
            case Mode::DRAGDROP:
                this->action = make_up<DragNDropPlacementAction<RailStationBuildTool::DragNDropPlacementHandler>>(*this);
                break;
            case Mode::SIZED:
                this->action = make_up<SizedPlacementAction<RailStationBuildTool::SizedPlacementHandler>>(*this);
                break;
            default:
                NOT_REACHED();
        }
        this->mode = new_mode;
    }
    this->action->Update(pt, tile);
}

std::optional<ObjectHighlight> RailStationBuildTool::GetStationObjectHighlight(TileIndex start_tile, TileIndex end_tile) const {
    assert(IsValidTile(start_tile));
    assert(!IsValidTile(end_tile) || (TileX(start_tile) <= TileX(end_tile) && TileY(start_tile) <= TileY(end_tile)));
    if (!IsValidTile(end_tile)) {
        // Sized placement mode
        if (_railstation.orientation == AXIS_X)
            end_tile = TILE_ADDXY(start_tile, _settings_client.gui.station_platlength - 1, _settings_client.gui.station_numtracks - 1);
        else
            end_tile = TILE_ADDXY(start_tile, _settings_client.gui.station_numtracks - 1, _settings_client.gui.station_platlength - 1);
    } else {

    }
    return ObjectHighlight::make_rail_station(start_tile, end_tile, _railstation.orientation);
}

CursorID RailStationBuildTool::GetCursor() { return SPR_CURSOR_RAIL_STATION; }

// --- RoadStopBuildTool Handler Implementations ---

up<Command> RoadStopBuildTool::RemoveHandler::GetCommand(TileArea area) {
    auto cmd = make_up<cmd::RemoveRoadStop>(
        area.tile,
        area.w,
        area.h,
        this->tool.stop_type,
        _fn_mod
    );
    auto rti = GetRoadTypeInfo(_cur_roadtype);
    cmd->with_error(rti->strings.err_remove_station[this->tool.stop_type]);
    return cmd;
}

bool RoadStopBuildTool::RemoveHandler::Execute(TileArea area) {
    auto cmd = this->GetCommand(area);
    return cmd->post(&CcPlaySound_CONSTRUCTION_OTHER);
}

up<Command> RoadStopBuildTool::DragNDropPlacementHandler::GetCommand(TileArea area, StationID to_join) {
    DiagDirection ddir = this->tool.ddir;
    bool drive_through = this->tool.ddir >= DIAGDIR_END;
    if (drive_through) ddir = static_cast<DiagDirection>(this->tool.ddir - DIAGDIR_END); // Adjust picker result to actual direction.

    auto res = make_up<cmd::BuildRoadStop>(
        area.tile,
        area.w,
        area.h,
        this->tool.stop_type,
        drive_through,
        ddir,
        _cur_roadtype,
        _roadstop_gui_settings.roadstop_class,
        _roadstop_gui_settings.roadstop_type,
        to_join,
        true
    );

    return res;
}

bool RoadStopBuildTool::DragNDropPlacementHandler::Execute(TileArea area) {
    return this->tool.ExecuteBuildCommand(this, &CcRoadStop, area);
}

std::optional<ObjectHighlight> RoadStopBuildTool::DragNDropPlacementHandler::GetObjectHighlight(TileArea area) {
    return ObjectHighlight::make_road_stop(
        area.tile,
        area.CMGetEndTile(),
        _cur_roadtype,
        this->tool.ddir,
        this->tool.stop_type == ROADSTOP_TRUCK,
        _roadstop_gui_settings.roadstop_class,
        _roadstop_gui_settings.roadstop_type
    );
}

// --- RoadStopBuildTool Implementation ---

RoadStopBuildTool::RoadStopBuildTool(RoadStopType stop_type) : mode(Mode::DRAGDROP), stop_type(stop_type)
{
    this->action = make_up<DragNDropPlacementAction<RoadStopBuildTool::DragNDropPlacementHandler>>(*this);
}

void RoadStopBuildTool::Update(Point pt, TileIndex tile) {
    Mode new_mode;
    if (_remove_button_clicked) {
        new_mode = Mode::REMOVE;
    } else if (UseImprovedStationJoin() && _fn_mod) {
        new_mode = Mode::SELECT;
    } else {
        new_mode = Mode::DRAGDROP;
    }

    if (new_mode != this->mode) {
        switch (new_mode) {
            case Mode::REMOVE:
                this->action = make_up<RemoveAction<RoadStopBuildTool::RemoveHandler>>(*this);
                break;
            case Mode::SELECT:
                this->action = make_up<StationSelectAction<StationBuildTool::StationSelectHandler>>(*this);
                break;
            case Mode::DRAGDROP:
                this->action = make_up<DragNDropPlacementAction<RoadStopBuildTool::DragNDropPlacementHandler>>(*this);
                break;
        }
        this->mode = new_mode;
    }
    this->action->Update(pt, tile);

    this->ddir = DIAGDIR_NE;
    auto area = this->action->GetArea();
    if (pt.x != -1 && this->mode == Mode::DRAGDROP && area.has_value()) {
        auto ddir = _roadstop_gui_settings.orientation;

        if (ddir >= DIAGDIR_END && ddir < STATIONDIR_AUTO) {
            // When placed on road autorotate anyway
            if (ddir == STATIONDIR_X) {
                if (!CheckDriveThroughRoadStopDirection(area.value(), ROAD_X))
                    ddir = STATIONDIR_Y;
            } else {
                if (!CheckDriveThroughRoadStopDirection(area.value(), ROAD_Y))
                    ddir = STATIONDIR_X;
            }
        } else if (ddir == STATIONDIR_AUTO) {
            ddir = AddAutodetectionRotation(AutodetectRoadObjectDirection(tile, pt, _cur_roadtype));
        } else if (ddir == STATIONDIR_AUTO_XY) {
            ddir = AddAutodetectionRotation(AutodetectDriveThroughRoadStopDirection(area.value(), pt, _cur_roadtype));
        }
        this->ddir = ddir;
    }
}

CursorID RoadStopBuildTool::GetCursor() {
    return this->stop_type == ROADSTOP_TRUCK ? SPR_CURSOR_TRUCK_STATION : SPR_CURSOR_BUS_STATION;
}

// --- DockBuildTool Handler Implementations ---

// RemoveHandler
up<Command> DockBuildTool::RemoveHandler::GetCommand(TileArea area) {
    // TODO: Implement dock removal command if available
    return nullptr;
}

bool DockBuildTool::RemoveHandler::Execute(TileArea area) {
    // TODO: Implement dock removal execution if available
    return false;
}

// SizedPlacementHandler
up<Command> DockBuildTool::SizedPlacementHandler::GetCommand(TileIndex tile, StationID to_join) {
    return make_up<cmd::BuildDock>(
        tile,
        to_join,
        true
    );
}

bool DockBuildTool::SizedPlacementHandler::Execute(TileIndex tile) {
    return this->tool.ExecuteBuildCommand(this, &CcBuildDocks, tile);
}

std::optional<ObjectHighlight> DockBuildTool::SizedPlacementHandler::GetObjectHighlight(TileIndex tile) {
    return ObjectHighlight::make_dock(tile, this->tool.ddir);
}

// --- DockBuildTool Implementation ---

DockBuildTool::DockBuildTool() : mode(Mode::SIZED) {
    this->action = make_up<SizedPlacementAction<DockBuildTool::SizedPlacementHandler>>(*this);
}

void DockBuildTool::Update(Point pt, TileIndex tile) {
    Mode new_mode;
    if (_remove_button_clicked) {
        new_mode = Mode::REMOVE;
    } else if (citymania::UseImprovedStationJoin() && _fn_mod) {
        new_mode = Mode::SELECT;
    } else {
        new_mode = Mode::SIZED;
    }
    if (new_mode != this->mode) {
        switch (new_mode) {
            case Mode::REMOVE:
                this->action = make_up<RemoveAction<DockBuildTool::RemoveHandler>>(*this);
                break;
            case Mode::SELECT:
                this->action = make_up<StationSelectAction<DockBuildTool::StationSelectHandler>>(*this);
                break;
            case Mode::SIZED:
                this->action = make_up<SizedPlacementAction<DockBuildTool::SizedPlacementHandler>>(*this);
                break;
            default:
                NOT_REACHED();
        }
        this->mode = new_mode;
    }
    this->action->Update(pt, tile);
    this->ddir = DIAGDIR_SE;
    if (pt.x != -1 && this->mode == Mode::SIZED) {
        auto slope_dir = GetInclinedSlopeDirection(GetTileSlope(tile));
        if (slope_dir != INVALID_DIAGDIR)
            this->ddir = ReverseDiagDir(slope_dir);
    }
 }

CursorID DockBuildTool::GetCursor() {
    return SPR_CURSOR_DOCK;
}

// --- AirportBuildTool Handler Implementations ---

// RemoveHandler
up<Command> AirportBuildTool::RemoveHandler::GetCommand(TileArea area) {
    // TODO: Implement aiport removal command if available
    return nullptr;
}

bool AirportBuildTool::RemoveHandler::Execute(TileArea area) {
    // TODO: Implement airport removal execution if available
    return false;
}

// SizedPlacementHandler
up<Command> AirportBuildTool::SizedPlacementHandler::GetCommand(TileIndex tile, StationID to_join) {
    // STR_ERROR_CAN_T_BUILD_AIRPORT_HERE,
    byte airport_type = AirportClass::Get(_selected_airport_class)->GetSpec(_selected_airport_index)->GetIndex();
    byte layout = _selected_airport_layout;
    return make_up<cmd::BuildAirport>(
        tile,
        airport_type,
        layout,
        StationBuildTool::station_to_join,
        true
    );
}

bool AirportBuildTool::SizedPlacementHandler::Execute(TileIndex tile) {
    this->tool.ExecuteBuildCommand(this, &CcBuildAirport, tile);
    return true;
}

std::optional<ObjectHighlight> AirportBuildTool::SizedPlacementHandler::GetObjectHighlight(TileIndex tile) {
    byte airport_type = AirportClass::Get(_selected_airport_class)->GetSpec(_selected_airport_index)->GetIndex();
    byte layout = _selected_airport_layout;
    return ObjectHighlight::make_airport(tile, airport_type, layout);
}

std::pair<StationCoverageType, uint> AirportBuildTool::SizedPlacementHandler::GetCatchmentParams() {
    auto rad = AirportClass::Get(_selected_airport_class)->GetSpec(_selected_airport_index)->catchment;
    return {SCT_ALL, rad};
}


// --- AirportBuildTool Implementation ---

AirportBuildTool::AirportBuildTool() : mode(Mode::SIZED) {
    this->action = make_up<SizedPlacementAction<AirportBuildTool::SizedPlacementHandler>>(*this);
}

void AirportBuildTool::Update(Point pt, TileIndex tile) {
    Mode new_mode;
    if (_remove_button_clicked) {
        new_mode = Mode::REMOVE;
    } else if (citymania::UseImprovedStationJoin() && _fn_mod) {
        new_mode = Mode::SELECT;
    } else {
        new_mode = Mode::SIZED;
    }
    if (new_mode != this->mode) {
        switch (new_mode) {
            case Mode::REMOVE:
                this->action = make_up<RemoveAction<AirportBuildTool::RemoveHandler>>(*this);
                break;
            case Mode::SELECT:
                this->action = make_up<StationSelectAction<AirportBuildTool::StationSelectHandler>>(*this);
                break;
            case Mode::SIZED:
                this->action = make_up<SizedPlacementAction<AirportBuildTool::SizedPlacementHandler>>(*this);
                break;
            default:
                NOT_REACHED();
        }
        this->mode = new_mode;
    }
    this->action->Update(pt, tile);
}

CursorID AirportBuildTool::GetCursor() {
    return SPR_CURSOR_AIRPORT;
}

// --- Explicit template instantiations for handlers ---
template class StationSelectAction<StationBuildTool::StationSelectHandler>;

template class DragNDropPlacementAction<RailStationBuildTool::DragNDropPlacementHandler>;
template class RemoveAction<RailStationBuildTool::RemoveHandler>;
template class SizedPlacementAction<RailStationBuildTool::SizedPlacementHandler>;

template class RemoveAction<RoadStopBuildTool::RemoveHandler>;
template class DragNDropPlacementAction<RoadStopBuildTool::DragNDropPlacementHandler>;

template class RemoveAction<DockBuildTool::RemoveHandler>;
template class SizedPlacementAction<DockBuildTool::SizedPlacementHandler>;


} // namespace citymania
