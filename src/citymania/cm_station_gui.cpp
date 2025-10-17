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
#include "../strings_func.h"  // GetString
#include "../tilehighlight_type.h"
#include "../town_map.h"
#include "../town.h"
#include "../viewport_func.h"
#include "../viewport_kdtree.h"
#include "../vehicle_base.h"
#include "../window_func.h"  // SetWindowDirty
#include "../window_gui.h"
#include "../zoom_type.h"
#include "../zoom_func.h"
#include "cm_type.hpp"
#include "generated/cm_gen_commands.hpp"

#include <cassert>
#include <complex>
#include <cstdio>
#include <optional>
#include <sstream>
#include <unordered_set>
#include <variant>

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

extern StationPickerSelection _station_gui; ///< Settings of the station picker.
extern RoadStopPickerSelection _roadstop_gui;

extern AirportClassID _selected_airport_class; ///< the currently visible airport class
extern int _selected_airport_index;            ///< the index of the selected airport in the current class or -1
extern uint8_t _selected_airport_layout;          ///< selected airport layout number.

namespace citymania {

// Copy of GetOrderDistance but returning both squared dist and manhattan
std::pair<uint, uint> GetOrderDistances(VehicleOrderID prev, VehicleOrderID cur, const Vehicle *v, int conditional_depth)
{
    assert(v->orders != nullptr);
    const OrderList &orderlist = *v->orders;
    auto orders = orderlist.GetOrders();

    if (orders[cur].IsType(OT_CONDITIONAL)) {
        if (conditional_depth > v->GetNumOrders()) return {0, 0};

        conditional_depth++;

        auto dist1 = GetOrderDistances(prev, orders[cur].GetConditionSkipToOrder(), v, conditional_depth);
        auto dist2 = GetOrderDistances(prev, orderlist.GetNext(cur), v, conditional_depth);
        return {std::max(dist1.first, dist2.first), std::max(dist1.second, dist2.second)};
    }

    TileIndex prev_tile = orders[prev].GetLocation(v, true);
    TileIndex cur_tile = orders[cur].GetLocation(v, true);
    if (prev_tile == INVALID_TILE || cur_tile == INVALID_TILE) return {0, 0};
    return {DistanceSquare(prev_tile, cur_tile), DistanceManhattan(prev_tile, cur_tile)};
}

bool UseImprovedStationJoin() {
    return _settings_client.gui.cm_use_improved_station_join && _settings_game.station.distant_join_stations;
}

namespace StationAction {
    struct Create {};
    struct Join { StationID station; };
    struct Picker {};

