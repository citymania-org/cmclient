#include "../stdafx.h"

#include "highlight.hpp"

#include "../core/math_func.hpp"
#include "../command_func.h"
#include "../house.h"
#include "../industry.h"
#include "../landscape.h"
#include "../town.h"
#include "../town_kdtree.h"
#include "../tilearea_type.h"
#include "../tilehighlight_type.h"
#include "../viewport_func.h"
#include "../zoning.h"

#include <set>

#include "station_ui.hpp"

#include "cm_main.hpp"


/** Enumeration of multi-part foundations */
enum FoundationPart {
    FOUNDATION_PART_NONE     = 0xFF,  ///< Neither foundation nor groundsprite drawn yet.
    FOUNDATION_PART_NORMAL   = 0,     ///< First part (normal foundation or no foundation)
    FOUNDATION_PART_HALFTILE = 1,     ///< Second part (halftile foundation)
    FOUNDATION_PART_END
};
extern void DrawSelectionSprite(SpriteID image, PaletteID pal, const TileInfo *ti, int z_offset, FoundationPart foundation_part); // viewport.cpp
extern const Station *_viewport_highlight_station;
extern TileHighlightData _thd;
extern bool IsInsideSelectedRectangle(int x, int y);

namespace citymania {

struct TileZoning {
    uint8 town_zone : 3;
    uint8 industry_fund_result : 2;
    uint8 advertisement_zone : 2;
    IndustryType industry_fund_type;
};

static TileZoning *_mz = nullptr;
static IndustryType _industry_forbidden_tiles = INVALID_INDUSTRYTYPE;

extern StationBuildingStatus _station_building_status;
extern const Station *_station_to_join;
extern const Station *_highlight_station_to_join;
extern TileArea _highlight_join_area;

std::set<std::pair<uint32, const Town*>, std::greater<std::pair<uint32, const Town*> > > _town_cache;
// struct {
//     int w;
//     int h;
//     int catchment;
// } _station_select;

const byte _tileh_to_sprite[32] = {
    0, 1, 2, 3, 4, 5, 6,  7, 8, 9, 10, 11, 12, 13, 14, 0,
    0, 0, 0, 0, 0, 0, 0, 16, 0, 0,  0, 17,  0, 15, 18, 0,
};


template <typename F>
uint8 Get(uint32 x, uint32 y, F getter) {
    if (x >= MapSizeX() || y >= MapSizeY()) return 0;
    return getter(TileXY(x, y));
}

template <typename F>
std::pair<ZoningBorder, uint8> CalcTileBorders(TileIndex tile, F getter) {
    auto x = TileX(tile), y = TileY(tile);
    ZoningBorder res = ZoningBorder::NONE;
    auto z = getter(tile);
    if (z == 0)
        return std::make_pair(res, 0);
    auto tr = Get(x - 1, y, getter);
    auto tl = Get(x, y - 1, getter);
    auto bl = Get(x + 1, y, getter);
    auto br = Get(x, y + 1, getter);
    if (tr < z) res |= ZoningBorder::TOP_RIGHT;
    if (tl < z) res |= ZoningBorder::TOP_LEFT;
    if (bl < z) res |= ZoningBorder::BOTTOM_LEFT;
    if (br < z) res |= ZoningBorder::BOTTOM_RIGHT;
    if (tr == z && tl == z && Get(x - 1, y - 1, getter) < z) res |= ZoningBorder::TOP_CORNER;
    if (tr == z && br == z && Get(x - 1, y + 1, getter) < z) res |= ZoningBorder::RIGHT_CORNER;
    if (br == z && bl == z && Get(x + 1, y + 1, getter) < z) res |= ZoningBorder::BOTTOM_CORNER;
    if (tl == z && bl == z && Get(x + 1, y - 1, getter) < z) res |= ZoningBorder::LEFT_CORNER;
    return std::make_pair(res, z);
}


bool CanBuildIndustryOnTileCached(IndustryType type, TileIndex tile) {
    if (_mz[tile].industry_fund_type != type || !_mz[tile].industry_fund_result) {
        bool res = CanBuildIndustryOnTile(type, tile);
        _mz[tile].industry_fund_type = type;
        _mz[tile].industry_fund_result = res ? 2 : 1;
        return res;
    }
    return (_mz[tile].industry_fund_result == 2);
}

void DrawBorderSprites(const TileInfo *ti, ZoningBorder border, SpriteID color) {
    auto b = (uint8)border & 15;
    auto tile_sprite = SPR_BORDER_HIGHLIGHT_BASE + _tileh_to_sprite[ti->tileh] * 19;
    if (b) {
        DrawSelectionSprite(tile_sprite + b - 1, color, ti, 7, FOUNDATION_PART_NORMAL);
    }
    if (border & ZoningBorder::TOP_CORNER)
        DrawSelectionSprite(tile_sprite + 15, color, ti, 7, FOUNDATION_PART_NORMAL);
    if (border & ZoningBorder::RIGHT_CORNER)
        DrawSelectionSprite(tile_sprite + 16, color, ti, 7, FOUNDATION_PART_NORMAL);
    if (border & ZoningBorder::BOTTOM_CORNER)
        DrawSelectionSprite(tile_sprite + 17, color, ti, 7, FOUNDATION_PART_NORMAL);
    if (border & ZoningBorder::LEFT_CORNER)
        DrawSelectionSprite(tile_sprite + 18, color, ti, 7, FOUNDATION_PART_NORMAL);
}

SpriteID GetIndustryZoningPalette(TileIndex tile) {
    if (!IsTileType(tile, MP_INDUSTRY)) return PAL_NONE;
    Industry *ind = Industry::GetByTile(tile);
    auto n_produced = 0;
    auto n_serviced = 0;
    for (auto j = 0; j < INDUSTRY_NUM_OUTPUTS; j++) {
        if (ind->produced_cargo[j] == CT_INVALID) continue;
        if (ind->last_month_production[j] == 0 && ind->this_month_production[j] == 0) continue;
        n_produced++;
        if (ind->last_month_transported[j] > 0 || ind->last_month_transported[j] > 0)
            n_serviced++;
    }
    if (n_serviced < n_produced)
        return (n_serviced == 0 ? PALETTE_TINT_RED_DEEP : PALETTE_TINT_ORANGE_DEEP);
    return PAL_NONE;
}

SpriteID GetTintBySelectionColour(SpriteID colour, bool deep=false) {
    switch(colour) {
        case SPR_PALETTE_ZONING_RED: return (deep ? PALETTE_TINT_RED_DEEP : PALETTE_TINT_RED);
        case SPR_PALETTE_ZONING_ORANGE: return (deep ? PALETTE_TINT_ORANGE_DEEP : PALETTE_TINT_ORANGE);
        case SPR_PALETTE_ZONING_GREEN: return (deep ? PALETTE_TINT_GREEN_DEEP : PALETTE_TINT_GREEN);
        case SPR_PALETTE_ZONING_LIGHT_BLUE: return (deep ? PALETTE_TINT_CYAN_DEEP : PALETTE_TINT_CYAN);
        case SPR_PALETTE_ZONING_YELLOW: return PALETTE_TINT_YELLOW;
        // case SPR_PALETTE_ZONING__: return PALETTE_TINT_YELLOW_WHITE;
        case SPR_PALETTE_ZONING_WHITE: return PALETTE_TINT_WHITE;
        default: return PAL_NONE;
    }
}

static void SetStationSelectionHighlight(const TileInfo *ti, TileHighlight &th) {
    bool draw_selection = ((_thd.drawstyle & HT_DRAG_MASK) == HT_RECT && _thd.outersize.x > 0);
    const Station *highlight_station = _viewport_highlight_station;

    if (_highlight_station_to_join) highlight_station = _highlight_station_to_join;

    if (draw_selection) {
        auto b = CalcTileBorders(ti->tile, [](TileIndex t) {
            auto x = TileX(t) * TILE_SIZE, y = TileY(t) * TILE_SIZE;
            return IsInsideSelectedRectangle(x, y);
        });
        const SpriteID pal[] = {SPR_PALETTE_ZONING_RED, SPR_PALETTE_ZONING_YELLOW, SPR_PALETTE_ZONING_LIGHT_BLUE, SPR_PALETTE_ZONING_GREEN};
        auto color = pal[(int)_station_building_status];
        if (_thd.make_square_red) color = SPR_PALETTE_ZONING_RED;
        if (b.first != ZoningBorder::NONE)
            th.add_border(b.first, color);
        if (IsInsideSelectedRectangle(TileX(ti->tile) * TILE_SIZE, TileY(ti->tile) * TILE_SIZE)) {
            th.ground_pal = GetTintBySelectionColour(color);
            return;
        }
    }

    auto coverage_getter = [draw_selection, highlight_station](TileIndex t) {
        auto x = TileX(t) * TILE_SIZE, y = TileY(t) * TILE_SIZE;
        if (highlight_station && IsTileType(t, MP_STATION) && GetStationIndex(t) == highlight_station->index) return 2;
        if (_settings_client.gui.station_show_coverage && highlight_station && highlight_station->TileIsInCatchment(t)) return 1;
        if (draw_selection && _settings_client.gui.station_show_coverage && IsInsideBS(x, _thd.pos.x + _thd.offs.x, _thd.size.x + _thd.outersize.x) &&
                        IsInsideBS(y, _thd.pos.y + _thd.offs.y, _thd.size.y + _thd.outersize.y)) return 1;
        return 0;
    };
    auto b = CalcTileBorders(ti->tile, coverage_getter);
    if (b.second) {
        // const SpriteID pal[] = {PAL_NONE, SPR_PALETTE_ZONING_WHITE, SPR_PALETTE_ZONING_LIGHT_BLUE};
        const SpriteID pal[] = {PAL_NONE, SPR_PALETTE_ZONING_WHITE, PAL_NONE};
        th.add_border(b.first, pal[b.second]);
        const SpriteID pal2[] = {PAL_NONE, PALETTE_TINT_WHITE, PALETTE_TINT_BLUE};
        th.ground_pal = th.structure_pal = pal2[b.second];
    }

    if (_highlight_join_area.tile != INVALID_TILE) {
        auto b = CalcTileBorders(ti->tile, [](TileIndex t) {
            return _highlight_join_area.Contains(t) ? 1 : 0;
        });
        th.add_border(b.first, SPR_PALETTE_ZONING_LIGHT_BLUE);
        if (b.second) {
            switch (th.ground_pal) {
                case PALETTE_TINT_WHITE: th.ground_pal = th.structure_pal = PALETTE_TINT_CYAN_WHITE; break;
                case PALETTE_TINT_BLUE: break;
                default: th.ground_pal = th.structure_pal = PALETTE_TINT_CYAN; break;
            }
        }
    }
}

void CalcCBAcceptanceBorders(TileHighlight &th, TileIndex tile, SpriteID border_pal, SpriteID ground_pal) {
    int tx = TileX(tile), ty = TileY(tile);
    uint16 radius = _settings_client.gui.cb_distance_check;
    bool in_zone = false;
    ZoningBorder border = ZoningBorder::NONE;
    _town_kdtree.FindContained(
        (uint16)max<int>(0, tx - radius),
        (uint16)max<int>(0, ty - radius),
        (uint16)min<int>(tx + radius + 1, MapSizeX()),
        (uint16)min<int>(ty + radius + 1, MapSizeY()),
        [tx, ty, radius, &in_zone, &border] (TownID tid) {
            Town *town = Town::GetIfValid(tid);
            if (!town || town->larger_town)
                return;

            int dx = TileX(town->xy) - tx;
            int dy = TileY(town->xy) - ty;
            in_zone = in_zone || (max(abs(dx), abs(dy)) <= radius);
            if (dx == radius) border |= ZoningBorder::TOP_RIGHT;
            if (dx == -radius) border |= ZoningBorder::BOTTOM_LEFT;
            if (dy == radius) border |= ZoningBorder::TOP_LEFT;
            if (dy == -radius) border |= ZoningBorder::BOTTOM_RIGHT;
        }
    );
    th.add_border(border, border_pal);
    if (in_zone) th.tint_all(ground_pal);
}


void AddTownCBLimitBorder(TileIndex tile, const Town *town, ZoningBorder &border, bool &in_zone) {
    auto sq = town->cache.squared_town_zone_radius[0] + 30;
    auto x = CalcTileBorders(tile, [town, sq] (TileIndex tile) {
        return DistanceSquare(tile, town->xy) <= sq ? 1 : 0;
    });
    border |= x.first;
    in_zone = in_zone || x.second;
}

void CalcCBTownLimitBorder(TileHighlight &th, TileIndex tile, SpriteID border_pal, SpriteID ground_pal) {
    auto n = Town::GetNumItems();
    uint32 sq = 0;
    uint i = 0;
    ZoningBorder border = ZoningBorder::NONE;
    bool in_zone = false;
    for (auto &p : _town_cache) {
        sq = p.second->cache.squared_town_zone_radius[0] + 30;
        if (4 * sq * n < MapSize() * i) break;
        AddTownCBLimitBorder(tile, p.second, border, in_zone);
        i++;
    }
    uint radius = IntSqrt(sq);
    int tx = TileX(tile), ty = TileY(tile);
    _town_kdtree.FindContained(
        (uint16)max<int>(0, tx - radius),
        (uint16)max<int>(0, ty - radius),
        (uint16)min<int>(tx + radius + 1, MapSizeX()),
        (uint16)min<int>(ty + radius + 1, MapSizeY()),
        [tile, &in_zone, &border] (TownID tid) {
            Town *town = Town::GetIfValid(tid);
            if (!town || town->larger_town)
                return;
            AddTownCBLimitBorder(tile, town, border, in_zone);
        }
    );
    th.add_border(border, border_pal);
    if (in_zone) th.tint_all(ground_pal);
}

TileHighlight GetTileHighlight(const TileInfo *ti) {
    TileHighlight th;
    if (ti->tile == INVALID_TILE) return th;
    if (_zoning.outer == CHECKTOWNZONES) {
        auto p = GetTownZoneBorder(ti->tile);
        auto color = PAL_NONE;
        switch (p.second) {
            default: break; // Tz0
            case 1: color = SPR_PALETTE_ZONING_WHITE; break; // Tz0
            case 2: color = SPR_PALETTE_ZONING_YELLOW; break; // Tz1
            case 3: color = SPR_PALETTE_ZONING_ORANGE; break; // Tz2
            case 4: color = SPR_PALETTE_ZONING_ORANGE; break; // Tz3
            case 5: color = SPR_PALETTE_ZONING_RED; break; // Tz4 - center
        };
        th.add_border(p.first, color);
        th.ground_pal = th.structure_pal = GetTintBySelectionColour(color);
        if (CB_Enabled())
            CalcCBTownLimitBorder(th, ti->tile, SPR_PALETTE_ZONING_RED, PAL_NONE);
    } else if (_zoning.outer == CHECKSTACATCH) {
        th.add_border(citymania::GetAnyStationCatchmentBorder(ti->tile),
                      SPR_PALETTE_ZONING_LIGHT_BLUE);
    } else if (_zoning.outer == CHECKTOWNGROWTHTILES) {
        // if (tgt == TGTS_NEW_HOUSE) th.sprite = SPR_IMG_HOUSE_NEW;
        switch (_game->get_town_growth_tile(ti->tile)) {
            // case TGTS_CB_HOUSE_REMOVED_NOGROW:
            case TownGrowthTileState::RH_REMOVED:
                th.selection = SPR_PALETTE_ZONING_LIGHT_BLUE;
                break;
            case TownGrowthTileState::RH_REBUILT:
                th.selection = SPR_PALETTE_ZONING_WHITE;
                th.structure_pal = PALETTE_TINT_WHITE;
                break;
            case TownGrowthTileState::NEW_HOUSE:
                th.selection = SPR_PALETTE_ZONING_GREEN;
                th.structure_pal = PALETTE_TINT_GREEN;
                break;
            case TownGrowthTileState::CS:
                th.selection = SPR_PALETTE_ZONING_ORANGE;
                break;
            case TownGrowthTileState::HS:
                th.selection = SPR_PALETTE_ZONING_YELLOW;
                break;
            case TownGrowthTileState::HR:
                th.selection = SPR_PALETTE_ZONING_RED;
                break;
            default: th.selection = PAL_NONE;
        }
        if (th.selection) th.ground_pal = GetTintBySelectionColour(th.selection);
    } else if (_zoning.outer == CHECKBULUNSER) {
        if (IsTileType (ti->tile, MP_HOUSE)) {
            StationFinder stations(TileArea(ti->tile, 1, 1));

            // TODO check cargos
            if (stations.GetStations()->empty())
                th.ground_pal = th.structure_pal = PALETTE_TINT_RED_DEEP;
        }
    } else if (_zoning.outer == CHECKINDUNSER) {
        auto pal = GetIndustryZoningPalette(ti->tile);
        if (pal) th.ground_pal = th.structure_pal = PALETTE_TINT_RED_DEEP;
    } else if (_zoning.outer == CHECKTOWNADZONES) {
        auto getter = [](TileIndex t) { return _mz[t].advertisement_zone; };
        auto b = CalcTileBorders(ti->tile, getter);
        const SpriteID pal[] = {PAL_NONE, SPR_PALETTE_ZONING_YELLOW, SPR_PALETTE_ZONING_ORANGE, SPR_PALETTE_ZONING_RED};
        th.add_border(b.first, pal[b.second]);
        auto check_tile = ti->tile;
        if (IsTileType (ti->tile, MP_STATION)) {
            auto station =  Station::GetByTile(ti->tile);
            if (station) check_tile = station->xy;
        }
        auto z = getter(check_tile);
        if (z) th.ground_pal = th.structure_pal = GetTintBySelectionColour(pal[z]);
    } else if (_zoning.outer == CHECKCBACCEPTANCE) {
        CalcCBAcceptanceBorders(th, ti->tile, SPR_PALETTE_ZONING_WHITE, PALETTE_TINT_WHITE);
    } else if (_zoning.outer == CHECKCBTOWNLIMIT) {
        CalcCBTownLimitBorder(th, ti->tile, SPR_PALETTE_ZONING_WHITE, PALETTE_TINT_WHITE);
    } else if (_zoning.outer == CHECKACTIVESTATIONS) {
        auto getter = [](TileIndex t) {
            if (!IsTileType (t, MP_STATION)) return 0;
            Station *st = Station::GetByTile(t);
            if (!st) return 0;
            if (st->time_since_load <= 20 || st->time_since_unload <= 20)
                return 1;
            return 2;
        };
        auto b = CalcTileBorders(ti->tile, getter);
        const SpriteID pal[] = {PAL_NONE, SPR_PALETTE_ZONING_GREEN, SPR_PALETTE_ZONING_RED};
        th.add_border(b.first, pal[b.second]);
        auto z = getter(ti->tile);
        if (z) th.ground_pal = th.structure_pal = GetTintBySelectionColour(pal[z]);
    }

    if (_settings_client.gui.show_industry_forbidden_tiles &&
            _industry_forbidden_tiles != INVALID_INDUSTRYTYPE) {
        auto b = CalcTileBorders(ti->tile, [](TileIndex t) { return !CanBuildIndustryOnTileCached(_industry_forbidden_tiles, t); });
        th.add_border(b.first, SPR_PALETTE_ZONING_RED);
        if (!CanBuildIndustryOnTileCached(_industry_forbidden_tiles, ti->tile))
            th.ground_pal = th.structure_pal = PALETTE_TINT_RED;
    }

    SetStationSelectionHighlight(ti, th);

    return th;
}

void DrawTileSelection(const TileInfo *ti, const TileHighlight &th) {
    for (uint i = 0; i < th.border_count; i++)
        DrawBorderSprites(ti, th.border[i], th.border_color[i]);
    if (th.sprite) {
        DrawSelectionSprite(th.sprite, PAL_NONE, ti, 0, FOUNDATION_PART_NORMAL);
    }
    if (th.selection) {
        DrawBorderSprites(ti, ZoningBorder::FULL, th.selection);
        // DrawSelectionSprite(SPR_SELECT_TILE + _tileh_to_sprite[ti->tileh],
        //                     th.selection, ti, 0, FOUNDATION_PART_NORMAL);
    }
}

void AllocateZoningMap(uint map_size) {
    free(_mz);
    _mz = CallocT<TileZoning>(map_size);
}

uint8 GetTownZone(Town *town, TileIndex tile) {
    auto dist = DistanceSquare(tile, town->xy);
    if (dist > town->cache.squared_town_zone_radius[HZB_TOWN_EDGE])
        return 0;

    uint8 z = 1;
    for (HouseZonesBits i = HZB_TOWN_OUTSKIRT; i < HZB_END; i++)
        if (dist < town->cache.squared_town_zone_radius[i])
            z = (uint8)i + 1;
        else if (town->cache.squared_town_zone_radius[i] != 0)
            break;
    return z;
}

uint8 GetAnyTownZone(TileIndex tile) {
    HouseZonesBits next_zone = HZB_BEGIN;
    uint8 z = 0;

    for (Town *town : Town::Iterate()) {
        uint dist = DistanceSquare(tile, town->xy);
        // town code uses <= for checking town borders (tz0) but < for other zones
        while (next_zone < HZB_END
            && (town->cache.squared_town_zone_radius[next_zone] == 0
                || dist <= town->cache.squared_town_zone_radius[next_zone] - (next_zone == HZB_BEGIN ? 0 : 1))
        ) {
            if (town->cache.squared_town_zone_radius[next_zone] != 0)  z = (uint8)next_zone + 1;
            next_zone++;
        }
    }
    return z;
}

void UpdateTownZoning(Town *town, uint32 prev_edge) {
    auto edge = town->cache.squared_town_zone_radius[HZB_TOWN_EDGE];
    if (prev_edge && edge == prev_edge)
        return;

    auto area = OrthogonalTileArea(town->xy, 1, 1);
    bool recalc;
    if (edge < prev_edge) {
        area.Expand(IntSqrt(prev_edge));
        recalc = true;
    } else {
        area.Expand(IntSqrt(edge));
        recalc = false;
    }
    // TODO mark dirty only if zoning is on
    TILE_AREA_LOOP(tile, area) {
        uint8 group = GetTownZone(town, tile);

        if (_mz[tile].town_zone != group)
            _mz[tile].industry_fund_result = 0;

        if (_mz[tile].town_zone > group) {
            if (recalc) {
                _mz[tile].town_zone = GetAnyTownZone(tile);
                if (_zoning.outer == CHECKTOWNZONES)
                    MarkTileDirtyByTile(tile);
            }
        } else if (_mz[tile].town_zone < group) {
            _mz[tile].town_zone = group;
            if (_zoning.outer == CHECKTOWNZONES)
                MarkTileDirtyByTile(tile);
        }
    }
}

void UpdateAdvertisementZoning(TileIndex center, uint radius, uint8 zone) {
    uint16 x1, y1, x2, y2;
    x1 = (uint16)max<int>(0, TileX(center) - radius);
    x2 = (uint16)min<int>(TileX(center) + radius + 1, MapSizeX());
    y1 = (uint16)max<int>(0, TileY(center) - radius);
    y2 = (uint16)min<int>(TileY(center) + radius + 1, MapSizeY());
    for (uint16 y = y1; y < y2; y++) {
        for (uint16 x = x1; x < x2; x++) {
            auto tile = TileXY(x, y);
            if (DistanceManhattan(tile, center) > radius) continue;
            _mz[tile].advertisement_zone = max(_mz[tile].advertisement_zone, zone);
        }
    }
}

void UpdateZoningTownHouses(const Town *town, uint32 old_houses) {
    if (!town->larger_town)
        return;
    _town_cache.erase(std::make_pair(old_houses, town));
    _town_cache.insert(std::make_pair(town->cache.num_houses, town));
}

void InitializeZoningMap() {
    _town_cache.clear();
    for (Town *t : Town::Iterate()) {
        UpdateTownZoning(t, 0);
        UpdateAdvertisementZoning(t->xy, 10, 3);
        UpdateAdvertisementZoning(t->xy, 15, 2);
        UpdateAdvertisementZoning(t->xy, 20, 1);
        UpdateZoningTownHouses(t, 0);
        if (!t->larger_town)
            _town_cache.insert(std::make_pair(t->cache.num_houses, t));
    }
}

std::pair<ZoningBorder, uint8> GetTownZoneBorder(TileIndex tile) {
    return CalcTileBorders(tile, [](TileIndex t) { return _mz[t].town_zone; });
}

ZoningBorder GetAnyStationCatchmentBorder(TileIndex tile) {
    ZoningBorder border = ZoningBorder::NONE;
    StationFinder morestations(TileArea(tile, 1, 1));
    for (Station *st: *morestations.GetStations()) {
        border |= CalcTileBorders(tile, [st](TileIndex t) {return st->TileIsInCatchment(t) ? 1 : 0; }).first;
    }
    if (border & ZoningBorder::TOP_CORNER && border & (ZoningBorder::TOP_LEFT | ZoningBorder::TOP_RIGHT))
        border &= ~ZoningBorder::TOP_CORNER;
    return border;
}

void SetIndustryForbiddenTilesHighlight(IndustryType type) {
    if (_settings_client.gui.show_industry_forbidden_tiles &&
            _industry_forbidden_tiles != type) {
        MarkWholeScreenDirty();
    }
    _industry_forbidden_tiles = type;
}


PaletteID GetTreeShadePal(TileIndex tile) {
    if (_settings_client.gui.cm_shaded_trees != 1)
        return PAL_NONE;

    Slope slope = GetTileSlope(tile);
    switch (slope) {
        case SLOPE_STEEP_N:
        case SLOPE_N:
            return PALETTE_SHADE_S;

        case SLOPE_NE:
            return PALETTE_SHADE_SW;

        case SLOPE_E:
        case SLOPE_STEEP_E:
            return PALETTE_SHADE_W;

        case SLOPE_SE:
            return PALETTE_SHADE_NW;

        case SLOPE_STEEP_S:
        case SLOPE_S:
            return PALETTE_SHADE_N;

        case SLOPE_SW:
            return PALETTE_SHADE_NE;

        case SLOPE_STEEP_W:
        case SLOPE_W:
            return PALETTE_SHADE_E;

        case SLOPE_NW:
            return PALETTE_SHADE_SE;

        default:
            return PAL_NONE;
    }
}

}  // namespace citymania
