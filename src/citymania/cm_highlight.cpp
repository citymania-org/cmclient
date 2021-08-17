#include "../stdafx.h"

#include "cm_highlight.hpp"

// #include "cm_blueprint.hpp"
#include "cm_main.hpp"
#include "cm_station_gui.hpp"
#include "cm_zoning.hpp"

#include "../core/math_func.hpp"
#include "../table/bridge_land.h"
#include "../command_func.h"
#include "../house.h"
#include "../industry.h"
#include "../landscape.h"
#include "../newgrf_airporttiles.h"
#include "../newgrf_railtype.h"
#include "../newgrf_roadtype.h"
#include "../newgrf_station.h"
#include "../town.h"
#include "../town_kdtree.h"
#include "../tilearea_type.h"
#include "../tilehighlight_type.h"
#include "../tilehighlight_func.h"
#include "../viewport_func.h"
// #include "../zoning.h"
#include "../table/airporttile_ids.h"
#include "../table/track_land.h"

#include <set>


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
extern RailType _cur_railtype;
extern RoadType _cur_roadtype;
extern AirportClassID _selected_airport_class; ///< the currently visible airport class
extern int _selected_airport_index;
extern byte _selected_airport_layout;
extern DiagDirection _build_depot_direction; ///< Currently selected depot direction
extern DiagDirection _road_station_picker_orientation;
extern DiagDirection _road_depot_orientation;
extern uint32 _realtime_tick;
extern void GetStationLayout(byte *layout, uint numtracks, uint plat_len, const StationSpec *statspec);

struct RailStationGUISettings {
    Axis orientation;                 ///< Currently selected rail station orientation

    bool newstations;                 ///< Are custom station definitions available?
    StationClassID station_class;     ///< Currently selected custom station class (if newstations is \c true )
    byte station_type;                ///< %Station type within the currently selected custom station class (if newstations is \c true )
    byte station_count;               ///< Number of custom stations (if newstations is \c true )
};
extern RailStationGUISettings _railstation; ///< Settings of the station builder GUI

