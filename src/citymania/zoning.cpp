#include "../stdafx.h"

#include "zoning.hpp"

#include "../core/math_func.hpp"
#include "../house.h"
#include "../industry.h"
#include "../town.h"
#include "../tilearea_type.h"
#include "../viewport_func.h"
#include "../zoning.h"

/** Enumeration of multi-part foundations */
enum FoundationPart {
    FOUNDATION_PART_NONE     = 0xFF,  ///< Neither foundation nor groundsprite drawn yet.
    FOUNDATION_PART_NORMAL   = 0,     ///< First part (normal foundation or no foundation)
    FOUNDATION_PART_HALFTILE = 1,     ///< Second part (halftile foundation)
    FOUNDATION_PART_END
};
extern void DrawSelectionSprite(SpriteID image, PaletteID pal, const TileInfo *ti, int z_offset, FoundationPart foundation_part); // viewport.cpp

namespace citymania {

struct TileZoning {
    uint8 town_zone : 3;
    uint8 industry_fund_result : 2;
    uint8 advertisement_zone : 2;
    IndustryType industry_fund_type;
};

static TileZoning *_mz = nullptr;
static IndustryType _industry_forbidden_tiles = INVALID_INDUSTRYTYPE;

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

TileHighlight GetTileHighlight(const TileInfo *ti) {
    TileHighlight th;
    if (ti->tile == INVALID_TILE) return th;
    if (_zoning.outer == CHECKTOWNZONES) {
        auto p = GetTownZoneBorder(ti->tile);
        th.border = p.first;
        auto pal = PAL_NONE;
        switch (p.second) {
            default: break; // Tz0
            case 1: th.border_color = SPR_PALETTE_ZONING_WHITE; pal = PALETTE_TINT_WHITE; break; // Tz0
            case 2: th.border_color = SPR_PALETTE_ZONING_YELLOW; pal = PALETTE_TINT_YELLOW; break; // Tz1
            case 3: th.border_color = SPR_PALETTE_ZONING_ORANGE; pal = PALETTE_TINT_ORANGE; break; // Tz2
            case 4: th.border_color = SPR_PALETTE_ZONING_ORANGE; pal = PALETTE_TINT_ORANGE; break; // Tz3
            case 5: th.border_color = SPR_PALETTE_ZONING_RED; pal = PALETTE_TINT_RED; break; // Tz4 - center
        };
        th.ground_pal = th.structure_pal = pal;
    } else if (_zoning.outer == CHECKSTACATCH) {
        th.border = citymania::GetAnyStationCatchmentBorder(ti->tile);
        th.border_color = SPR_PALETTE_ZONING_LIGHT_BLUE;
    } else if (_zoning.outer == CHECKTOWNGROWTHTILES) {
        auto tgt = max(_towns_growth_tiles[ti->tile], _towns_growth_tiles_last_month[ti->tile]);
        // if (tgt == TGTS_NEW_HOUSE) th.sprite = SPR_IMG_HOUSE_NEW;
        switch (tgt) {
            case TGTS_CB_HOUSE_REMOVED_NOGROW:
            case TGTS_RH_REMOVED:
                th.selection = SPR_PALETTE_ZONING_LIGHT_BLUE;
                th.ground_pal = PALETTE_TINT_CYAN;
                break;
            case TGTS_RH_REBUILT:
                th.selection = SPR_PALETTE_ZONING_WHITE;
                th.ground_pal = th.structure_pal = PALETTE_TINT_WHITE;
                break;
            case TGTS_NEW_HOUSE:
                th.selection = SPR_PALETTE_ZONING_GREEN;
                th.ground_pal = th.structure_pal = PALETTE_TINT_GREEN;
                break;
            case TGTS_CYCLE_SKIPPED:
                th.selection = SPR_PALETTE_ZONING_ORANGE;
                th.ground_pal = PALETTE_TINT_ORANGE;
                break;
            case TGTS_HOUSE_SKIPPED:
                th.selection = SPR_PALETTE_ZONING_YELLOW;
                th.ground_pal = PALETTE_TINT_YELLOW;
                break;
            case TGTS_CB_HOUSE_REMOVED:
                th.selection = SPR_PALETTE_ZONING_RED;
                th.ground_pal = PALETTE_TINT_RED;
                break;
            default: th.selection = PAL_NONE;
        }
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
        if (b.first != ZoningBorder::NONE) {
            th.border = b.first;
            const SpriteID pal[] = {PAL_NONE, SPR_PALETTE_ZONING_YELLOW, SPR_PALETTE_ZONING_ORANGE, SPR_PALETTE_ZONING_RED};
            th.border_color = pal[b.second];
        }
        auto z = getter(ti->tile);
        if (z) {
            const SpriteID pal[] = {PAL_NONE, PALETTE_TINT_YELLOW, PALETTE_TINT_ORANGE, PALETTE_TINT_RED};
            th.ground_pal = th.structure_pal = pal[b.second];
        }
    }

    if (_settings_client.gui.show_industry_forbidden_tiles &&
            _industry_forbidden_tiles != INVALID_INDUSTRYTYPE) {
        auto b = CalcTileBorders(ti->tile, [](TileIndex t) { return !CanBuildIndustryOnTileCached(_industry_forbidden_tiles, t); });
        if(b.first != ZoningBorder::NONE) {
            th.border = b.first;
            th.border_color = SPR_PALETTE_ZONING_RED;
        }
        if (!CanBuildIndustryOnTileCached(_industry_forbidden_tiles, ti->tile))
            th.ground_pal = th.structure_pal = PALETTE_TINT_RED;
    }

    return th;
}

void DrawTileSelection(const TileInfo *ti, const TileHighlight &th) {
    if (th.border != ZoningBorder::NONE)
        DrawBorderSprites(ti, th.border, th.border_color);
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

void InitializeZoningMap() {
    for (Town *t : Town::Iterate()) {
        UpdateTownZoning(t, 0);
        UpdateAdvertisementZoning(t->xy, 10, 3);
        UpdateAdvertisementZoning(t->xy, 15, 2);
        UpdateAdvertisementZoning(t->xy, 20, 1);
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

}  // namespace citymania