    using Mode = std::variant<Create, Join, Picker>;
};

StationAction::Mode _station_action = StationAction::Create{};

static const int MAX_TILE_EXTENT_LEFT   = ZOOM_BASE * TILE_PIXELS;                     ///< Maximum left   extent of tile relative to north corner.
static const int MAX_TILE_EXTENT_RIGHT  = ZOOM_BASE * TILE_PIXELS;                     ///< Maximum right  extent of tile relative to north corner.
static const int MAX_TILE_EXTENT_TOP    = ZOOM_BASE * MAX_BUILDING_PIXELS;             ///< Maximum top    extent of tile relative to north corner (not considering bridges).
static const int MAX_TILE_EXTENT_BOTTOM = ZOOM_BASE * (TILE_PIXELS + 2 * TILE_HEIGHT); ///< Maximum bottom extent of tile relative to north corner (worst case: #SLOPE_STEEP_N).

void MarkTileAreaDirty(const TileArea &ta) {
    if (ta.tile == INVALID_TILE) return;
    auto x = TileX(ta.tile);
    auto y = TileY(ta.tile);
    // TODO check that tile height is used correctly wrt whole area
    Point p1 = RemapCoords(x * TILE_SIZE, y * TILE_SIZE, TileHeight(ta.tile) * TILE_HEIGHT);
    Point p2 = RemapCoords((x + ta.w)  * TILE_SIZE, (y + ta.h) * TILE_SIZE, TileHeight(TileXY(x + ta.w - 1, y + ta.h - 1)) * TILE_HEIGHT);
    Point p3 = RemapCoords((x + ta.w)  * TILE_SIZE, y * TILE_SIZE, TileHeight(TileXY(x + ta.w - 1, y)) * TILE_HEIGHT);
    Point p4 = RemapCoords(x * TILE_SIZE, (y + ta.h) * TILE_SIZE, TileHeight(TileXY(x, y + ta.h - 1)) * TILE_HEIGHT);
    MarkAllViewportsDirty(
            p3.x - MAX_TILE_EXTENT_LEFT,
            p1.y - MAX_TILE_EXTENT_TOP,
            p4.x + MAX_TILE_EXTENT_RIGHT,
            p2.y + MAX_TILE_EXTENT_BOTTOM);
}

// static void MarkCoverageAreaDirty(const Station *station) {
//     MarkTileAreaDirty(station->catchment_tiles);
// }

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
        auto st = Station::Get(std::get<StationID>(item.id));
        if (st->IsInUse()) return;
        if (_local_company != st->owner) return;
        if (CheckClickOnViewportSign(*vp, x, y, &st->sign)) last_st = st;
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
//     command.station_to_join = StationID::Invalid();

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

//  ---- New tools code

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
//     if (this->selected_station_to_join != StationID::Invalid()) {
//         this->station_to_join = this->selected_station_to_join;
//         return;
//     }

//     if (!IsValidTile(this->type->cur_tile)) return;
//     this->station_to_join = StationID::Invalid();
//     auto area = this->type->GetArea(false);
//     area.Expand(1);
//     area.ClampToMap();
//     for (auto tile : area) {
//         if (IsTileType(tile, MP_STATION) && GetTileOwner(tile) == _local_company) {
//             Station *st = Station::GetByTile(tile);
//             if (st == nullptr || st->index == this->station_to_join) continue;
//             if (this->station_to_join != StationID::Invalid()) {
//                 this->station_to_join = StationID::Invalid();
//                 this->palette = CM_PALETTE_TINT_YELLOW;
//                 break;
//             }
//             this->station_to_join = st->index;
//             // TODO check for command to return multiple? but also check each to
//             // see if they can be built
//             // if (this->GetCommand(true, st->index)->test().Succeeded()) {
//             //     if (this->station_to_join != StationID::Invalid()) {
//             //         this->station_to_join = StationID::Invalid();
//             //         this->palette = CM_PALETTE_TINT_YELLOW;
//             //         break;
//             //     } else this->station_to_join = st->index;
//             // }
//         }
//     }
//     if (this->station_to_join == StationID::Invalid() && !this->GetCommand(true, NEW_STATION)->test().Succeeded())
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
//     if (this->station_to_join == station->index) this->station_to_join = StationID::Invalid();
//     if (this->selected_station_to_join == station->index) this->station_to_join = StationID::Invalid();
// }

// StationPreview::StationPreview(sp<PreviewStationType> type)
//     :StationPreviewBase{type}
// {
//     auto seconds_since_selected = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - _station_to_join_selected).count();
//     if (seconds_since_selected < 30) this->station_to_join = _station_to_join;
//     else this->station_to_join = StationID::Invalid();
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
//     if (this->station_to_join == station->index) this->station_to_join = StationID::Invalid();
// }

// Non-tool station highlight management (coverage area and picker selection)

enum class StationHighlightMode {
    None,
    Picker,
    Coverage
};

std::optional<ObjectHighlight> _active_highlight_object = std::nullopt;
StationID _selected_station = StationID::Invalid();
StationHighlightMode _station_highlight_mode = StationHighlightMode::None;

void SetSelectedStationToJoin(StationID station_id) {
    _selected_station = station_id;
    UpdateActiveTool();
}

void ResetSelectedStationToJoin() {
    _station_highlight_mode = StationHighlightMode::None;
    UpdateActiveTool();
}

void SetHighlightCoverageStation(Station *station, bool sel) {
    if (_station_highlight_mode == StationHighlightMode::Picker) return;
    SetWindowDirty(WC_STATION_VIEW, _selected_station);
    if (station == nullptr || !sel) {
        _station_highlight_mode = StationHighlightMode::None;
    } else {
        _selected_station = station->index;
        _station_highlight_mode = StationHighlightMode::Coverage;
    }
    SetWindowDirty(WC_STATION_VIEW, _selected_station);
    UpdateActiveTool();
}

static void ResetHighlightCoverageStation() {
    SetHighlightCoverageStation(nullptr, false);
}

bool IsHighlightCoverageStation(const Station *station) {
    if (_station_highlight_mode != StationHighlightMode::Coverage) return false;
    if (station == nullptr) return false;
    return station->index == _selected_station;
}

void OnStationRemoved(const Station *station) {
    // if (_last_built_station == station) _last_built_station = nullptr;
    if (auto mode = std::get_if<StationAction::Join>(&_station_action); mode && mode->station == station->index) {
        _station_action = StationAction::Create{};
        UpdateActiveTool();
    }
    if (_selected_station == station->index) {
        _selected_station = StationID::Invalid();
        if (_station_highlight_mode == StationHighlightMode::Coverage) {
            _station_highlight_mode = StationHighlightMode::None;
            SetWindowDirty(WC_STATION_VIEW, _selected_station);
        }
        UpdateActiveTool();
    }
    // TODO?
    // if (GetActiveTool() != nullptr) GetActiveTool()->OnStationRemoved(station);
}

static void SetActiveHighlightObject(std::optional<ObjectHighlight> &ohl) {
    _active_highlight_object = ohl;
    if (_active_highlight_object.has_value()) _active_highlight_object->UpdateTiles();
    if (_station_highlight_mode == StationHighlightMode::Coverage)
        SetWindowDirty(WC_STATION_VIEW, _selected_station);
    _station_highlight_mode = StationHighlightMode::Picker;
}

void AbortStationPlacement() {
    // TODO is it necessary?
    // SetHighlightStationToJoin(station=nullptr, with_area=false);
}

bool HasSelectedStationHighlight() {
    return _station_highlight_mode != StationHighlightMode::None;
}

static HighlightMap PrepareHighilightMap(Station *st_join, std::optional<ObjectHighlight> ohl, SpriteID pal, bool show_join_area, bool show_coverage, uint rad) {
    bool add_current = true;  // FIXME

    auto hlmap = ohl.has_value() ? ohl->GetHighlightMap(pal) : HighlightMap{};
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

    auto area = ohl.has_value() ? ohl->GetArea() : std::nullopt;
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

ToolGUIInfo GetSelectedStationGUIInfo() {
    if (_station_highlight_mode == StationHighlightMode::None) return {};
    auto st = Station::GetIfValid(_selected_station);

    auto hlmap = PrepareHighilightMap(
        st,
        _station_highlight_mode == StationHighlightMode::Picker ? _active_highlight_object : std::nullopt,
        CM_PALETTE_TINT_WHITE,
        false,
        _station_highlight_mode == StationHighlightMode::Coverage,
        0
    );
    return {hlmap, {}, {}};
}

// --- Action base class ---

void Action::OnStationRemoved(const Station *) {}

// --- RemoveAction ---

void RemoveAction::Update(Point, TileIndex tile) {
    this->cur_tile = tile;
}

bool RemoveAction::HandleMousePress() {
    if (!IsValidTile(this->cur_tile)) return false;
    this->start_tile = this->cur_tile;
    return true;
}

void RemoveAction::HandleMouseRelease() {
    auto area = this->GetArea();
    if (!area.has_value()) return;
    this->Execute(area.value());
    this->start_tile = INVALID_TILE;
}

std::optional<TileArea> RemoveAction::GetArea() const {
    if (!IsValidTile(this->cur_tile)) return std::nullopt;
    if (!IsValidTile(this->start_tile)) return TileArea{this->cur_tile, this->cur_tile};
    return TileArea{this->start_tile, this->cur_tile};
}

ToolGUIInfo RemoveAction::GetGUIInfo() {
    HighlightMap hlmap;
    BuildInfoOverlayData data;
    auto area = this->GetArea();
    CommandCost cost;
    if (area.has_value()) {
        hlmap.AddTileAreaWithBorder(area.value(), CM_PALETTE_TINT_RED_DEEP);
        auto cmd = this->GetCommand(area.value());
        if (cmd) cost = cmd->test();
    }
    return {hlmap, data, cost};
}

void RemoveAction::OnStationRemoved(const Station *) {}

// --- PlacementAction ---

ToolGUIInfo PlacementAction::PrepareGUIInfo(std::optional<ObjectHighlight> ohl, up<Command> cmd, StationCoverageType sct, uint rad) {
    if (!cmd || !ohl.has_value()) return {};
    ohl.value().UpdateTiles();
    auto palette = CM_PALETTE_TINT_WHITE;
    auto area = ohl.value().GetArea();

    auto cost = cmd->test();
    if (std::holds_alternative<StationAction::Picker>(_station_action)) {
        palette = CM_PALETTE_TINT_YELLOW;
    } else {
        palette = cost.Succeeded() ? CM_PALETTE_TINT_WHITE : CM_PALETTE_TINT_RED_DEEP;
    }

    bool show_coverage = _settings_client.gui.station_show_coverage;

    Station *to_join = nullptr;
    if (auto mode = std::get_if<StationAction::Join>(&_station_action))
        to_join = Station::GetIfValid(mode->station);
    auto hlmap = PrepareHighilightMap(
        to_join,
        ohl.value(),
        palette,
        true,
        show_coverage,
        rad
    );

    // Prepare build overlay

    BuildInfoOverlayData data;

    if (auto mode = std::get_if<StationAction::Join>(&_station_action)) {
        data.emplace_back(0, PAL_NONE, GetString(CM_STR_BULID_INFO_OVERLAY_JOIN_STATION, mode->station));
    } else {
        data.emplace_back(0, PAL_NONE, GetString(CM_STR_BULID_INFO_OVERLAY_NEW_STATION));
    }

    if (area.has_value()) {
        // Add supplied cargo information
        // TODO can we use rad_area since we already have it?
        auto production = citymania::GetProductionAroundTiles(area->tile, area->w, area->h, rad);
        bool has_header = false;
        for (CargoType i = 0; i < NUM_CARGO; i++) {
            if (production[i] == 0) continue;

            switch (sct) {
                case SCT_PASSENGERS_ONLY: if (!IsCargoInClass(i, CargoClass::Passengers)) continue; break;
                case SCT_NON_PASSENGERS_ONLY: if (IsCargoInClass(i, CargoClass::Passengers)) continue; break;
                case SCT_ALL: break;
                default: NOT_REACHED();
            }

            const CargoSpec *cs = CargoSpec::Get(i);
            if (cs == nullptr) continue;

            if (!has_header) {
                data.emplace_back(0, PAL_NONE, GetString(CM_STR_BUILD_INFO_OVERLAY_STATION_SUPPLIES));
                has_header = true;
            }
            data.emplace_back(1, cs->GetCargoIcon(), GetString(CM_STR_BUILD_INFO_OVERLAY_STATION_CARGO, i, production[i] >> 8));
        }

        // Add accepted cargo information
        CargoTypes always_accepted;
        auto cargoes = ::GetAcceptanceAroundTiles(area->tile, area->w, area->h, rad, &always_accepted);
        /* Convert cargo counts to a set of cargo bits, and draw the result. */
        std::vector<std::pair<uint, std::string>> cargostr;
        for (CargoType i = 0; i < NUM_CARGO; i++) {
            switch (sct) {
                case SCT_PASSENGERS_ONLY: if (!IsCargoInClass(i, CargoClass::Passengers)) continue; break;
                case SCT_NON_PASSENGERS_ONLY: if (IsCargoInClass(i, CargoClass::Passengers)) continue; break;
                case SCT_ALL: break;
                default: NOT_REACHED();
            }
            if (cargoes[i] > 0) {
                if(cargoes[i] < 8) {
                    cargostr.emplace_back(2, GetString(CM_STR_BULID_INFO_OVERLAY_ACCEPTS_CARGO_PARTIAL, 1 << i, cargoes[i]));
                } else if (HasBit(always_accepted, i)) {
                    cargostr.emplace_back(1, GetString(CM_STR_BULID_INFO_OVERLAY_ACCEPTS_CARGO_FULL, 1 << i, cargoes[i]));
                } else {
                    cargostr.emplace_back(0, GetString(CM_STR_BULID_INFO_OVERLAY_ACCEPTS_CARGO, 1 << i));
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
        data.emplace_back(0, PAL_NONE, GetString(CM_STR_BULID_INFO_OVERLAY_ACCEPTS, cargoliststr));

        Town *t = ClosestTownFromTile(area->tile, _settings_game.economy.dist_local_authority);
        if (t) {
            auto town_allowed = CheckIfAuthorityAllowsNewStation(area->tile, {}).Succeeded();
            auto rating = t->ratings[_current_company];
            auto dist = DistanceManhattan(t->xy, area->tile);
            StringID zone_id;
            if (dist <= 10) {
                zone_id = CM_STR_BULID_INFO_OVERLAY_TOWN_S_ADS;
            } else if (dist <= 15) {
                zone_id = CM_STR_BULID_INFO_OVERLAY_TOWN_M_ADS;
            } else if (dist <= 20) {
                zone_id = CM_STR_BULID_INFO_OVERLAY_TOWN_L_ADS;
            } else {
                zone_id = CM_STR_BULID_INFO_OVERLAY_TOWN_NO_ADS;
            }
            data.emplace_back(0, PAL_NONE, GetString(
                town_allowed ? CM_STR_BULID_INFO_OVERLAY_TOWN_ALLOWS : CM_STR_BULID_INFO_OVERLAY_TOWN_DENIES,
                t->index,
                rating,
                zone_id
            ));
        } else {
            data.emplace_back(0, PAL_NONE, GetString(CM_STR_BULID_INFO_OVERLAY_TOWN_NONE));
        }

        data.emplace_back(0, PAL_NONE, GetString(CM_STR_BULID_INFO_OVERLAY_STATION_SIZE, area->w, area->h));
    }

    return {hlmap, data, cost};
}

// --- SizedPlacementAction ---

void SizedPlacementAction::Update(Point, TileIndex tile) {
    this->cur_tile = tile;
    if (UseImprovedStationJoin()) return;

    _station_action = StationAction::Create{};

    auto area = this->GetArea();
    if (!area.has_value()) return;
    auto cmdptr = this->GetCommand(tile, StationID::Invalid());
    auto cmd = dynamic_cast<cmd::BuildRailStation *>(cmdptr.get());
    if (cmd == nullptr) return;

    if (!_settings_game.station.distant_join_stations && _fn_mod) return;

    area->Expand(1);
    area->ClampToMap();
    StationID to_join = StationID::Invalid();
    bool ambigous_join = false;
    for (auto tile : area.value()) {
        if (IsTileType(tile, MP_STATION) && GetTileOwner(tile) == _local_company) {
            Station *st = Station::GetByTile(tile);
            if (st == nullptr || st->index == to_join) continue;
            if (to_join != StationID::Invalid()) {
                to_join = StationID::Invalid();
                if (_settings_game.station.distant_join_stations)
                    ambigous_join = true;
                break;
            }
            to_join = st->index;
            // TODO check for command to return multiple? but also check each to
            // see if they can be built
            // if (this->GetCommand(true, st->index)->test().Succeeded()) {
            //     if (this->station_to_join != INVALID_STATION) {
            //         this->station_to_join = INVALID_STATION;
            //         this->palette = CM_PALETTE_TINT_YELLOW;
            //         break;
            //     } else this->station_to_join = st->index;
            // }
        }
    }

    if (!_settings_game.station.distant_join_stations) return;

    if (ambigous_join) _station_action = StationAction::Picker{};
    else if (to_join != StationID::Invalid()) _station_action = StationAction::Join{to_join};
    else _station_action = StationAction::Create{};

    // cmd->station_to_join = NEW_STATION;
    // cmd->adjacent = true;

    // if (StationBuildTool::station_to_join == INVALID_STATION && !cmd->test().Succeeded()) {
    //     StationBuildTool::ambigous_join = false;
    // }
}

bool SizedPlacementAction::HandleMousePress() {
    return IsValidTile(this->cur_tile);
}

void SizedPlacementAction::HandleMouseRelease() {
    if (!IsValidTile(this->cur_tile)) return;
    this->Execute(this->cur_tile);
}

ToolGUIInfo SizedPlacementAction::GetGUIInfo() {
    if (!IsValidTile(this->cur_tile)) return {};
    auto [sct, rad] = this->GetCatchmentParams();
    return this->PrepareGUIInfo(
        this->GetObjectHighlight(this->cur_tile),
        this->GetCommand(this->cur_tile, StationID::Invalid()),
        sct,
        rad
    );
}

void SizedPlacementAction::OnStationRemoved(const Station *) {}

// --- DragNDropPlacementAction ---

std::optional<TileArea> DragNDropPlacementAction::GetArea() const {
    // TODO separate common fuctions with RemoveAction into base class
    if (!IsValidTile(this->cur_tile)) return std::nullopt;
    if (!IsValidTile(this->start_tile)) return TileArea{this->cur_tile, this->cur_tile};
    return TileArea{this->start_tile, this->cur_tile};
}

void DragNDropPlacementAction::Update(Point, TileIndex tile) {
    this->cur_tile = tile;
}

bool DragNDropPlacementAction::HandleMousePress() {
    if (!IsValidTile(this->cur_tile)) return false;
    this->start_tile = this->cur_tile;
    return true;
}

void DragNDropPlacementAction::HandleMouseRelease() {
    auto area = this->GetArea();
    if (!area.has_value()) return;
    this->Execute(area.value());
    this->start_tile = INVALID_TILE;
}

ToolGUIInfo DragNDropPlacementAction::GetGUIInfo() {
    auto area = this->GetArea();
    if (!area.has_value()) return {};
    auto ohl = this->GetObjectHighlight(area.value());
    auto [sct, rad] = this->GetCatchmentParams();
    return this->PrepareGUIInfo(
        this->GetObjectHighlight(area.value()),
        this->GetCommand(area.value(), StationID::Invalid()),
        sct,
        rad
    );
}

void DragNDropPlacementAction::OnStationRemoved(const Station *) {}

// --- StationSelectAction ---

void StationSelectAction::Update(Point, TileIndex tile) {
    this->cur_tile = tile;
}

bool StationSelectAction::HandleMousePress() {
    return true;
}

void StationSelectAction::HandleMouseRelease() {
    // TODO station sign click
    if (!IsValidTile(this->cur_tile)) return;
    _station_action = StationAction::Create{};
    if (IsTileType(this->cur_tile, MP_STATION)) {
        auto st = Station::GetByTile(this->cur_tile);
        if (st) _station_action = StationAction::Join{st->index};
    }
}

ToolGUIInfo StationSelectAction::GetGUIInfo() {
    if (!IsValidTile(this->cur_tile)) return {};
    HighlightMap hlmap;
    hlmap.Add(this->cur_tile, ObjectTileHighlight::make_border(CM_PALETTE_TINT_BLUE, ZoningBorder::FULL));
    BuildInfoOverlayData data;
    Station *st = IsTileType(this->cur_tile, MP_STATION) ? Station::GetByTile(this->cur_tile) : nullptr;
    if (st) {
        data.emplace_back(0, PAL_NONE, GetString(CM_STR_BULID_INFO_OVERLAY_JOIN_STATION, st->index));
    } else {
        data.emplace_back(0, PAL_NONE, GetString(CM_STR_BULID_INFO_OVERLAY_NEW_STATION));
    }
    return {hlmap, data, {}};
}

void StationSelectAction::OnStationRemoved(const Station *station) {
    // if (this->selected_station == station->index) this->selected_station = INVALID_STATION;
}

// --- Misc functions ---

TileArea GetCommandArea(const up<Command> &cmd) {
    if (auto rail_cmd = dynamic_cast<cmd::BuildRailStation *>(cmd.get())) {
        auto w = rail_cmd->numtracks;
        auto h = rail_cmd->plat_len;
        if (!rail_cmd->axis) std::swap(w, h);
        return {rail_cmd->tile_org, w, h};
    } else if (auto road_cmd = dynamic_cast<cmd::BuildRoadStop *>(cmd.get())) {
        return {road_cmd->tile, road_cmd->width, road_cmd->length};
    } else if (auto dock_cmd = dynamic_cast<cmd::BuildDock *>(cmd.get())) {
        DiagDirection dir = GetInclinedSlopeDirection(GetTileSlope(dock_cmd->tile));
        TileIndex tile_to = (dir != INVALID_DIAGDIR ? TileAddByDiagDir(dock_cmd->tile, ReverseDiagDir(dir)) : dock_cmd->tile);
        return {dock_cmd->tile, tile_to};
    } else if (auto airport_cmd = dynamic_cast<cmd::BuildAirport *>(cmd.get())) {
        const AirportSpec *as = AirportSpec::Get(airport_cmd->airport_type);
        if (as == nullptr) return {};
        return {airport_cmd->tile, as->size_x, as->size_y};
    }
    NOT_REACHED();
}


StationBuildTool::StationBuildTool() {
    ResetHighlightCoverageStation();
}

extern void ShowSelectStationWindow(TileArea ta, StationPickerCmdProc&& proc);

template<typename Taction, typename Tcallback, typename Targ>
bool ExecuteBuildCommand(Taction *action, Tcallback callback, Targ arg) {
    std::visit(Overload{
        [&](StationAction::Join &a) {
            Debug(misc, 0, "Join to {}", a.station);
            auto cmd = action->GetCommand(arg, a.station);
            return cmd ? cmd->post(callback) : false;
        },
        [&](StationAction::Create &) {
            Debug(misc, 0, "Create new station");
            auto cmd = action->GetCommand(arg, NEW_STATION);
            return cmd ? cmd->post(callback) : false;
        },
        [&](StationAction::Picker &) {
            Debug(misc, 0, "Show picker");
            auto cmd = action->GetCommand(arg, StationID::Invalid());
            auto proc = [cmd=sp<Command>{std::move(cmd)}, callback](bool test, StationID to_join) -> bool {
                if (!cmd) return false;
                auto station_cmd = dynamic_cast<StationBuildCommand *>(cmd.get());
                if (station_cmd == nullptr) return false;
                station_cmd->station_to_join = to_join;
                if (test) {
                    return cmd->test().Succeeded();
                } else {
                    ResetSelectedStationToJoin();
                    return cmd->post(callback);
                }
            };

            auto ohl = action->GetObjectHighlight(arg);
            if (!ohl.has_value()) return false;
            auto area = ohl->GetArea();
            if (!area.has_value()) return false;
            // SetActiveHighlightObject(ohl);
            ShowSelectStationWindow(*area, std::move(proc));
            return true;
        }
    }, _station_action);

    return true;
}


// --- RailStationBuildTool::RemoveAction ---

up<Command> RailStationBuildTool::RemoveAction::GetCommand(TileArea area) {
    auto cmd = make_up<cmd::RemoveFromRailStation>(
        area.tile,
        area.CMGetEndTile(),
        !citymania::_fn_mod
    );
    cmd->with_error(STR_ERROR_CAN_T_REMOVE_PART_OF_STATION);
    return cmd;
}

bool RailStationBuildTool::RemoveAction::Execute(TileArea area) {
    auto cmd = this->GetCommand(area);
    return cmd->post(&CcPlaySound_CONSTRUCTION_RAIL);
}


// --- RailStationBuildTool::SizedPlacementAction ---

std::optional<TileArea> RailStationBuildTool::SizedPlacementAction::GetArea() const {
    if (!IsValidTile(this->cur_tile)) return std::nullopt;
    auto w = _settings_client.gui.station_numtracks;
    auto h = _settings_client.gui.station_platlength;
    if (_station_gui.axis == AXIS_X) std::swap(w, h);
    return TileArea{this->cur_tile, w, h};
}

up<Command> RailStationBuildTool::SizedPlacementAction::GetCommand(TileIndex tile, StationID to_join) {
    // TODO mostly same as DragNDropPlacement
    auto cmd = make_up<cmd::BuildRailStation>(
        tile,
        _cur_railtype,
        _station_gui.axis,
        _settings_client.gui.station_numtracks,
        _settings_client.gui.station_platlength,
        _station_gui.sel_class,
        _station_gui.sel_type,
        to_join,
        true
    );
    cmd->with_error(STR_ERROR_CAN_T_BUILD_RAILROAD_STATION);
    return cmd;
}

bool RailStationBuildTool::SizedPlacementAction::Execute(TileIndex tile) {
    return ExecuteBuildCommand(this, &CcStation, tile);
}

// --- RailStationBuildTool::DragNDropPlacementAction ---

up<Command> RailStationBuildTool::DragNDropPlacementAction::GetCommand(TileArea area, StationID to_join) {
    uint numtracks = area.w;
    uint platlength = area.h;

    if (_station_gui.axis == AXIS_X) std::swap(numtracks, platlength);

    auto cmd = make_up<cmd::BuildRailStation>(
        area.tile,
        _cur_railtype,
        _station_gui.axis,
        numtracks,
        platlength,
        _station_gui.sel_class,
        _station_gui.sel_type,
        to_join,
        true
    );
    cmd->with_error(STR_ERROR_CAN_T_BUILD_RAILROAD_STATION);
    return cmd;
}

bool RailStationBuildTool::DragNDropPlacementAction::Execute(TileArea area) {
    return ExecuteBuildCommand(this, &CcStation, area);
}

std::optional<ObjectHighlight> RailStationBuildTool::DragNDropPlacementAction::GetObjectHighlight(TileArea area) {
    // Debug(misc, 0, "GetObjectHighlight {} {} ", _railstation.station_class, _railstation.station_type);
    return ObjectHighlight::make_rail_station(area.tile, area.CMGetEndTile(), _station_gui.axis, _station_gui.sel_class, _station_gui.sel_type);
}

std::optional<ObjectHighlight> RailStationBuildTool::SizedPlacementAction::GetObjectHighlight(TileIndex tile) {
    TileIndex end_tile;
    if (_station_gui.axis == AXIS_X)
        end_tile = TileAddXY(tile, _settings_client.gui.station_platlength - 1, _settings_client.gui.station_numtracks - 1);
    else
        end_tile = TileAddXY(tile, _settings_client.gui.station_numtracks - 1, _settings_client.gui.station_platlength - 1);
    return ObjectHighlight::make_rail_station(tile, end_tile, _station_gui.axis, _station_gui.sel_class, _station_gui.sel_type);
}

// --- RailStationBuildTool implementation ---

RailStationBuildTool::RailStationBuildTool() : mode(Mode::SIZED) {
    this->action = make_sp<RailStationBuildTool::SizedPlacementAction>();
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
                this->action = make_sp<RailStationBuildTool::RemoveAction>();
                break;
            case Mode::SELECT:
                this->action = make_sp<StationSelectAction>();
                break;
            case Mode::DRAGDROP:
                this->action = make_sp<RailStationBuildTool::DragNDropPlacementAction>();
                break;
            case Mode::SIZED:
                this->action = make_sp<RailStationBuildTool::SizedPlacementAction>();
                break;
            default:
                NOT_REACHED();
        }
        this->mode = new_mode;
    }
    this->action->Update(pt, tile);
}

CursorID RailStationBuildTool::GetCursor() { return SPR_CURSOR_RAIL_STATION; }

// --- RoadStopBuildTool::RemoveAction ---

up<Command> RoadStopBuildTool::RemoveAction::GetCommand(TileArea area) {
    auto cmd = make_up<cmd::RemoveRoadStop>(
        area.tile,
        area.w,
        area.h,
        this->stop_type,
        _fn_mod
    );
    auto rti = GetRoadTypeInfo(_cur_roadtype);
    cmd->with_error(rti->strings.err_remove_station[to_underlying(this->stop_type)]);
    return cmd;
}

bool RoadStopBuildTool::RemoveAction::Execute(TileArea area) {
    auto cmd = this->GetCommand(area);
    return cmd->post(&CcPlaySound_CONSTRUCTION_OTHER);
}

up<Command> RoadStopBuildTool::DragNDropPlacementAction::GetCommand(TileArea area, StationID to_join) {
    DiagDirection ddir = this->ddir;
    bool drive_through = this->ddir >= DIAGDIR_END;
    if (drive_through) ddir = static_cast<DiagDirection>(this->ddir - DIAGDIR_END); // Adjust picker result to actual direction.

    auto res = make_up<cmd::BuildRoadStop>(
        area.tile,
        area.w,
        area.h,
        this->stop_type,
        drive_through,
        ddir,
        this->road_type,
        _roadstop_gui.sel_class,
        _roadstop_gui.sel_type,
        to_join,
        true
    );

    return res;
}

bool RoadStopBuildTool::DragNDropPlacementAction::Execute(TileArea area) {
    return ExecuteBuildCommand(this, &CcRoadStop, area);
}

std::optional<ObjectHighlight> RoadStopBuildTool::DragNDropPlacementAction::GetObjectHighlight(TileArea area) {
    return ObjectHighlight::make_road_stop(
        area.tile,
        area.CMGetEndTile(),
        this->road_type,
        this->ddir,
        this->stop_type == RoadStopType::Truck,
        _roadstop_gui.sel_class,
        _roadstop_gui.sel_type
    );
}

// --- RoadStopBuildTool implementation ---

RoadStopBuildTool::RoadStopBuildTool(RoadStopType stop_type) : mode(Mode::DRAGDROP), stop_type(stop_type)
{
    this->action = make_sp<RoadStopBuildTool::DragNDropPlacementAction>(_cur_roadtype, this->stop_type);
}

void RoadStopBuildTool::DragNDropPlacementAction::Update(Point pt, TileIndex tile) {
    citymania::DragNDropPlacementAction::Update(pt, tile);
    this->ddir = DIAGDIR_NE;
    auto area = this->GetArea();
    if (pt.x != -1 && area.has_value()) {
        auto ddir = _roadstop_gui.orientation;

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
            ddir = AddAutodetectionRotation(AutodetectRoadObjectDirection(tile, pt, this->road_type));
        } else if (ddir == STATIONDIR_AUTO_XY) {
            ddir = AddAutodetectionRotation(AutodetectDriveThroughRoadStopDirection(area.value(), pt, this->road_type));
        }
        this->ddir = ddir;
    }
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
                this->action = make_sp<RoadStopBuildTool::RemoveAction>(this->stop_type);
                break;
            case Mode::SELECT:
                this->action = make_sp<StationSelectAction>();
                break;
            case Mode::DRAGDROP:
                this->action = make_sp<RoadStopBuildTool::DragNDropPlacementAction>(_cur_roadtype, this->stop_type);
                break;
        }
        this->mode = new_mode;
    }
    this->action->Update(pt, tile);
}

