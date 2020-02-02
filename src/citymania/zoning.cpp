#include "../stdafx.h"

#include "zoning.hpp"

#include "../core/math_func.hpp"
#include "../house.h"
#include "../industry.h"
#include "../town.h"
#include "../tilearea_type.h"
#include "../viewport_func.h"
#include "../zoning.h"

namespace citymania {

struct TileZoning {
    uint8 town_zone : 3;
};

TileZoning *_mz = nullptr;

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

void InitializeZoningMap() {
    for (Town *t : Town::Iterate()) {
        UpdateTownZoning(t, 0);
    }
}

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

SpriteID GetTownTileZoningPalette(TileIndex tile) {
    if (_zoning.outer == CHECKBULUNSER) {
        StationFinder stations(TileArea(tile, 1, 1));

        if (stations.GetStations()->empty())
            return SPR_RECOLOUR_RED;
    }
    return PAL_NONE;
}

SpriteID GetIndustryTileZoningPalette(TileIndex tile, Industry *ind) {
    if (_zoning.outer == CHECKINDUNSER) {
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
            return (n_serviced == 0 ? SPR_RECOLOUR_RED : SPR_RECOLOUR_ORANGE);
    }

    return PAL_NONE;
}

}  // namespace citymania