namespace citymania {

extern void (*DrawTileSelectionRect)(const TileInfo *ti, PaletteID pal);
extern void (*DrawAutorailSelection)(const TileInfo *ti, HighLightStyle autorail_type, PaletteID pal);

struct TileZoning {
    uint8 town_zone : 3;
    uint8 industry_fund_result : 2;
    uint8 advertisement_zone : 2;
    // IndustryType industry_fund_type;
    uint8 industry_fund_update;
};

static TileZoning *_mz = nullptr;
static IndustryType _industry_forbidden_tiles = INVALID_INDUSTRYTYPE;

extern StationBuildingStatus _station_building_status;
extern const Station *_station_to_join;
extern const Station *_highlight_station_to_join;
extern TileArea _highlight_join_area;

std::set<std::pair<uint32, const Town*>, std::greater<std::pair<uint32, const Town*>>> _town_cache;
// struct {
//     int w;
//     int h;
//     int catchment;
// } _station_select;

const byte _tileh_to_sprite[32] = {
    0, 1, 2, 3, 4, 5, 6,  7, 8, 9, 10, 11, 12, 13, 14, 0,
    0, 0, 0, 0, 0, 0, 0, 16, 0, 0,  0, 17,  0, 15, 18, 0,
};

ObjectTileHighlight ObjectTileHighlight::make_rail_depot(SpriteID palette, DiagDirection ddir) {
    auto oh = ObjectTileHighlight(Type::RAIL_DEPOT, palette);
    oh.u.rail.depot.ddir = ddir;
    return oh;
}

ObjectTileHighlight ObjectTileHighlight::make_rail_track(SpriteID palette, Track track) {
    auto oh = ObjectTileHighlight(Type::RAIL_TRACK, palette);
    oh.u.rail.track = track;
    return oh;
}

ObjectTileHighlight ObjectTileHighlight::make_rail_station(SpriteID palette, Axis axis, byte section) {
    auto oh = ObjectTileHighlight(Type::RAIL_STATION, palette);
    oh.u.rail.station.axis = axis;
    oh.u.rail.station.section = section;
    return oh;
}

ObjectTileHighlight ObjectTileHighlight::make_rail_signal(SpriteID palette, uint pos, SignalType type, SignalVariant variant) {
    auto oh = ObjectTileHighlight(Type::RAIL_SIGNAL, palette);
    oh.u.rail.signal.pos = pos;
    oh.u.rail.signal.type = type;
    oh.u.rail.signal.variant = variant;
    return oh;
}

ObjectTileHighlight ObjectTileHighlight::make_rail_bridge_head(SpriteID palette, DiagDirection ddir, BridgeType type) {
    auto oh = ObjectTileHighlight(Type::RAIL_BRIDGE_HEAD, palette);
    oh.u.rail.bridge_head.ddir = ddir;
    oh.u.rail.bridge_head.type = type;
    return oh;
}

ObjectTileHighlight ObjectTileHighlight::make_rail_tunnel_head(SpriteID palette, DiagDirection ddir) {
    auto oh = ObjectTileHighlight(Type::RAIL_TUNNEL_HEAD, palette);
    oh.u.rail.tunnel_head.ddir = ddir;
    return oh;
}

ObjectTileHighlight ObjectTileHighlight::make_road_stop(SpriteID palette, RoadType roadtype, DiagDirection ddir, bool is_truck) {
    auto oh = ObjectTileHighlight(Type::ROAD_STOP, palette);
    oh.u.road.stop.roadtype = roadtype;
    oh.u.road.stop.ddir = ddir;
    oh.u.road.stop.is_truck = is_truck;
    return oh;
}

ObjectTileHighlight ObjectTileHighlight::make_road_depot(SpriteID palette, RoadType roadtype, DiagDirection ddir) {
    auto oh = ObjectTileHighlight(Type::ROAD_DEPOT, palette);
    oh.u.road.depot.roadtype = roadtype;
    oh.u.road.depot.ddir = ddir;
    return oh;
}

ObjectTileHighlight ObjectTileHighlight::make_airport_tile(SpriteID palette, StationGfx gfx) {
    auto oh = ObjectTileHighlight(Type::AIRPORT_TILE, palette);
    oh.u.airport_tile.gfx = gfx;
    return oh;
}


bool ObjectHighlight::operator==(const ObjectHighlight& oh) {
    if (this->type != oh.type) return false;
    return (this->tile == oh.tile
            && this->end_tile == oh.end_tile
            && this->axis == oh.axis
            && this->ddir == oh.ddir
            && this->roadtype == oh.roadtype
            && this->is_truck == oh.is_truck
            && this->airport_type == oh.airport_type
            && this->airport_layout == oh.airport_layout
            && this->blueprint == oh.blueprint);
    // switch (this->type) {
    //     case Type::RAIL_DEPOT: return this->tile == oh.tile && this->ddir == oh.ddir;
    //     default: return true;
    // }
    // return true;
}

bool ObjectHighlight::operator!=(const ObjectHighlight& oh) {
    return !(*this == oh);
}


ObjectHighlight ObjectHighlight::make_rail_depot(TileIndex tile, DiagDirection ddir) {
    auto oh = ObjectHighlight{ObjectHighlight::Type::RAIL_DEPOT};
    oh.tile = tile;
    oh.ddir = ddir;
    return oh;
}

ObjectHighlight ObjectHighlight::make_rail_station(TileIndex start_tile, TileIndex end_tile, Axis axis) {
    auto oh = ObjectHighlight{ObjectHighlight::Type::RAIL_STATION};
    oh.tile = start_tile;
    oh.end_tile = end_tile;
    oh.axis = axis;
    return oh;
}

ObjectHighlight ObjectHighlight::make_road_stop(TileIndex start_tile, TileIndex end_tile, RoadType roadtype, DiagDirection orientation, bool is_truck) {
    auto oh = ObjectHighlight{ObjectHighlight::Type::ROAD_STOP};
    oh.tile = start_tile;
    oh.end_tile = end_tile;
    oh.ddir = orientation;
    oh.roadtype = roadtype;
    oh.is_truck = is_truck;
    return oh;
}

ObjectHighlight ObjectHighlight::make_road_depot(TileIndex tile, RoadType roadtype, DiagDirection orientation) {
    auto oh = ObjectHighlight{ObjectHighlight::Type::ROAD_DEPOT};
    oh.tile = tile;
    oh.ddir = orientation;
    oh.roadtype = roadtype;
    return oh;
}

ObjectHighlight ObjectHighlight::make_airport(TileIndex start_tile, int airport_type, byte airport_layout) {
    auto oh = ObjectHighlight{ObjectHighlight::Type::AIRPORT};
    oh.tile = start_tile;
    oh.airport_type = airport_type;
    oh.airport_layout = airport_layout;
    return oh;
}



// ObjectHighlight ObjectHighlight::make_blueprint(TileIndex tile, sp<Blueprint> blueprint) {
//     auto oh = ObjectHighlight{ObjectHighlight::Type::BLUEPRINT};
//     oh.tile = tile;
//     oh.blueprint = blueprint;
//     return oh;
// }

/**
 * Try to add an additional rail-track at the entrance of a depot
 * @param tile  Tile to use for adding the rail-track
 * @param dir   Direction to check for already present tracks
 * @param track Track to add
 * @see CcRailDepot()
 */
void ObjectHighlight::PlaceExtraDepotRail(TileIndex tile, DiagDirection dir, Track track)
{
    if (GetRailTileType(tile) != RAIL_TILE_NORMAL) return;
    if ((GetTrackBits(tile) & DiagdirReachesTracks(dir)) == 0) return;

    this->tiles.insert(std::make_pair(tile, ObjectTileHighlight::make_rail_track(PALETTE_TINT_WHITE, track)));
}

/** Additional pieces of track to add at the entrance of a depot. */
static const Track _place_depot_extra_track[12] = {
    TRACK_LEFT,  TRACK_UPPER, TRACK_UPPER, TRACK_RIGHT, // First additional track for directions 0..3
    TRACK_X,     TRACK_Y,     TRACK_X,     TRACK_Y,     // Second additional track
    TRACK_LOWER, TRACK_LEFT,  TRACK_RIGHT, TRACK_LOWER, // Third additional track
};

/** Direction to check for existing track pieces. */
static const DiagDirection _place_depot_extra_dir[12] = {
    DIAGDIR_SE, DIAGDIR_SW, DIAGDIR_SE, DIAGDIR_SW,
    DIAGDIR_SW, DIAGDIR_NW, DIAGDIR_NE, DIAGDIR_SE,
    DIAGDIR_NW, DIAGDIR_NE, DIAGDIR_NW, DIAGDIR_NE,
};

static bool CanBuild(TileIndex tile, uint32 p1, uint32 p2, uint32 cmd) {
    return DoCommandPInternal(
        tile,
        p1,
        p2,
        cmd,
        nullptr,  // callback
        "",  // text
        true,  // my_cmd
        true  // estimate_only
    ).Succeeded();
}

void ObjectHighlight::UpdateTiles() {
    this->tiles.clear();
    switch (this->type) {
        case Type::NONE:
            break;

        case Type::RAIL_DEPOT: {
            auto dir = this->ddir;

            auto palette = (CanBuild(
                this->tile,
                _cur_railtype,
                dir,
                CMD_BUILD_TRAIN_DEPOT
            ) ? PALETTE_TINT_WHITE : PALETTE_TINT_RED_DEEP);

            this->tiles.insert(std::make_pair(this->tile, ObjectTileHighlight::make_rail_depot(palette, dir)));
            auto tile = this->tile + TileOffsByDiagDir(dir);
            if (IsTileType(tile, MP_RAILWAY) && IsCompatibleRail(GetRailType(tile), _cur_railtype)) {
                this->PlaceExtraDepotRail(tile, _place_depot_extra_dir[dir], _place_depot_extra_track[dir]);
                this->PlaceExtraDepotRail(tile, _place_depot_extra_dir[dir + 4], _place_depot_extra_track[dir + 4]);
                this->PlaceExtraDepotRail(tile, _place_depot_extra_dir[dir + 8], _place_depot_extra_track[dir + 8]);
            }
            break;
        }
        case Type::RAIL_STATION: {
            auto ta = OrthogonalTileArea(this->tile, this->end_tile);
            auto numtracks = ta.w;
            auto plat_len = ta.h;
            if (this->axis == AXIS_X) Swap(numtracks, plat_len);

            auto palette = (CanBuild(
                this->tile,
                _cur_railtype
                    | (this->axis << 6)
                    | ((uint32)numtracks << 8)
                    | ((uint32)plat_len << 16),
                NEW_STATION << 16,
                CMD_BUILD_RAIL_STATION
            ) ? PALETTE_TINT_WHITE : PALETTE_TINT_RED_DEEP);

            auto layout_ptr = AllocaM(byte, (int)numtracks * plat_len);
            GetStationLayout(layout_ptr, numtracks, plat_len, nullptr); // TODO statspec

            auto tile_delta = (this->axis == AXIS_X ? TileDiffXY(1, 0) : TileDiffXY(0, 1));
            TileIndex tile_track = this->tile;
            do {
                TileIndex tile = tile_track;
                int w = plat_len;
                do {
                    byte layout = *layout_ptr++;
                    this->tiles.insert(std::make_pair(tile, ObjectTileHighlight::make_rail_station(palette, this->axis, layout & ~1)));
                    tile += tile_delta;
                } while (--w);
                tile_track += tile_delta ^ TileDiffXY(1, 1); // perpendicular to tile_delta
            } while (--numtracks);

            break;
        }
        case Type::ROAD_STOP: {
            auto ta = OrthogonalTileArea(this->tile, this->end_tile);
            auto palette = (CanBuild(
                this->tile,
                (uint32)(ta.w | ta.h << 8),
                (this->is_truck ? 1 : 0) | (this->ddir >= DIAGDIR_END ? 2 : 0) | (((uint)this->ddir % 4) << 3) | (NEW_STATION << 16),
                CMD_BUILD_ROAD_STOP
            ) ? PALETTE_TINT_WHITE : PALETTE_TINT_RED_DEEP);
            TileIndex tile;
            for (TileIndex tile : ta) {
                this->tiles.insert(std::make_pair(tile, ObjectTileHighlight::make_road_stop(palette, this->roadtype, this->ddir, this->is_truck)));
            }
            break;
        }

        case Type::ROAD_DEPOT: {
            auto palette = (CanBuild(
                this->tile,
                this->roadtype << 2 | this->ddir,
                0,
                CMD_BUILD_ROAD_DEPOT
            ) ? PALETTE_TINT_WHITE : PALETTE_TINT_RED_DEEP);
            this->tiles.insert(std::make_pair(this->tile, ObjectTileHighlight::make_road_depot(palette, this->roadtype, this->ddir)));
            break;
        }

        case Type::AIRPORT: {
            auto palette = (CanBuild(
                this->tile,
                this->airport_type | ((uint)this->airport_layout << 8),
                1 | (NEW_STATION << 16),
                CMD_BUILD_AIRPORT
            ) ? PALETTE_TINT_WHITE : PALETTE_TINT_RED_DEEP);

            const AirportSpec *as = AirportSpec::Get(this->airport_type);
            if (!as->IsAvailable() || this->airport_layout >= as->num_table) break;
            Direction rotation = as->rotation[this->airport_layout];
            int w = as->size_x;
            int h = as->size_y;
            if (rotation == DIR_E || rotation == DIR_W) Swap(w, h);
            TileArea airport_area = TileArea(this->tile, w, h);
            for (AirportTileTableIterator iter(as->table[this->airport_layout], this->tile); iter != INVALID_TILE; ++iter) {
                this->tiles.insert(std::make_pair(iter, ObjectTileHighlight::make_airport_tile(palette, iter.GetStationGfx())));
            }
            break;
        }
        // case Type::BLUEPRINT:
        //     if (this->blueprint && this->tile != INVALID_TILE)
        //         this->tiles = this->blueprint->GetTiles(this->tile);
        //     break;
        default:
            NOT_REACHED();
    }
}

void ObjectHighlight::MarkDirty() {
    this->UpdateTiles();
    for (const auto &kv: this->tiles) {
        MarkTileDirtyByTile(kv.first);
    }
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

SpriteID GetSelectionColourByTint(SpriteID colour) {
    switch(colour) {
        case PALETTE_TINT_RED_DEEP:
        case PALETTE_TINT_RED:
            return SPR_PALETTE_ZONING_RED;
        case PALETTE_TINT_ORANGE_DEEP:
        case PALETTE_TINT_ORANGE:
            return SPR_PALETTE_ZONING_ORANGE;
        case PALETTE_TINT_GREEN_DEEP:
        case PALETTE_TINT_GREEN:
            return SPR_PALETTE_ZONING_GREEN;
        case PALETTE_TINT_CYAN_DEEP:
        case PALETTE_TINT_CYAN:
            return SPR_PALETTE_ZONING_LIGHT_BLUE;
        case PALETTE_TINT_YELLOW:
            return SPR_PALETTE_ZONING_YELLOW;
        // returnase SPR_PALETTE_ZONING__: return PALETTE_TINT_YELLOW_WHITE;
        case PALETTE_TINT_WHITE:
            return SPR_PALETTE_ZONING_WHITE;
        default: return PAL_NONE;
    }
}

void DrawTrainDepotSprite(SpriteID palette, const TileInfo *ti, RailType railtype, DiagDirection ddir)
{
    const DrawTileSprites *dts = &_depot_gfx_table[ddir];
    const RailtypeInfo *rti = GetRailTypeInfo(railtype);
    SpriteID image = rti->UsesOverlay() ? SPR_FLAT_GRASS_TILE : dts->ground.sprite;
    uint32 offset = rti->GetRailtypeSpriteOffset();

    if (image != SPR_FLAT_GRASS_TILE) image += offset;
    // PaletteID palette = COMPANY_SPRITE_COLOUR(_local_company);

    // DrawSprite(image, PAL_NONE, x, y);

    switch (ddir) {
        case DIAGDIR_SW: DrawAutorailSelection(ti, HT_DIR_X, GetSelectionColourByTint(palette)); break;
        case DIAGDIR_SE: DrawAutorailSelection(ti, HT_DIR_Y, GetSelectionColourByTint(palette)); break;
        default: break;
    }
    // if (rti->UsesOverlay()) {
    //     SpriteID ground = GetCustomRailSprite(rti, INVALID_TILE, RTSG_GROUND);

    //     switch (ddir) {
    //         case DIAGDIR_SW: DrawSprite(ground + RTO_X, PALETTE_TINT_WHITE, x, y); break;
    //         case DIAGDIR_SE: DrawSprite(ground + RTO_Y, PALETTE_TINT_WHITE, x, y); break;
    //         default: break;
    //     }
    // }
    int depot_sprite = GetCustomRailSprite(rti, INVALID_TILE, RTSG_DEPOT);
    if (depot_sprite != 0) offset = depot_sprite - SPR_RAIL_DEPOT_SE_1;

    DrawRailTileSeq(ti, dts, TO_INVALID, offset, 0, palette);
}

void DrawTrainStationSprite(SpriteID palette, const TileInfo *ti, RailType railtype, Axis axis, byte section) {
    int32 total_offset = 0;
    PaletteID pal = COMPANY_SPRITE_COLOUR(_local_company);
    const DrawTileSprites *t = GetStationTileLayout(STATION_RAIL, section + (axis == AXIS_X ? 0 : 1));
    const RailtypeInfo *rti = nullptr;

    if (railtype != INVALID_RAILTYPE) {
        rti = GetRailTypeInfo(railtype);
        total_offset = rti->GetRailtypeSpriteOffset();
    }

    DrawAutorailSelection(ti, (axis == AXIS_X ? HT_DIR_X : HT_DIR_Y), GetSelectionColourByTint(palette));

    // if (roadtype != INVALID_ROADTYPE) {
    //     const RoadTypeInfo* rti = GetRoadTypeInfo(roadtype);
    //     if (image >= 4) {
    //         /* Drive-through stop */
    //         uint sprite_offset = 5 - image;

    //         /* Road underlay takes precedence over tram */
    //         if (rti->UsesOverlay()) {
    //             SpriteID ground = GetCustomRoadSprite(rti, INVALID_TILE, ROTSG_GROUND);
    //             DrawSprite(ground + sprite_offset, PAL_NONE, x, y);

    //             SpriteID overlay = GetCustomRoadSprite(rti, INVALID_TILE, ROTSG_OVERLAY);
    //             if (overlay) DrawSprite(overlay + sprite_offset, PAL_NONE, x, y);
    //         } else if (RoadTypeIsTram(roadtype)) {
    //             DrawSprite(SPR_TRAMWAY_TRAM + sprite_offset, PAL_NONE, x, y);
    //         }
    //     } else {
    //         /* Drive-in stop */
    //         if (RoadTypeIsRoad(roadtype) && rti->UsesOverlay()) {
    //             SpriteID ground = GetCustomRoadSprite(rti, INVALID_TILE, ROTSG_ROADSTOP);
    //             DrawSprite(ground + image, PAL_NONE, x, y);
    //         }
    //     }
    // }

    /* Default waypoint has no railtype specific sprites */
    // DrawRailTileSeq(ti, t, TO_INVALID, (st == STATION_WAYPOINT ? 0 : total_offset), 0, PALETTE_TINT_WHITE);
    DrawRailTileSeq(ti, t, TO_INVALID, total_offset, 0, palette);
}

void DrawRoadStop(SpriteID palette, const TileInfo *ti, RoadType roadtype, DiagDirection orientation, bool is_truck) {
    int32 total_offset = 0;
    const RoadTypeInfo* rti = GetRoadTypeInfo(roadtype);

    uint image = (uint)orientation;
    if (image >= 4) {
        /* Drive-through stop */
        uint sprite_offset = 5 - image;

        /* Road underlay takes precedence over tram */
        if (rti->UsesOverlay()) {
            SpriteID ground = GetCustomRoadSprite(rti, INVALID_TILE, ROTSG_GROUND);
            DrawSprite(ground + sprite_offset, PAL_NONE, ti->x, ti->y);

            SpriteID overlay = GetCustomRoadSprite(rti, INVALID_TILE, ROTSG_OVERLAY);
            // if (overlay) DrawSprite(overlay + sprite_offset, PAL_NONE, x, y);
            if (overlay) AddSortableSpriteToDraw(overlay + sprite_offset, palette, ti->x, ti->y, 1, 1, BB_HEIGHT_UNDER_BRIDGE, ti->z);
        } else if (RoadTypeIsTram(roadtype)) {
            // DrawSprite(SPR_TRAMWAY_TRAM + sprite_offset, PAL_NONE, x, y);
            AddSortableSpriteToDraw(SPR_TRAMWAY_TRAM + sprite_offset, palette, ti->x, ti->y, 1, 1, BB_HEIGHT_UNDER_BRIDGE, ti->z);
        }
    } else {
        /* Drive-in stop */
        if (RoadTypeIsRoad(roadtype) && rti->UsesOverlay()) {
            SpriteID ground = GetCustomRoadSprite(rti, INVALID_TILE, ROTSG_ROADSTOP);
            // DrawSprite(, PAL_NONE, x, y);
            AddSortableSpriteToDraw(ground + image, palette, ti->x, ti->y, 1, 1, BB_HEIGHT_UNDER_BRIDGE, ti->z);
        }
    }

    const DrawTileSprites *t = GetStationTileLayout(is_truck ? STATION_TRUCK : STATION_BUS, image);
    AddSortableSpriteToDraw(t->ground.sprite, palette, ti->x, ti->y, 1, 1, BB_HEIGHT_UNDER_BRIDGE, ti->z);
    DrawRailTileSeq(ti, t, TO_INVALID, total_offset, 0, palette);
    /* Draw road, tram catenary */
    // DrawRoadCatenary(ti);
}


struct DrawRoadTileStruct {
    uint16 image;
    byte subcoord_x;
    byte subcoord_y;
};

#include "../table/road_land.h"

// copied from road_gui.cpp
static uint GetRoadSpriteOffset(Slope slope, RoadBits bits)
{
    if (slope != SLOPE_FLAT) {
        switch (slope) {
            case SLOPE_NE: return 11;
            case SLOPE_SE: return 12;
            case SLOPE_SW: return 13;
            case SLOPE_NW: return 14;
            default: NOT_REACHED();
        }
    } else {
        static const uint offsets[] = {
            0, 18, 17, 7,
            16, 0, 10, 5,
            15, 8, 1, 4,
            9, 3, 6, 2
        };
        return offsets[bits];
    }
}


void DrawRoadDepot(SpriteID palette, const TileInfo *ti, RoadType roadtype, DiagDirection orientation) {
    const RoadTypeInfo* rti = GetRoadTypeInfo(roadtype);
    int relocation = GetCustomRoadSprite(rti, INVALID_TILE, ROTSG_DEPOT);
    bool default_gfx = relocation == 0;
    if (default_gfx) {
        if (HasBit(rti->flags, ROTF_CATENARY)) {
            if (_loaded_newgrf_features.tram == TRAMWAY_REPLACE_DEPOT_WITH_TRACK && RoadTypeIsTram(roadtype) && !rti->UsesOverlay()) {
                /* Sprites with track only work for default tram */
                relocation = SPR_TRAMWAY_DEPOT_WITH_TRACK - SPR_ROAD_DEPOT;
                default_gfx = false;
            } else {
                /* Sprites without track are always better, if provided */
                relocation = SPR_TRAMWAY_DEPOT_NO_TRACK - SPR_ROAD_DEPOT;
            }
        }
    } else {
        relocation -= SPR_ROAD_DEPOT;
    }

    const DrawTileSprites *dts = &_road_depot[orientation];
    AddSortableSpriteToDraw(dts->ground.sprite, palette, ti->x, ti->y, 1, 1, BB_HEIGHT_UNDER_BRIDGE, ti->z);

    if (default_gfx) {
        uint offset = GetRoadSpriteOffset(SLOPE_FLAT, DiagDirToRoadBits(orientation));
        if (rti->UsesOverlay()) {
            SpriteID ground = GetCustomRoadSprite(rti, INVALID_TILE, ROTSG_OVERLAY);
            if (ground != 0) AddSortableSpriteToDraw(ground + offset, palette, ti->x, ti->y, 1, 1, BB_HEIGHT_UNDER_BRIDGE, ti->z);
        } else if (RoadTypeIsTram(roadtype)) {
            AddSortableSpriteToDraw(SPR_TRAMWAY_OVERLAY + offset, palette, ti->x, ti->y, 1, 1, BB_HEIGHT_UNDER_BRIDGE, ti->z);
        }
    }

    DrawRailTileSeq(ti, dts, TO_INVALID, relocation, 0, palette);
}

#include "../table/station_land.h"

void DrawAirportTile(SpriteID palette, const TileInfo *ti, StationGfx gfx) {
    int32 total_offset = 0;
    const DrawTileSprites *t = nullptr;
    gfx = GetTranslatedAirportTileID(gfx);
    if (gfx >= NEW_AIRPORTTILE_OFFSET) {
        const AirportTileSpec *ats = AirportTileSpec::Get(gfx);
        if (ats->grf_prop.spritegroup[0] != nullptr /* && DrawNewAirportTile(ti, Station::GetByTile(ti->tile), gfx, ats) */) {
            return;
        }
        /* No sprite group (or no valid one) found, meaning no graphics associated.
         * Use the substitute one instead */
        assert(ats->grf_prop.subst_id != INVALID_AIRPORTTILE);
        gfx = ats->grf_prop.subst_id;
    }
    switch (gfx) {
        case APT_RADAR_GRASS_FENCE_SW:
            t = &_station_display_datas_airport_radar_grass_fence_sw[0];
            break;
        case APT_GRASS_FENCE_NE_FLAG:
            t = &_station_display_datas_airport_flag_grass_fence_ne[0];
            break;
        case APT_RADAR_FENCE_SW:
            t = &_station_display_datas_airport_radar_fence_sw[0];
            break;
        case APT_RADAR_FENCE_NE:
            t = &_station_display_datas_airport_radar_fence_ne[0];
            break;
        case APT_GRASS_FENCE_NE_FLAG_2:
            t = &_station_display_datas_airport_flag_grass_fence_ne_2[0];
            break;
    }
    if (t == nullptr || t->seq == nullptr) t = GetStationTileLayout(STATION_AIRPORT, gfx);
    if (t) {
        AddSortableSpriteToDraw(t->ground.sprite, palette, ti->x, ti->y, 1, 1, BB_HEIGHT_UNDER_BRIDGE, ti->z);
        DrawRailTileSeq(ti, t, TO_INVALID, total_offset, 0, palette);
    }
}

enum SignalOffsets {  // from rail_cmd.cpp
    SIGNAL_TO_SOUTHWEST,
    SIGNAL_TO_NORTHEAST,
    SIGNAL_TO_SOUTHEAST,
    SIGNAL_TO_NORTHWEST,
    SIGNAL_TO_EAST,
    SIGNAL_TO_WEST,
    SIGNAL_TO_SOUTH,
    SIGNAL_TO_NORTH,
};

/**
 * copied from rail_cmd.cpp
 * Get surface height in point (x,y)
 * On tiles with halftile foundations move (x,y) to a safe point wrt. track
 */
static uint GetSaveSlopeZ(uint x, uint y, Track track)
{
    switch (track) {
        case TRACK_UPPER: x &= ~0xF; y &= ~0xF; break;
        case TRACK_LOWER: x |=  0xF; y |=  0xF; break;
        case TRACK_LEFT:  x |=  0xF; y &= ~0xF; break;
        case TRACK_RIGHT: x &= ~0xF; y |=  0xF; break;
        default: break;
    }
    return GetSlopePixelZ(x, y);
}

void DrawSignal(const TileInfo *ti, RailType railtype, uint pos, SignalType type, SignalVariant variant) {
    // reference: DraawSingleSignal in rail_cmd.cpp
    bool side;
    switch (_settings_game.construction.train_signal_side) {
        case 0:  side = false;                                 break; // left
        case 2:  side = true;                                  break; // right
        default: side = _settings_game.vehicle.road_side != 0; break; // driving side
    }
    static const Point SignalPositions[2][12] = {
        { // Signals on the left side
        /*  LEFT      LEFT      RIGHT     RIGHT     UPPER     UPPER */
            { 8,  5}, {14,  1}, { 1, 14}, { 9, 11}, { 1,  0}, { 3, 10},
        /*  LOWER     LOWER     X         X         Y         Y     */
            {11,  4}, {14, 14}, {11,  3}, { 4, 13}, { 3,  4}, {11, 13}
        }, { // Signals on the right side
        /*  LEFT      LEFT      RIGHT     RIGHT     UPPER     UPPER */
            {14,  1}, {12, 10}, { 4,  6}, { 1, 14}, {10,  4}, { 0,  1},
        /*  LOWER     LOWER     X         X         Y         Y     */
            {14, 14}, { 5, 12}, {11, 13}, { 4,  3}, {13,  4}, { 3, 11}
        }
    };

    uint x = TileX(ti->tile) * TILE_SIZE + SignalPositions[side][pos].x;
    uint y = TileY(ti->tile) * TILE_SIZE + SignalPositions[side][pos].y;

    static const Track pos_track[] = {
        TRACK_LEFT, TRACK_LEFT, TRACK_RIGHT, TRACK_RIGHT,
        TRACK_UPPER, TRACK_UPPER, TRACK_LOWER, TRACK_LOWER,
        TRACK_X, TRACK_X, TRACK_Y, TRACK_Y,
    };
    static const SignalOffsets pos_offset[] = {
        SIGNAL_TO_NORTH, SIGNAL_TO_SOUTH, SIGNAL_TO_NORTH, SIGNAL_TO_SOUTH,
        SIGNAL_TO_WEST, SIGNAL_TO_EAST, SIGNAL_TO_WEST, SIGNAL_TO_EAST,
        SIGNAL_TO_SOUTHWEST, SIGNAL_TO_NORTHEAST, SIGNAL_TO_SOUTHEAST, SIGNAL_TO_NORTHWEST,
    };

    auto track = pos_track[pos];
    auto image = pos_offset[pos];
    static const SignalState condition = SIGNAL_STATE_GREEN;

    auto rti = GetRailTypeInfo(railtype);
    SpriteID sprite = GetCustomSignalSprite(rti, ti->tile, type, variant, condition);
    if (sprite != 0) {
        sprite += image;
    } else {
        /* Normal electric signals are stored in a different sprite block than all other signals. */
        sprite = (type == SIGTYPE_NORMAL && variant == SIG_ELECTRIC) ? SPR_ORIGINAL_SIGNALS_BASE : SPR_SIGNALS_BASE - 16;
        sprite += type * 16 + variant * 64 + image * 2 + condition + (type > SIGTYPE_LAST_NOPBS ? 64 : 0);
    }

    AddSortableSpriteToDraw(sprite, PALETTE_TINT_WHITE, x, y, 1, 1, BB_HEIGHT_UNDER_BRIDGE, GetSaveSlopeZ(x, y, track));
}

// copied from tunnelbridge_cmd.cpp
static inline const PalSpriteID *GetBridgeSpriteTable(int index, BridgePieces table)
{
    const BridgeSpec *bridge = GetBridgeSpec(index);
    assert(table < BRIDGE_PIECE_INVALID);
    if (bridge->sprite_table == nullptr || bridge->sprite_table[table] == nullptr) {
        return _bridge_sprite_table[index][table];
    } else {
        return bridge->sprite_table[table];
    }
}

void DrawBridgeHead(const TileInfo *ti, RailType railtype, DiagDirection ddir, BridgeType type) {
    auto rti = GetRailTypeInfo(railtype);
    int base_offset = rti->bridge_offset;
    const PalSpriteID *psid;

    /* HACK Wizardry to convert the bridge ramp direction into a sprite offset */
    base_offset += (6 - ddir) % 4;

    /* Table number BRIDGE_PIECE_HEAD always refers to the bridge heads for any bridge type */
    if (ti->tileh == SLOPE_FLAT) base_offset += 4; // sloped bridge head
    psid = &GetBridgeSpriteTable(type, BRIDGE_PIECE_HEAD)[base_offset];

    AddSortableSpriteToDraw(psid->sprite, PALETTE_TINT_WHITE, ti->x, ti->y, 16, 16, ti->tileh == SLOPE_FLAT ? 0 : 8, ti->z);
    // DrawAutorailSelection(ti, (ddir == DIAGDIR_SW || ddir == DIAGDIR_NE ? HT_DIR_X : HT_DIR_Y), PAL_NONE);
}

void DrawTunnelHead(const TileInfo *ti, RailType railtype, DiagDirection ddir) {
    auto rti = GetRailTypeInfo(railtype);

    SpriteID image;
    SpriteID railtype_overlay = 0;

    image = rti->base_sprites.tunnel;
    if (rti->UsesOverlay()) {
        /* Check if the railtype has custom tunnel portals. */
        railtype_overlay = GetCustomRailSprite(rti, ti->tile, RTSG_TUNNEL_PORTAL);
        if (railtype_overlay != 0) image = SPR_RAILTYPE_TUNNEL_BASE; // Draw blank grass tunnel base.
    }

    image += ddir * 2;
    AddSortableSpriteToDraw(image, PALETTE_TINT_WHITE, ti->x, ti->y, 16, 16, 0, ti->z);
}

void ObjectHighlight::Draw(const TileInfo *ti) {
    auto range = this->tiles.equal_range(ti->tile);
    for (auto t = range.first; t != range.second; t++) {
        auto &oth = t->second;
        switch (oth.type) {
            case ObjectTileHighlight::Type::RAIL_DEPOT:
                DrawTrainDepotSprite(oth.palette, ti, _cur_railtype, oth.u.rail.depot.ddir);
                break;
            case ObjectTileHighlight::Type::RAIL_TRACK: {
                auto hs = (HighLightStyle)oth.u.rail.track;
                DrawAutorailSelection(ti, hs, PAL_NONE);
                break;
            }
            case ObjectTileHighlight::Type::RAIL_STATION:
                DrawTrainStationSprite(oth.palette, ti, _cur_railtype, oth.u.rail.station.axis, oth.u.rail.station.section);
                break;
            case ObjectTileHighlight::Type::RAIL_SIGNAL:
                DrawSignal(ti, _cur_railtype, oth.u.rail.signal.pos, oth.u.rail.signal.type, oth.u.rail.signal.variant);
                break;
            case ObjectTileHighlight::Type::RAIL_BRIDGE_HEAD:
                DrawBridgeHead(ti, _cur_railtype, oth.u.rail.bridge_head.ddir, oth.u.rail.bridge_head.type);
                break;
            case ObjectTileHighlight::Type::RAIL_TUNNEL_HEAD:
                DrawTunnelHead(ti, _cur_railtype, oth.u.rail.tunnel_head.ddir);
                break;
            case ObjectTileHighlight::Type::ROAD_STOP:
                DrawRoadStop(oth.palette, ti, oth.u.road.stop.roadtype, oth.u.road.stop.ddir, oth.u.road.stop.is_truck);
                break;
            case ObjectTileHighlight::Type::ROAD_DEPOT:
                DrawRoadDepot(oth.palette, ti, oth.u.road.depot.roadtype, oth.u.road.depot.ddir);
                break;
            case ObjectTileHighlight::Type::AIRPORT_TILE:
                DrawAirportTile(oth.palette, ti, oth.u.airport_tile.gfx);
                break;
            default:
                break;
        }
    }
    // fprintf(stderr, "TILEH DRAW %d %d %d\n", ti->tile, (int)i, (int)this->tiles.size());
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

static uint8 _industry_highlight_hash = 0;

void UpdateIndustryHighlight() {
    _industry_highlight_hash++;
}

bool CanBuildIndustryOnTileCached(IndustryType type, TileIndex tile) {
    // if (_mz[tile].industry_fund_type != type || !_mz[tile].industry_fund_result) {
    if (_mz[tile].industry_fund_update != _industry_highlight_hash || !_mz[tile].industry_fund_result) {
        bool res = CanBuildIndustryOnTile(type, tile);
        // _mz[tile].industry_fund_type = type;
        _mz[tile].industry_fund_update = _industry_highlight_hash;
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

static void SetStationSelectionHighlight(const TileInfo *ti, TileHighlight &th) {
    bool draw_selection = ((_thd.drawstyle & HT_DRAG_MASK) == HT_RECT && _thd.outersize.x > 0);
    const Station *highlight_station = _viewport_highlight_station;

    if (_highlight_station_to_join) highlight_station = _highlight_station_to_join;

    if (draw_selection) {
        // const SpriteID pal[] = {SPR_PALETTE_ZONING_RED, SPR_PALETTE_ZONING_YELLOW, SPR_PALETTE_ZONING_LIGHT_BLUE, SPR_PALETTE_ZONING_GREEN};
        // auto color = pal[(int)_station_building_status];
        // if (_thd.make_square_red) color = SPR_PALETTE_ZONING_RED;
        if (_thd.make_square_red) {
            auto b = CalcTileBorders(ti->tile, [](TileIndex t) {
                auto x = TileX(t) * TILE_SIZE, y = TileY(t) * TILE_SIZE;
                return IsInsideSelectedRectangle(x, y);
            });
            if (b.first != ZoningBorder::NONE)
                th.add_border(b.first, SPR_PALETTE_ZONING_RED);
        }
        if (IsInsideSelectedRectangle(TileX(ti->tile) * TILE_SIZE, TileY(ti->tile) * TILE_SIZE)) {
            // th.ground_pal = GetTintBySelectionColour(color);
            th.ground_pal = th.structure_pal = (_thd.make_square_red ? PALETTE_TINT_RED : PAL_NONE);
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
        (uint16)std::max<int>(0, tx - radius),
        (uint16)std::max<int>(0, ty - radius),
        (uint16)std::min<int>(tx + radius + 1, MapSizeX()),
        (uint16)std::min<int>(ty + radius + 1, MapSizeY()),
        [tx, ty, radius, &in_zone, &border] (TownID tid) {
            Town *town = Town::GetIfValid(tid);
            if (!town || town->larger_town)
                return;

            int dx = TileX(town->xy) - tx;
            int dy = TileY(town->xy) - ty;
            in_zone = in_zone || (std::max(abs(dx), abs(dy)) <= radius);
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
        (uint16)std::max<int>(0, tx - radius),
        (uint16)std::max<int>(0, ty - radius),
        (uint16)std::min<int>(tx + radius + 1, MapSizeX()),
        (uint16)std::min<int>(ty + radius + 1, MapSizeY()),
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
    // SetBlueprintHighlight(ti, th);

    return th;
}

void DrawTileZoning(const TileInfo *ti, const TileHighlight &th) {
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

bool DrawTileSelection(const TileInfo *ti, const TileHighlightType &tht) {
    _thd.cm.Draw(ti);

    // if (_thd.drawstyle == CM_HT_BLUEPRINT_PLACE) return true;

    if (_thd.select_proc == DDSP_BUILD_STATION || _thd.select_proc == DDSP_BUILD_BUSSTOP
        || _thd.select_proc == DDSP_BUILD_TRUCKSTOP || _thd.select_proc == CM_DDSP_BUILD_AIRPORT
        || _thd.select_proc == CM_DDSP_BUILD_ROAD_DEPOT || _thd.select_proc == CM_DDSP_BUILD_RAIL_DEPOT) {
        // handled by DrawTileZoning
        return true;
    }

    return false;
}

TileIndex _autodetection_tile = INVALID_TILE;
DiagDirDiff _autodetection_rotation = DIAGDIRDIFF_SAME;

static DiagDirDiff GetAutodetectionRotation() {
    auto pt = GetTileBelowCursor();
    auto tile = TileVirtXY(pt.x, pt.y);

    if (tile != _autodetection_tile) {
        _autodetection_tile = tile;
        _autodetection_rotation = DIAGDIRDIFF_SAME;
    }

    return _autodetection_rotation;
}

void RotateAutodetection() {
    auto rotation = GetAutodetectionRotation();
    if (rotation == DIAGDIRDIFF_90LEFT) rotation = DIAGDIRDIFF_SAME;
    else rotation++;
    _autodetection_rotation = rotation;
    ::UpdateTileSelection();
}

void ResetRotateAutodetection() {
    _autodetection_tile = INVALID_TILE;
    _autodetection_rotation = DIAGDIRDIFF_SAME;
}

DiagDirection AddAutodetectionRotation(DiagDirection ddir) {
    if (ddir >= DIAGDIR_END) return (DiagDirection)(((uint)ddir + (uint)GetAutodetectionRotation()) % 2 + DIAGDIR_END);
    return ChangeDiagDir(ddir, GetAutodetectionRotation());
}

HighLightStyle UpdateTileSelection(HighLightStyle new_drawstyle) {
    _thd.cm_new = ObjectHighlight(ObjectHighlight::Type::NONE);
    auto pt = GetTileBelowCursor();
    auto tile = (pt.x == -1 ? INVALID_TILE : TileVirtXY(pt.x, pt.y));
    // fprintf(stderr, "UPDATE %d %d %d %d\n", tile, _thd.size.x, _thd.size.y, (int)((_thd.place_mode & HT_DRAG_MASK) == HT_RECT));
    // if (_thd.place_mode == CM_HT_BLUEPRINT_PLACE) {
    //     UpdateBlueprintTileSelection(pt, tile);
    //     new_drawstyle = CM_HT_BLUEPRINT_PLACE;
    // } else
    if (pt.x == -1) {
    } else if (_thd.make_square_red) {
    } else if (_thd.select_proc == CM_DDSP_BUILD_ROAD_DEPOT) {
        auto dir = _road_depot_orientation;
        if (dir == DEPOTDIR_AUTO) {
            dir = AddAutodetectionRotation(AutodetectRoadObjectDirection(tile, pt, _cur_roadtype));
        }
        _thd.cm_new = ObjectHighlight::make_road_depot(tile, _cur_roadtype, dir);
        new_drawstyle = HT_RECT;
    } else if (_thd.select_proc == CM_DDSP_BUILD_RAIL_DEPOT) {
        auto dir = _build_depot_direction;
        if (dir >= DiagDirection::DIAGDIR_END) {
            dir = AddAutodetectionRotation(AutodetectRailObjectDirection(tile, pt));
        }
        _thd.cm_new = ObjectHighlight::make_rail_depot(tile, dir);
    // } else if (((_thd.place_mode & HT_DRAG_MASK) == HT_RECT || ((_thd.place_mode & HT_DRAG_MASK) == HT_SPECIAL && (_thd.next_drawstyle & HT_DRAG_MASK) == HT_RECT)) && _thd.new_outersize.x > 0 && !_thd.make_square_red) {  // station
    } else if (_thd.select_proc == CM_DDSP_BUILD_AIRPORT) {
        auto tile = TileXY(_thd.new_pos.x / TILE_SIZE, _thd.new_pos.y / TILE_SIZE);
        if (_selected_airport_index != -1) {
            const AirportSpec *as = AirportClass::Get(_selected_airport_class)->GetSpec(_selected_airport_index);
            _thd.cm_new = ObjectHighlight::make_airport(tile, as->GetIndex(), _selected_airport_layout);
            new_drawstyle = HT_RECT;
        }
    } else if (_thd.select_proc == DDSP_BUILD_STATION || _thd.select_proc == DDSP_BUILD_BUSSTOP
               || _thd.select_proc == DDSP_BUILD_TRUCKSTOP) {  // station
        if (_thd.size.x >= (int)TILE_SIZE && _thd.size.y >= (int)TILE_SIZE) {
            auto start_tile = TileXY(_thd.new_pos.x / TILE_SIZE, _thd.new_pos.y / TILE_SIZE);
            auto end_tile = TileXY(
                std::min((_thd.new_pos.x + _thd.new_size.x) / TILE_SIZE, MapSizeX()) - 1,
                std::min((_thd.new_pos.y + _thd.new_size.y) / TILE_SIZE, MapSizeY()) - 1
            );
            if (_thd.select_proc == DDSP_BUILD_STATION)
                _thd.cm_new = ObjectHighlight::make_rail_station(start_tile, end_tile, _railstation.orientation);
            else if (_thd.select_proc == DDSP_BUILD_BUSSTOP || _thd.select_proc == DDSP_BUILD_TRUCKSTOP) {
                auto ddir = _road_station_picker_orientation;
                auto ta = TileArea(start_tile, end_tile);
                if (pt.x != -1) {
                    if (ddir >= DIAGDIR_END && ddir < STATIONDIR_AUTO) {
                        // When placed on road autorotate anyway
                        if (ddir == STATIONDIR_X) {
                            if (!CheckDriveThroughRoadStopDirection(ta, ROAD_X))
                                ddir = STATIONDIR_Y;
                        } else {
                            if (!CheckDriveThroughRoadStopDirection(ta, ROAD_Y))
                                ddir = STATIONDIR_X;
                        }
                    } else if (ddir == STATIONDIR_AUTO) {
                        ddir = AddAutodetectionRotation(AutodetectRoadObjectDirection(start_tile, pt, _cur_roadtype));
                    } else if (ddir == STATIONDIR_AUTO_XY) {
                        ddir = AddAutodetectionRotation(AutodetectDriveThroughRoadStopDirection(ta, pt, _cur_roadtype));
                    }
                }
                _thd.cm_new = ObjectHighlight::make_road_stop(start_tile, end_tile, _cur_roadtype, ddir, _thd.select_proc == DDSP_BUILD_TRUCKSTOP);
            }
        }
        new_drawstyle = HT_RECT;
    }
    if (_thd.cm != _thd.cm_new) {
        _thd.cm.MarkDirty();
        _thd.cm = _thd.cm_new;
        _thd.cm.MarkDirty();
    }
    return new_drawstyle;
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
    for(TileIndex tile : area) {
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
    x1 = (uint16)std::max<int>(0, TileX(center) - radius);
    x2 = (uint16)std::min<int>(TileX(center) + radius + 1, MapSizeX());
    y1 = (uint16)std::max<int>(0, TileY(center) - radius);
    y2 = (uint16)std::min<int>(TileY(center) + radius + 1, MapSizeY());
    for (uint16 y = y1; y < y2; y++) {
        for (uint16 x = x1; x < x2; x++) {
            auto tile = TileXY(x, y);
            if (DistanceManhattan(tile, center) > radius) continue;
            _mz[tile].advertisement_zone = std::max(_mz[tile].advertisement_zone, zone);
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
    UpdateIndustryHighlight();
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