CursorID RoadStopBuildTool::GetCursor() {
    return this->stop_type == RoadStopType::Truck ? SPR_CURSOR_TRUCK_STATION : SPR_CURSOR_BUS_STATION;
}

// --- DockBuildTool::RemoveAction ---

up<Command> DockBuildTool::RemoveAction::GetCommand(TileArea area) {
    // TODO: Implement dock removal command if available
    return nullptr;
}

bool DockBuildTool::RemoveAction::Execute(TileArea area) {
    // TODO: Implement dock removal execution if available
    return false;
}

// --- DockBuildTool::SizedPlacementAction ---

std::optional<TileArea> DockBuildTool::SizedPlacementAction::GetArea() const {
    auto ddir = this->GetDirection(this->cur_tile);
    if (!ddir.has_value()) return std::nullopt;
    return TileArea{this->cur_tile, TileAddByDiagDir(this->cur_tile, *ddir)};
}

up<Command> DockBuildTool::SizedPlacementAction::GetCommand(TileIndex tile, StationID to_join) {
    return make_up<cmd::BuildDock>(
        tile,
        to_join,
        true
    );
}


bool DockBuildTool::SizedPlacementAction::Execute(TileIndex tile) {
    return ExecuteBuildCommand(this, &CcBuildDocks, tile);
}

std::optional<ObjectHighlight> DockBuildTool::SizedPlacementAction::GetObjectHighlight(TileIndex tile) {
    return ObjectHighlight::make_dock(tile, this->GetDirection(tile).value_or(DIAGDIR_SE));
}

std::optional<DiagDirection> DockBuildTool::SizedPlacementAction::GetDirection(TileIndex tile) const {
    if (!IsValidTile(tile)) return std::nullopt;
    auto slope_dir = GetInclinedSlopeDirection(GetTileSlope(tile));
    if (slope_dir == INVALID_DIAGDIR) return std::nullopt;
    return ReverseDiagDir(slope_dir);
};


// --- DockBuildTool Implementation ---

DockBuildTool::DockBuildTool() : mode(Mode::SIZED) {
    this->action = make_sp<DockBuildTool::SizedPlacementAction>();
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
                this->action = make_up<DockBuildTool::RemoveAction>();
                break;
            case Mode::SELECT:
                this->action = make_up<StationSelectAction>();
                break;
            case Mode::SIZED:
                this->action = make_up<DockBuildTool::SizedPlacementAction>();
                break;
            default:
                NOT_REACHED();
        }
        this->mode = new_mode;
    }
    this->action->Update(pt, tile);
}

CursorID DockBuildTool::GetCursor() {
    return SPR_CURSOR_DOCK;
}

// --- AirportBuildTool::RemoveAction ---

up<Command> AirportBuildTool::RemoveAction::GetCommand(TileArea area) {
    // TODO: Implement aiport removal command if available
    return nullptr;
}

bool AirportBuildTool::RemoveAction::Execute(TileArea area) {
    // TODO: Implement airport removal execution if available
    return false;
}

// --- AirportBuildTool::SizedPlacementAction ---

std::optional<TileArea> AirportBuildTool::SizedPlacementAction::GetArea() const {
    if (!IsValidTile(this->cur_tile)) return std::nullopt;
    auto as = AirportClass::Get(_selected_airport_class)->GetSpec(_selected_airport_index);
    if (as == nullptr) return std::nullopt;
    return TileArea{this->cur_tile, as->size_x, as->size_y};
}

up<Command> AirportBuildTool::SizedPlacementAction::GetCommand(TileIndex tile, StationID to_join) {
    auto as = AirportClass::Get(_selected_airport_class)->GetSpec(_selected_airport_index);
    if (as == nullptr) return nullptr;
    auto airport_type = as->GetIndex();
    auto layout = _selected_airport_layout;
    auto cmd = make_up<cmd::BuildAirport>(
        tile,
        airport_type,
        layout,
        to_join,
        true
    );
    cmd->with_error(STR_ERROR_CAN_T_BUILD_AIRPORT_HERE);
    return cmd;
}

bool AirportBuildTool::SizedPlacementAction::Execute(TileIndex tile) {
    return ExecuteBuildCommand(this, &CcBuildAirport, tile);
}

std::optional<ObjectHighlight> AirportBuildTool::SizedPlacementAction::GetObjectHighlight(TileIndex tile) {
    auto airport_type = AirportClass::Get(_selected_airport_class)->GetSpec(_selected_airport_index)->GetIndex();
    auto layout = _selected_airport_layout;
    return ObjectHighlight::make_airport(tile, airport_type, layout);
}

std::pair<StationCoverageType, uint> AirportBuildTool::SizedPlacementAction::GetCatchmentParams() {
    auto rad = AirportClass::Get(_selected_airport_class)->GetSpec(_selected_airport_index)->catchment;
    return {SCT_ALL, rad};
}


// --- AirportBuildTool implementation ---

AirportBuildTool::AirportBuildTool() : mode(Mode::SIZED) {
    this->action = make_sp<AirportBuildTool::SizedPlacementAction>();
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
                this->action = make_sp<AirportBuildTool::RemoveAction>();
                break;
            case Mode::SELECT:
                this->action = make_sp<StationSelectAction>();
                break;
            case Mode::SIZED:
                this->action = make_sp<AirportBuildTool::SizedPlacementAction>();
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

} // namespace citymania
