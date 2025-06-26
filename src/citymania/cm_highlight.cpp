#include "../stdafx.h"

#include "cm_highlight.hpp"

#include "cm_blueprint.hpp"
#include "cm_commands.hpp"
#include "cm_highlight_type.hpp"
#include "cm_main.hpp"
#include "cm_overlays.hpp"
#include "cm_station_gui.hpp"
#include "cm_type.hpp"
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
#include "../newgrf_industrytiles.h"
#include "../sound_func.h"
#include "../spritecache.h"
#include "../strings_func.h"
#include "../town.h"
#include "../town_kdtree.h"
#include "../tilearea_type.h"
#include "../tilehighlight_type.h"
#include "../tilehighlight_func.h"
#include "../viewport_func.h"
#include "../window_gui.h"
#include "../window_func.h"
#include "../zoom_func.h"
// #include "../zoning.h"
#include "../table/airporttile_ids.h"
#include "../table/animcursors.h"
#include "../table/track_land.h"
#include "../table/autorail.h"
#include "../table/industry_land.h"
#include "../debug.h"
#include "generated/cm_gen_commands.hpp"
#include "station_gui.h"
#include "station_type.h"
#include "table/sprites.h"
#include "table/strings.h"
#include "tile_type.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <set>

/** Enumeration of multi-part foundations */
enum FoundationPart {
    FOUNDATION_PART_NONE     = 0xFF,  ///< Neither foundation nor groundsprite drawn yet.
    FOUNDATION_PART_NORMAL   = 0,     ///< First part (normal foundation or no foundation)
    FOUNDATION_PART_HALFTILE = 1,     ///< Second part (halftile foundation)
    FOUNDATION_PART_END
};
extern void DrawSelectionSprite(SpriteID image, PaletteID pal, const TileInfo *ti, int z_offset, FoundationPart foundation_part, int extra_offs_x = 0, int extra_offs_y = 0); // viewport.cpp
extern const Station *_viewport_highlight_station;
extern TileHighlightData _thd;
extern bool IsInsideSelectedRectangle(int x, int y);
extern RailType _cur_railtype;
extern RoadType _cur_roadtype;
extern AirportClassID _selected_airport_class; ///< the currently visible airport class
extern int _selected_airport_index;
extern byte _selected_airport_layout;
extern DiagDirection _build_depot_direction; ///< Currently selected depot direction
extern DiagDirection _road_depot_orientation;
extern uint32 _realtime_tick;
extern uint32 _cm_funding_layout;
extern IndustryType _cm_funding_type;
extern void GetStationLayout(byte *layout, uint numtracks, uint plat_len, const StationSpec *statspec);
extern void IndustryDrawTileLayout(const TileInfo *ti, const TileLayoutSpriteGroup *group, byte rnd_colour, byte stage);
extern void SetSelectionTilesDirty();

struct RailStationGUISettings {
    Axis orientation;                 ///< Currently selected rail station orientation

    bool newstations;                 ///< Are custom station definitions available?
    StationClassID station_class;     ///< Currently selected custom station class (if newstations is \c true )
    byte station_type;                ///< %Station type within the currently selected custom station class (if newstations is \c true )
    byte station_count;               ///< Number of custom stations (if newstations is \c true )
};
extern RailStationGUISettings _railstation; ///< Settings of the station builder GUI

struct RoadStopGUISettings {
    DiagDirection orientation;

    RoadStopClassID roadstop_class;
    uint16_t roadstop_type;
    uint16_t roadstop_count;
};
extern RoadStopGUISettings _roadstop_gui_settings;


template <>
struct std::hash<citymania::ObjectTileHighlight> {
    std::size_t operator()(const citymania::ObjectTileHighlight &oh) const {
        std::size_t h = std::hash<SpriteID>()(oh.palette);
        h ^= hash<citymania::ObjectTileHighlight::Type>()(oh.type);
        switch (oh.type) {
            case citymania::ObjectTileHighlight::Type::RAIL_DEPOT:
                h ^= std::hash<DiagDirection>()(oh.u.rail.depot.ddir);
                break;
            case citymania::ObjectTileHighlight::Type::RAIL_TRACK:
                h ^= std::hash<Track>()(oh.u.rail.track);
                break;
            case citymania::ObjectTileHighlight::Type::RAIL_STATION:
                h ^= hash<Axis>()(oh.u.rail.station.axis);
                h ^= oh.u.rail.station.section;
                break;
            case citymania::ObjectTileHighlight::Type::RAIL_SIGNAL:
                h ^= hash<uint>()(oh.u.rail.signal.pos);
                h ^= hash<SignalType>()(oh.u.rail.signal.type);
                h ^= hash<SignalVariant>()(oh.u.rail.signal.variant);
                break;
            case citymania::ObjectTileHighlight::Type::RAIL_BRIDGE_HEAD:
                h ^= hash<DiagDirection>()(oh.u.rail.bridge_head.ddir);
                h ^= hash<uint32>()(oh.u.rail.bridge_head.other_end);
                h ^= hash<BridgeType>()(oh.u.rail.bridge_head.type);
                break;
            case citymania::ObjectTileHighlight::Type::RAIL_TUNNEL_HEAD:
                h ^= hash<DiagDirection>()(oh.u.rail.tunnel_head.ddir);
                break;
            case citymania::ObjectTileHighlight::Type::ROAD_STOP:
                h ^= hash<DiagDirection>()(oh.u.road.stop.ddir);
                h ^= hash<RoadType>()(oh.u.road.stop.roadtype);
                h ^= hash<bool>()(oh.u.road.stop.is_truck);
                h ^= hash<RoadStopClassID>()(oh.u.road.stop.spec_class);
                h ^= hash<uint16_t>()(oh.u.road.stop.spec_index);
                break;
            case citymania::ObjectTileHighlight::Type::ROAD_DEPOT:
                h ^= hash<DiagDirection>()(oh.u.road.depot.ddir);
                h ^= hash<RoadType>()(oh.u.road.depot.roadtype);
                break;
            case citymania::ObjectTileHighlight::Type::DOCK_SLOPE:
                h ^= hash<DiagDirection>()(oh.u.dock_slope.ddir);
                break;
            case citymania::ObjectTileHighlight::Type::DOCK_FLAT:
                h ^= hash<Axis>()(oh.u.dock_flat.axis);
                break;
            case citymania::ObjectTileHighlight::Type::AIRPORT_TILE:
                h ^= hash<StationGfx>()(oh.u.airport_tile.gfx);
                break;
            case citymania::ObjectTileHighlight::Type::INDUSTRY_TILE:
                h ^= hash<IndustryGfx>()(oh.u.industry_tile.gfx);
                h ^= hash<IndustryType>()(oh.u.industry_tile.ind_type);
                h ^= oh.u.industry_tile.ind_layout;
                h ^= hash<TileIndexDiff>()(oh.u.industry_tile.tile_diff);
                break;
            case citymania::ObjectTileHighlight::Type::NUMBERED_RECT:
                h ^= hash<uint32>()(oh.u.numbered_rect.number);
                break;
            case citymania::ObjectTileHighlight::Type::BORDER:
                h ^= hash<citymania::ZoningBorder>()(oh.u.border);
                break;
            case citymania::ObjectTileHighlight::Type::POINT:
            case citymania::ObjectTileHighlight::Type::RECT:
            case citymania::ObjectTileHighlight::Type::END:
            case citymania::ObjectTileHighlight::Type::TINT:
            case citymania::ObjectTileHighlight::Type::STRUCT_TINT:
                break;
        }
        return h;
    }
};

namespace citymania {
extern CargoArray GetProductionAroundTiles(TileIndex tile, int w, int h, int rad);

extern void (*DrawTileSelectionRect)(const TileInfo *ti, PaletteID pal);
extern void (*DrawAutorailSelection)(const TileInfo *ti, HighLightStyle autorail_type, PaletteID pal);
extern HighLightStyle (*GetPartOfAutoLine)(int px, int py, const Point &selstart, const Point &selend, HighLightStyle dir);

struct TileZoning {
    uint8 town_zone : 3;
    uint8 industry_fund_result : 2;
    uint8 advertisement_zone : 2;
    // IndustryType industry_fund_type;
    uint8 industry_fund_update;
};

static TileZoning *_mz = nullptr;
static IndustryType _industry_forbidden_tiles = INVALID_INDUSTRYTYPE;

extern const Station *_station_to_join;
extern const Station *_highlight_station_to_join;
extern TileArea _highlight_join_area;
extern bool _fn_mod;

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

// Copied from rail_cmd.cpp
static const TileIndexDiffC _trackdelta[] = {
    { -1,  0 }, {  0,  1 }, { -1,  0 }, {  0,  1 }, {  1,  0 }, {  0,  1 },
    {  0,  0 },
    {  0,  0 },
    {  1,  0 }, {  0, -1 }, {  0, -1 }, {  1,  0 }, {  0, -1 }, { -1,  0 },
    {  0,  0 },
    {  0,  0 }
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

ObjectTileHighlight ObjectTileHighlight::make_road_stop(SpriteID palette, RoadType roadtype, DiagDirection ddir, bool is_truck, RoadStopClassID spec_class, uint16_t spec_index) {
    auto oh = ObjectTileHighlight(Type::ROAD_STOP, palette);
    oh.u.road.stop.roadtype = roadtype;
    oh.u.road.stop.ddir = ddir;
    oh.u.road.stop.is_truck = is_truck;
    oh.u.road.stop.spec_class = spec_class;
    oh.u.road.stop.spec_index = spec_index;
    return oh;
}

ObjectTileHighlight ObjectTileHighlight::make_road_depot(SpriteID palette, RoadType roadtype, DiagDirection ddir) {
    auto oh = ObjectTileHighlight(Type::ROAD_DEPOT, palette);
    oh.u.road.depot.roadtype = roadtype;
    oh.u.road.depot.ddir = ddir;
    return oh;
}

ObjectTileHighlight ObjectTileHighlight::make_dock_slope(SpriteID palette, DiagDirection ddir) {
    auto oh = ObjectTileHighlight(Type::DOCK_SLOPE, palette);
    oh.u.dock_slope.ddir = ddir;
    return oh;
}

ObjectTileHighlight ObjectTileHighlight::make_dock_flat(SpriteID palette, Axis axis) {
    auto oh = ObjectTileHighlight(Type::DOCK_FLAT, palette);
    oh.u.dock_flat.axis = axis;
    return oh;
}

ObjectTileHighlight ObjectTileHighlight::make_airport_tile(SpriteID palette, StationGfx gfx) {
    auto oh = ObjectTileHighlight(Type::AIRPORT_TILE, palette);
    oh.u.airport_tile.gfx = gfx;
    return oh;
}

ObjectTileHighlight ObjectTileHighlight::make_industry_tile(SpriteID palette, IndustryType ind_type, byte ind_layout, TileIndexDiff tile_diff, IndustryGfx gfx) {
    auto oh = ObjectTileHighlight(Type::INDUSTRY_TILE, palette);
    oh.u.industry_tile.tile_diff = tile_diff;
    oh.u.industry_tile.gfx = gfx;
    oh.u.industry_tile.ind_type = ind_type;
    oh.u.industry_tile.ind_layout = ind_layout;
    return oh;
}

ObjectTileHighlight ObjectTileHighlight::make_point(SpriteID palette) {
    return ObjectTileHighlight(Type::POINT, palette);
}

ObjectTileHighlight ObjectTileHighlight::make_rect(SpriteID palette) {
    auto oh = ObjectTileHighlight(Type::RECT, palette);
    return oh;
}

ObjectTileHighlight ObjectTileHighlight::make_numbered_rect(SpriteID palette, uint32 number) {
    auto oh = ObjectTileHighlight(Type::NUMBERED_RECT, palette);
    oh.u.numbered_rect.number = number;
    return oh;
}

ObjectTileHighlight ObjectTileHighlight::make_border(SpriteID palette, ZoningBorder border) {
    auto oh = ObjectTileHighlight(Type::BORDER, palette);
    oh.u.border = border;
    return oh;
}

ObjectTileHighlight ObjectTileHighlight::make_tint(SpriteID palette) {
    auto oh = ObjectTileHighlight(Type::TINT, palette);
    return oh;
}

ObjectTileHighlight ObjectTileHighlight::make_struct_tint(SpriteID palette) {
    auto oh = ObjectTileHighlight(Type::STRUCT_TINT, palette);
    return oh;
}

bool ObjectTileHighlight::operator==(const ObjectTileHighlight &oh) const {
    if (this->type != oh.type) return false;
    if (this->palette != oh.palette) return false;
    switch (this->type) {
        case ObjectTileHighlight::Type::RAIL_DEPOT:
            return this->u.rail.depot.ddir == oh.u.rail.depot.ddir;
        case ObjectTileHighlight::Type::RAIL_TRACK:
            return this->u.rail.track == oh.u.rail.track;
        case ObjectTileHighlight::Type::RAIL_STATION:
            return this->u.rail.station.axis == oh.u.rail.station.axis
                && this->u.rail.station.section == oh.u.rail.station.section;
        case ObjectTileHighlight::Type::RAIL_SIGNAL:
            return this->u.rail.signal.pos == oh.u.rail.signal.pos
                && this->u.rail.signal.type == oh.u.rail.signal.type
                && this->u.rail.signal.variant == oh.u.rail.signal.variant;
        case ObjectTileHighlight::Type::RAIL_BRIDGE_HEAD:
            return this->u.rail.bridge_head.ddir == oh.u.rail.bridge_head.ddir
                && this->u.rail.bridge_head.other_end == oh.u.rail.bridge_head.other_end
                && this->u.rail.bridge_head.type == oh.u.rail.bridge_head.type;
        case ObjectTileHighlight::Type::RAIL_TUNNEL_HEAD:
            return this->u.rail.tunnel_head.ddir == oh.u.rail.tunnel_head.ddir;
        case ObjectTileHighlight::Type::ROAD_STOP:
            return this->u.road.stop.ddir == oh.u.road.stop.ddir
                && this->u.road.stop.roadtype == oh.u.road.stop.roadtype
                && this->u.road.stop.is_truck == oh.u.road.stop.is_truck
                && this->u.road.stop.spec_class == oh.u.road.stop.spec_class
                && this->u.road.stop.spec_index == oh.u.road.stop.spec_index;
        case ObjectTileHighlight::Type::ROAD_DEPOT:
            return this->u.road.depot.ddir == oh.u.road.depot.ddir
                && this->u.road.depot.roadtype == oh.u.road.depot.roadtype;
        case ObjectTileHighlight::Type::AIRPORT_TILE:
            return this->u.airport_tile.gfx == oh.u.airport_tile.gfx;
        case ObjectTileHighlight::Type::INDUSTRY_TILE:
            return this->u.industry_tile.gfx == oh.u.industry_tile.gfx
                && this->u.industry_tile.ind_type == oh.u.industry_tile.ind_type
                && this->u.industry_tile.ind_layout == oh.u.industry_tile.ind_layout
                && this->u.industry_tile.tile_diff == oh.u.industry_tile.tile_diff;
        case ObjectTileHighlight::Type::NUMBERED_RECT:
            return this->u.numbered_rect.number == oh.u.numbered_rect.number;
        case Type::BORDER:
            return this->u.border == oh.u.border;
        case ObjectTileHighlight::Type::DOCK_SLOPE:
            return this->u.dock_slope.ddir == oh.u.dock_slope.ddir;
        case ObjectTileHighlight::Type::DOCK_FLAT:
            return this->u.dock_flat.axis == oh.u.dock_flat.axis;
        case Type::POINT:
        case Type::RECT:
        case Type::END:
        case Type::TINT:
        case Type::STRUCT_TINT:
            return true;
    }
    return true;
}

bool ObjectTileHighlight::SetTileHighlight(TileHighlight &th, const TileInfo *) const {
    switch (this->type) {
        case ObjectTileHighlight::Type::RAIL_DEPOT:
        // case ObjectTileHighlight::Type::RAIL_TRACK:  Depot track shouldn't remove foundation
        case ObjectTileHighlight::Type::RAIL_STATION:
        case ObjectTileHighlight::Type::RAIL_SIGNAL:
        case ObjectTileHighlight::Type::RAIL_BRIDGE_HEAD:
        case ObjectTileHighlight::Type::RAIL_TUNNEL_HEAD:
        case ObjectTileHighlight::Type::ROAD_STOP:
        case ObjectTileHighlight::Type::ROAD_DEPOT:
        case ObjectTileHighlight::Type::AIRPORT_TILE:
        case ObjectTileHighlight::Type::INDUSTRY_TILE:
        case ObjectTileHighlight::Type::DOCK_SLOPE:
        case ObjectTileHighlight::Type::DOCK_FLAT:
            th.structure_pal = CM_PALETTE_HIDE_SPRITE;
            th.highlight_ground_pal = th.highlight_structure_pal = this->palette;
            return true;
        case ObjectTileHighlight::Type::TINT:
            th.ground_pal = th.structure_pal = this->palette;
            return true;
        case ObjectTileHighlight::Type::STRUCT_TINT:
            th.structure_pal = this->palette;
            return true;

        default:
            break;
    }
    return false;
}

bool ObjectHighlight::operator==(const ObjectHighlight& oh) const {
    if (this->type != oh.type) return false;
    return (this->tile == oh.tile
            && this->end_tile == oh.end_tile
            && this->trackdir == oh.trackdir
            && this->tile2 == oh.tile2
            && this->end_tile2 == oh.end_tile2
            && this->trackdir2 == oh.trackdir2
            && this->axis == oh.axis
            && this->ddir == oh.ddir
            && this->roadtype == oh.roadtype
            && this->is_truck == oh.is_truck
            && this->airport_type == oh.airport_type
            && this->airport_layout == oh.airport_layout
            && this->blueprint == oh.blueprint);
}


bool ObjectHighlight::operator!=(const ObjectHighlight& oh) const {
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

ObjectHighlight ObjectHighlight::make_road_stop(TileIndex start_tile, TileIndex end_tile, RoadType roadtype, DiagDirection orientation, bool is_truck, RoadStopClassID spec_class, uint16_t spec_index) {
    auto oh = ObjectHighlight{ObjectHighlight::Type::ROAD_STOP};
    oh.tile = start_tile;
    oh.end_tile = end_tile;
    oh.ddir = orientation;
    oh.roadtype = roadtype;
    oh.is_truck = is_truck;
    oh.road_stop_spec_class = spec_class;
    oh.road_stop_spec_index = spec_index;
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

ObjectHighlight ObjectHighlight::make_blueprint(TileIndex tile, sp<Blueprint> blueprint) {
    auto oh = ObjectHighlight{ObjectHighlight::Type::BLUEPRINT};
    oh.tile = tile;
    oh.blueprint = blueprint;
    return oh;
}

ObjectHighlight ObjectHighlight::make_polyrail(TileIndex start_tile, TileIndex end_tile, Trackdir trackdir,
                                               TileIndex start_tile2, TileIndex end_tile2, Trackdir trackdir2) {
    auto oh = ObjectHighlight{ObjectHighlight::Type::POLYRAIL};
    oh.tile = start_tile;
    oh.end_tile = end_tile;
    oh.trackdir = trackdir;
    oh.tile2 = start_tile2;
    oh.end_tile2 = end_tile2;
    oh.trackdir2 = trackdir2;
    return oh;
}

ObjectHighlight ObjectHighlight::make_industry(TileIndex tile, IndustryType ind_type, uint32 ind_layout) {
    auto oh = ObjectHighlight{ObjectHighlight::Type::INDUSTRY};
    oh.tile = tile;
    oh.ind_type = ind_type;
    oh.ind_layout = ind_layout;
    return oh;
}

ObjectHighlight ObjectHighlight::make_dock(TileIndex tile, DiagDirection orientation) {
	auto oh = ObjectHighlight{ObjectHighlight::Type::DOCK};
    oh.tile = tile;
    oh.ddir = orientation;
	return oh;
}

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

    this->AddTile(tile, ObjectTileHighlight::make_rail_track(CM_PALETTE_TINT_WHITE, track));
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

void ObjectHighlight::AddTile(TileIndex tile, ObjectTileHighlight &&oh) {
    if (tile >= Map::Size()) return;
    this->tiles.insert(std::make_pair(tile, std::move(oh)));
}

void ObjectHighlight::AddStationOverlayData(int w, int h, int rad, StationCoverageType sct) {
    if (!_settings_game.station.modified_catchment) rad = CA_UNMODIFIED;
    auto production = citymania::GetProductionAroundTiles(this->tile, w, h, rad);
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
            this->overlay_data.emplace_back(0, PAL_NONE, GetString(CM_STR_BUILD_INFO_OVERLAY_STATION_SUPPLIES));
            has_header = true;
        }
        SetDParam(0, i);
        SetDParam(1, production[i] >> 8);
        this->overlay_data.emplace_back(1, cs->GetCargoIcon(), GetString(CM_STR_BUILD_INFO_OVERLAY_STATION_CARGO));
    }
}

void ObjectHighlight::UpdateTiles() {
    this->tiles.clear();
    this->sprites.clear();
    this->cost = CMD_ERROR;
    switch (this->type) {
        case Type::NONE:
            break;

        case Type::RAIL_DEPOT: {
            auto dir = this->ddir;

            this->cost = cmd::BuildTrainDepot(
                this->tile,
                _cur_railtype,
                dir
            ).test();
            auto palette = (cost.Succeeded() ? CM_PALETTE_TINT_WHITE : CM_PALETTE_TINT_RED_DEEP);

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

            this->cost = cmd::BuildRailStation(
                this->tile,
                _cur_railtype,
                this->axis,
                numtracks,
                plat_len,
                _railstation.station_class,
                _railstation.station_type,
                NEW_STATION,
                true
            ).test();
            auto palette = (this->cost.Succeeded() ? CM_PALETTE_TINT_WHITE : CM_PALETTE_TINT_RED_DEEP);

            std::vector<byte> layouts(numtracks * plat_len);
            auto layout_ptr = layouts.data();
            GetStationLayout(layout_ptr, numtracks, plat_len, nullptr); // TODO statspec

            auto tile_delta = (this->axis == AXIS_X ? TileDiffXY(1, 0) : TileDiffXY(0, 1));
            TileIndex tile_track = this->tile;
            do {
                TileIndex tile = tile_track;
                int w = plat_len;
                do {
                    byte layout = *layout_ptr++;
                    this->AddTile(tile, ObjectTileHighlight::make_rail_station(palette, this->axis, layout & ~1));
                    tile += tile_delta;
                } while (--w);
                tile_track += tile_delta ^ TileDiffXY(1, 1); // perpendicular to tile_delta
            } while (--numtracks);

            this->AddStationOverlayData(ta.w, ta.h, CA_TRAIN, SCT_ALL);
            break;
        }
        case Type::ROAD_STOP: {
            auto ta = OrthogonalTileArea(this->tile, this->end_tile);
            this->cost = cmd::BuildRoadStop(
                this->tile,
                ta.w,
                ta.h,
                (this->is_truck ? ROADSTOP_TRUCK : ROADSTOP_BUS),
                (this->ddir >= DIAGDIR_END),  // is_drive_through
                (DiagDirection)(this->ddir % 4),
                this->roadtype,
                this->road_stop_spec_class,
                this->road_stop_spec_index,
                NEW_STATION,
                true
            ).test();
            auto palette = (this->cost.Succeeded() ? CM_PALETTE_TINT_WHITE : CM_PALETTE_TINT_RED_DEEP);
            for (TileIndex tile : ta) {
                this->AddTile(tile, ObjectTileHighlight::make_road_stop(palette, this->roadtype, this->ddir, this->is_truck, this->road_stop_spec_class, this->road_stop_spec_index));
            }
            auto sct = (this->is_truck ? SCT_NON_PASSENGERS_ONLY : SCT_PASSENGERS_ONLY);
            auto rad = (this->is_truck ? CA_BUS : CA_TRUCK);
            this->AddStationOverlayData(ta.w, ta.h, rad, sct);
            break;
        }

        case Type::ROAD_DEPOT: {
            this->cost = cmd::BuildRoadDepot(
                this->tile,
                this->roadtype,
                this->ddir
            ).test();
            auto palette = (this->cost.Succeeded() ? CM_PALETTE_TINT_WHITE : CM_PALETTE_TINT_RED_DEEP);
            this->AddTile(this->tile, ObjectTileHighlight::make_road_depot(palette, this->roadtype, this->ddir));
            break;
        }

        case Type::AIRPORT: {
            this->cost = cmd::BuildAirport(
                this->tile,
                this->airport_type,
                this->airport_layout,
                NEW_STATION,
                true
            ).test();
            auto palette = (this->cost.Succeeded() ? CM_PALETTE_TINT_WHITE : CM_PALETTE_TINT_RED_DEEP);

            const AirportSpec *as = AirportSpec::Get(this->airport_type);
            if (!as->IsAvailable() || this->airport_layout >= as->num_table) break;
            Direction rotation = as->rotation[this->airport_layout];
            int w = as->size_x;
            int h = as->size_y;
            if (rotation == DIR_E || rotation == DIR_W) Swap(w, h);
            for (AirportTileTableIterator iter(as->table[this->airport_layout], this->tile); iter != INVALID_TILE; ++iter) {
                this->AddTile(iter, ObjectTileHighlight::make_airport_tile(palette, iter.GetStationGfx()));
            }
            this->AddStationOverlayData(w, h, as->catchment, SCT_ALL);
            break;
        }
        case Type::BLUEPRINT:
            if (this->blueprint && this->tile != INVALID_TILE)
                this->tiles = this->blueprint->GetTiles(this->tile);
            break;
        case Type::POLYRAIL: {

            auto point1 = this->tile;
            auto point2 = INVALID_TILE;
            switch (trackdir) {
                case TRACKDIR_X_NE:
                    point1 += ToTileIndexDiff({1, 0});
                    point2 = point1 + ToTileIndexDiff({0, 1});
                    break;
                case TRACKDIR_Y_NW:
                    point1 += ToTileIndexDiff({0, 1});
                    point2 = point1 + ToTileIndexDiff({1, 0});
                    break;
                case TRACKDIR_Y_SE:
                    point2 = point1 + ToTileIndexDiff({1, 0});
                    break;
                case TRACKDIR_X_SW:
                    point2 = point1 + ToTileIndexDiff({0, 1});
                    break;
                case TRACKDIR_RIGHT_N:
                case TRACKDIR_LEFT_N:
                    point1 += ToTileIndexDiff({1, 1});
                    break;
                case TRACKDIR_UPPER_W:
                case TRACKDIR_LOWER_W:
                    point1 += ToTileIndexDiff({0, 1});
                    break;
                case TRACKDIR_UPPER_E:
                case TRACKDIR_LOWER_E:
                    point1 += ToTileIndexDiff({1, 0});
                    break;
                // TRACKDIR_RIGHT/LEFT_S - ok
                default:
                    break;
            }
            this->AddTile(point1, ObjectTileHighlight::make_point(CM_PALETTE_TINT_WHITE));
            if (point2 != INVALID_TILE)
                this->AddTile(point2, ObjectTileHighlight::make_point(CM_PALETTE_TINT_WHITE));
            auto z = TileHeight(point1);

            auto add_track = [this, z](TileIndex tile, TileIndex end_tile, Trackdir trackdir, SpriteID palette, TileIndex point1, TileIndex point2) {
                if (trackdir == INVALID_TRACKDIR) return;

                while(tile <= Map::Size()) {
                    this->sprites.emplace_back(
                        RemapCoords(TileX(tile) * TILE_SIZE, TileY(tile) * TILE_SIZE, z * TILE_HEIGHT + 7 /* z_offset */),
                        SPR_AUTORAIL_BASE + _AutorailTilehSprite[0][TrackdirToTrack(trackdir)],
                        palette
                    );
                    // this->AddTile(tile, std::move(ObjectTileHighlight::make_rail_track(palette, TrackdirToTrack(trackdir)).set_z(z)));
                    if (point1 != INVALID_TILE) {
                        point1 += ToTileIndexDiff(_trackdelta[trackdir]);
                        this->AddTile(point1, ObjectTileHighlight::make_point(CM_PALETTE_TINT_WHITE));
                    }
                    if (point2 != INVALID_TILE) {
                        point2 += ToTileIndexDiff(_trackdelta[trackdir]);
                        this->AddTile(point2, ObjectTileHighlight::make_point(CM_PALETTE_TINT_WHITE));
                    }

                    if (tile == end_tile) break;

                    tile += ToTileIndexDiff(_trackdelta[trackdir]);
                    /* toggle railbit for the non-diagonal tracks */
                    if (!IsDiagonalTrackdir(trackdir)) ToggleBit(trackdir, 0);
                }
                if (!IsDiagonalTrackdir(trackdir) && point1 != INVALID_TILE) {
                    ToggleBit(trackdir, 0);
                    point1 += ToTileIndexDiff(_trackdelta[trackdir]);
                    this->AddTile(point1, ObjectTileHighlight::make_point(CM_PALETTE_TINT_WHITE));
                }
            };
            add_track(this->tile, this->end_tile, this->trackdir, CM_PALETTE_TINT_YELLOW, point1, point2);
            add_track(this->tile2, this->end_tile2, this->trackdir2, PALETTE_SEL_TILE_BLUE, INVALID_TILE, INVALID_TILE);
            break;
        }
        case Type::INDUSTRY: {
            this->cost = cmd::BuildIndustry{this->tile, this->ind_type, this->ind_layout, true, 0}.call(DC_NONE);
            if (this->cost.Succeeded()) {
                const IndustrySpec *indspec = GetIndustrySpec(this->ind_type);
                if (indspec == nullptr) break;
                if (cost.cm.industry_layout >= indspec->layouts.size()) break;

                const IndustryTileLayout &layout = indspec->layouts[cost.cm.industry_layout];
                for (const IndustryTileLayoutTile &it : layout) {
                    if (it.gfx == GFX_WATERTILE_SPECIALCHECK) continue;
                    auto tile_diff = ToTileIndexDiff(it.ti);
                    TileIndex cur_tile = this->tile + tile_diff;
                    // WaterClass wc = (IsWaterTile(cur_tile) ? GetWaterClass(cur_tile) : WATER_CLASS_INVALID);
                    this->AddTile(
                        cur_tile,
                        ObjectTileHighlight::make_industry_tile(
                            CM_PALETTE_TINT_WHITE,
                            this->ind_type,
                            cost.cm.industry_layout,
                            tile_diff,
                            it.gfx
                        )
                    );
                }
            } else {
                this->AddTile(this->tile, ObjectTileHighlight::make_rect(CM_SPR_PALETTE_ZONING_RED));
            }
            break;
        }
        case Type::DOCK: {
            this->cost = cmd::BuildDock(
                this->tile,
                NEW_STATION,
                true
            ).test();
            auto palette = (cost.Succeeded() ? CM_PALETTE_TINT_WHITE : CM_PALETTE_TINT_RED_DEEP);
            this->AddTile(this->tile, ObjectTileHighlight::make_dock_slope(palette, this->ddir));
            if (this->ddir != INVALID_DIAGDIR) {
                TileIndex tile_to = TileAddByDiagDir(this->tile, this->ddir);
                this->AddTile(tile_to, ObjectTileHighlight::make_dock_flat(palette, DiagDirToAxis(this->ddir)));
            }
            break;
        }
        default:
            NOT_REACHED();
    }
}

void ObjectHighlight::UpdateOverlay() {
    HideBuildInfoOverlay();
    auto w = FindWindowFromPt(_cursor.pos.x, _cursor.pos.y);
    if (w == nullptr) return;
    auto vp = IsPtInWindowViewport(w, _cursor.pos.x, _cursor.pos.y);
    if (vp == nullptr) return;

    if (this->tile == INVALID_TILE) {
        HideBuildInfoOverlay();
        return;
    }

    auto err = this->cost.GetErrorMessage();
    // auto extra_err = this->cost.GetExtraErrorMessage();
    bool no_money = (err == STR_ERROR_NOT_ENOUGH_CASH_REQUIRES_CURRENCY);
    SetDParam(0, this->cost.GetCost());
    this->overlay_data.emplace_back(0, PAL_NONE, GetString(no_money ? CM_STR_BUILD_INFO_OVERLAY_COST_NO_MONEY : CM_STR_BUILD_INFO_OVERLAY_COST_OK));
    // if (this->cost.Failed() && err != STR_ERROR_NOT_ENOUGH_CASH_REQUIRES_CURRENCY) {
    //     if (err == INVALID_STRING_ID) {
    //         this->overlay_data.emplace_back(PAL_NONE, GetString(CM_STR_BUILD_INFO_OVERLAY_ERROR_UNKNOWN));
    //     } else {
    //         SetDParam(0, err);
    //         this->overlay_data.emplace_back(PAL_NONE, GetString(CM_STR_BUILD_INFO_OVERLAY_ERROR));
    //     }
    //     if (extra_err != INVALID_STRING_ID) {
    //         SetDParam(0, extra_err);
    //         this->overlay_data.emplace_back(PAL_NONE, GetString(CM_STR_BUILD_INFO_OVERLAY_ERROR));
    //     }
    // }

    // Point pt = RemapCoords2(TileX(this->tile) * TILE_SIZE + TILE_SIZE / 2, TileY(this->tile) * TILE_SIZE + TILE_SIZE / 2);
    Point pt = RemapCoords2(TileX(this->tile) * TILE_SIZE, TileY(this->tile) * TILE_SIZE);
    pt.x = UnScaleByZoom(pt.x - vp->virtual_left, vp->zoom) + vp->left;
    pt.y = UnScaleByZoom(pt.y - vp->virtual_top, vp->zoom) + vp->top;
    // this->overlay_pos = pt;
    ShowBuildInfoOverlay(pt.x, pt.y, this->overlay_data);
}

void ObjectHighlight::MarkDirty() {
    for (const auto &kv: this->tiles) {
        MarkTileDirtyByTile(kv.first);
    }
    for (const auto &s: this->sprites) {
        auto sprite = GetSprite(GB(s.sprite_id, 0, SPRITE_WIDTH), SpriteType::Normal);
        auto left = s.pt.x + sprite->x_offs;
        auto top = s.pt.y + sprite->y_offs;
        MarkAllViewportsDirty(
            left,
            top,
            left + UnScaleByZoom(sprite->width, ZOOM_LVL_NORMAL),
            top + UnScaleByZoom(sprite->height, ZOOM_LVL_NORMAL)
        );
    }
    if (this->type == ObjectHighlight::Type::BLUEPRINT && this->blueprint) {  // TODO why && blueprint check is needed?
        for (auto tile : this->blueprint->source_tiles) {
            // fprintf(stderr, "D %d\n", (int)tile);
            MarkTileDirtyByTile(tile);
        }
    }
    // fprintf(stderr, "E\n");
}


template <typename F>
uint8 Get(uint32 x, uint32 y, F getter) {
    if (x >= Map::SizeX() || y >= Map::SizeY()) return 0;
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

const HighlightMap::MapType &HighlightMap::GetMap() const {
    return this->map;
}

void HighlightMap::Add(TileIndex tile, ObjectTileHighlight oth) {
    this->map[tile].push_back(oth);
}

bool HighlightMap::Contains(TileIndex tile) const {
    return this->map.find(tile) != this->map.end();
}

std::optional<std::reference_wrapper<const std::vector<ObjectTileHighlight>>>
        HighlightMap::GetForTile(TileIndex tile) const {
    auto it = this->map.find(tile);
    if (it == this->map.end()) return std::nullopt;
    return it->second;
}

HighlightMap::MapTypeKeys HighlightMap::GetAllTiles() const {
    return std::views::keys(this->map);
}

std::vector<TileIndex> HighlightMap::UpdateWithMap(const HighlightMap &update) {
    std::vector<TileIndex> tiles_changed;
    for (auto it = this->map.begin(); it != this->map.end();) {
        tiles_changed.push_back(it->first);
        it = (update.Contains(it->first) ? std::next(it) : this->map.erase(it));
    }
    for (auto &[t, l] : update.GetMap()) {
        auto it = this->map.find(t);
        if (it != this->map.end() && it->second == l)
            continue;
        this->map.insert_or_assign(it, t, l);
        tiles_changed.push_back(t);
    }
    return tiles_changed;
}

void HighlightMap::AddTileArea(const TileArea &area, SpriteID palette) {
    if (area.w == 0 || area.h == 0) return;

    auto sx = TileX(area.tile), sy = TileY(area.tile);
    auto ex = sx + area.w - 1, ey = sy + area.h - 1;

    for (auto y = sy; y <= ey; y++) {
        for (auto x = sx; x <= ex; x++) {
            this->Add(TileXY(x, y), ObjectTileHighlight::make_tint(palette));
        }
    }
}

void HighlightMap::AddTileAreaWithBorder(const TileArea &area, SpriteID palette) {
    if (area.w == 0 || area.h == 0) return;

    this->AddTileArea(area, palette);

    auto sx = TileX(area.tile), sy = TileY(area.tile);
    auto ex = sx + area.w - 1, ey = sy + area.h - 1;

    if (area.w == 1 && area.h == 1) {
        this->Add(area.tile, ObjectTileHighlight::make_border(palette, ZoningBorder::FULL));
        return;
    }
    // NOTE: Doesn't handle one-tile width/height separately but relies on border overlapping
    this->Add(TileXY(sx, sy), ObjectTileHighlight::make_border(palette, ZoningBorder::TOP_LEFT | ZoningBorder::TOP_RIGHT));
    for (auto x = sx + 1; x < ex; x++)
        this->Add(TileXY(x, sy), ObjectTileHighlight::make_border(palette, ZoningBorder::TOP_LEFT));
    this->Add(TileXY(ex, sy), ObjectTileHighlight::make_border(palette, ZoningBorder::TOP_LEFT | ZoningBorder::BOTTOM_LEFT));
    for (auto y = sy + 1; y < ey; y++) {
        this->Add(TileXY(sx, y), ObjectTileHighlight::make_border(palette, ZoningBorder::TOP_RIGHT));
        for (auto x = sx + 1; x < ex; x++) {
            this->Add(TileXY(x, y), ObjectTileHighlight::make_border(palette, ZoningBorder::NONE));
        }
        this->Add(TileXY(ex, y), ObjectTileHighlight::make_border(palette, ZoningBorder::BOTTOM_LEFT));
    }
    this->Add(TileXY(sx, ey), ObjectTileHighlight::make_border(palette, ZoningBorder::TOP_RIGHT | ZoningBorder::BOTTOM_RIGHT));
    for (auto x = sx + 1; x < ex; x++)
        this->Add(TileXY(x, ey), ObjectTileHighlight::make_border(palette, ZoningBorder::BOTTOM_RIGHT));
    this->Add(TileXY(ex, ey), ObjectTileHighlight::make_border(palette, ZoningBorder::BOTTOM_LEFT | ZoningBorder::BOTTOM_RIGHT));
}

void HighlightMap::AddTilesBorder(const std::set<TileIndex> &tiles, SpriteID palette) {
    for (auto t : tiles) {
        auto b = CalcTileBorders(t, [&tiles](TileIndex t) {
            return tiles.find(t) == tiles.end() ? 0 : 1;
        });
        if (b.first != ZoningBorder::NONE)
            this->Add(t, ObjectTileHighlight::make_border(palette, b.first));
    }
}

SpriteID GetTintBySelectionColour(SpriteID colour, bool deep=false) {
    switch(colour) {
        case CM_SPR_PALETTE_ZONING_RED: return (deep ? CM_PALETTE_TINT_RED_DEEP : CM_PALETTE_TINT_RED);
        case CM_SPR_PALETTE_ZONING_ORANGE: return (deep ? CM_PALETTE_TINT_ORANGE_DEEP : CM_PALETTE_TINT_ORANGE);
        case CM_SPR_PALETTE_ZONING_GREEN: return (deep ? CM_PALETTE_TINT_GREEN_DEEP : CM_PALETTE_TINT_GREEN);
        case CM_SPR_PALETTE_ZONING_LIGHT_BLUE: return (deep ? CM_PALETTE_TINT_CYAN_DEEP : CM_PALETTE_TINT_CYAN);
        case CM_SPR_PALETTE_ZONING_YELLOW: return CM_PALETTE_TINT_YELLOW;
        // case SPR_PALETTE_ZONING__: return PALETTE_TINT_YELLOW_WHITE;
        case CM_SPR_PALETTE_ZONING_WHITE: return CM_PALETTE_TINT_WHITE;
        default: return PAL_NONE;
    }
}

SpriteID GetSelectionColourByTint(SpriteID colour) {
    switch(colour) {
        case CM_PALETTE_TINT_RED_DEEP:
        case CM_PALETTE_TINT_RED:
            return CM_SPR_PALETTE_ZONING_RED;
        case CM_PALETTE_TINT_ORANGE_DEEP:
        case CM_PALETTE_TINT_ORANGE:
            return CM_SPR_PALETTE_ZONING_ORANGE;
        case CM_PALETTE_TINT_GREEN_DEEP:
        case CM_PALETTE_TINT_GREEN:
            return CM_SPR_PALETTE_ZONING_GREEN;
        case CM_PALETTE_TINT_CYAN_DEEP:
        case CM_PALETTE_TINT_CYAN:
            return CM_SPR_PALETTE_ZONING_LIGHT_BLUE;
        case CM_PALETTE_TINT_YELLOW:
            return CM_SPR_PALETTE_ZONING_YELLOW;
        // returnase SPR_PALETTE_ZONING__: return PALETTE_TINT_YELLOW_WHITE;
        case CM_PALETTE_TINT_WHITE:
            return CM_SPR_PALETTE_ZONING_WHITE;
        default: return PAL_NONE;
    }
}

void DrawTrainDepotSprite(SpriteID palette, const TileInfo *ti, RailType railtype, DiagDirection ddir)
{
    const DrawTileSprites *dts = &_depot_gfx_table[ddir];
    const RailTypeInfo *rti = GetRailTypeInfo(railtype);
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
    const DrawTileSprites *t = GetStationTileLayout(STATION_RAIL, section + (axis == AXIS_X ? 0 : 1));
    const RailTypeInfo *rti = nullptr;

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

void DrawDockSlope(SpriteID palette, const TileInfo *ti, DiagDirection ddir) {
    uint image = (uint)ddir;
    const DrawTileSprites *t = GetStationTileLayout(STATION_DOCK, image);
    DrawRailTileSeq(ti, t, TO_INVALID, 0, 0, palette);
}

void DrawDockFlat(SpriteID palette, const TileInfo *ti, Axis axis) {
    uint image = GFX_DOCK_BASE_WATER_PART + (uint)axis;
    const DrawTileSprites *t = GetStationTileLayout(STATION_DOCK, image);
    DrawRailTileSeq(ti, t, TO_INVALID, 0, 0, palette);
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

bool is_same_industry(TileIndex tile, Industry* ind) {
    const IndustrySpec *indspec = GetIndustrySpec(ind->type);
    const IndustryTileLayout &layout = indspec->layouts[ind->selected_layout - 1];

    auto diff = TileIndexToTileIndexDiffC(tile, ind->location.tile);
    for (const IndustryTileLayoutTile &it : layout) {
        if (it.ti.x == diff.x && it.ti.y == diff.y) return true;
    }
    return false;
}

uint32 GetNearbyIndustryTileInformation(byte parameter, TileIndex tile, [[maybe_unused]] Industry* ind, bool signed_offsets, bool grf_version8)
{
    if (parameter != 0) tile = GetNearbyTile(parameter, tile, signed_offsets); // only perform if it is required

    //auto same = is_same_industry(tile, ind);
    auto same = true;
    auto res = GetNearbyTileInformation(tile, grf_version8) | (same ? 1 : 0) << 8;
    if (same) res = (res & 0xFFFFFFU) | ((uint32)MP_INDUSTRY << 24);
    return res;
}


struct IndustryTilePreviewScopeResolver : public IndustryTileScopeResolver {
    IndustryTilePreviewScopeResolver(ResolverObject &ro, Industry *industry, TileIndex tile)
        : IndustryTileScopeResolver{ro, industry, tile} {}

    uint32 GetRandomBits() const override { return 0; };
    uint32 GetTriggers() const override { return 0; };

    uint32 GetVariable(byte variable, uint32 parameter, bool *available) const override {
        // Debug(misc, 0, "TILE VAR {:X} requested", variable);
        switch (variable) {
            /* Construction state of the tile: a value between 0 and 3 */
            case 0x40: return INDUSTRY_COMPLETED;

            /* Animation frame. Like house variable 46 but can contain anything 0..FF. */
            case 0x44: return 0;

            /* Land info of nearby tiles */
            case 0x60:
                return citymania::GetNearbyIndustryTileInformation(parameter, this->tile,
                    this->industry, true, this->ro.grffile->grf_version >= 8);

            /* Animation stage of nearby tiles */
            case 0x61: {
                TileIndex tile = GetNearbyTile(parameter, this->tile);
                if (is_same_industry(tile, this->industry)) return 0;
                return UINT_MAX;
            }

            /* Get industry tile ID at offset */
            case 0x62:
                if (is_same_industry(GetNearbyTile(parameter, this->tile), this->industry)) {
                    const IndustrySpec *indspec = GetIndustrySpec(this->industry->type);
                    return indspec->grf_prop.local_id;  // our local id
                };
                return 0xFFFF;  // empty tile

            default:
                return IndustryTileScopeResolver::GetVariable(variable, parameter, available);
        }
    }
};

struct IndustriesPreviewScopeResolver : public IndustriesScopeResolver {
    IndustriesPreviewScopeResolver(ResolverObject &ro, TileIndex tile, Industry *industry, IndustryType type, uint32 random_bits = 0)
        : IndustriesScopeResolver{ro, tile, industry, type, random_bits} {}

    uint32 GetRandomBits() const override { return 0; };
    uint32 GetTriggers() const override { return 0; };
    uint32 GetVariable(byte variable, uint32 parameter, bool *available) const override {
        // Debug(misc, 0, "UNDUSTRY VAR {:X} requested", variable);
        return IndustriesScopeResolver::GetVariable(variable, parameter, available);
    }
};

static const GRFFile *GetIndTileGrffile(IndustryGfx gfx)
{
    const IndustryTileSpec *its = GetIndustryTileSpec(gfx);
    return (its != nullptr) ? its->grf_prop.grffile : nullptr;
}

struct IndustryTilePreviewResolverObject : public ResolverObject {
    IndustryTilePreviewScopeResolver indtile_scope; ///< Scope resolver for the industry tile.
    IndustriesPreviewScopeResolver ind_scope;       ///< Scope resolver for the industry owning the tile.
    IndustryGfx gfx;

    IndustryTilePreviewResolverObject(IndustryGfx gfx, TileIndex tile, Industry *indus,
            CallbackID callback = CBID_NO_CALLBACK, uint32 callback_param1 = 0, uint32 callback_param2 = 0)
        :ResolverObject(GetIndTileGrffile(gfx), callback, callback_param1, callback_param2),
         indtile_scope(*this, indus, tile),
         ind_scope(*this, tile, indus, indus->type),
         gfx(gfx)
    {
        this->root_spritegroup = GetIndustryTileSpec(gfx)->grf_prop.spritegroup[0];
    }

    ScopeResolver *GetScope(VarSpriteGroupScope scope = VSG_SCOPE_SELF, byte relative = 0) override {
        // Debug(misc, 0, "Scope requested {} {}", (int)scope, (int)relative);
        switch (scope) {
            case VSG_SCOPE_SELF: return &indtile_scope;
            case VSG_SCOPE_PARENT: return &ind_scope;
            default: return ResolverObject::GetScope(scope, relative);
        }
    }

    GrfSpecFeature GetFeature() const override { return GSF_INDUSTRYTILES; }
    uint32 GetDebugID() const override { return GetIndustryTileSpec(gfx)->grf_prop.local_id; }
};

bool DrawNewIndustryTile(TileInfo *ti, Industry *i, IndustryGfx gfx, const IndustryTileSpec *inds)
{
    if (ti->tileh != SLOPE_FLAT) {
        bool draw_old_one = true;
        if (HasBit(inds->callback_mask, CBM_INDT_DRAW_FOUNDATIONS)) {
            /* Called to determine the type (if any) of foundation to draw for industry tile */
            uint32 callback_res = GetIndustryTileCallback(CBID_INDTILE_DRAW_FOUNDATIONS, 0, 0, gfx, i, ti->tile);
            if (callback_res != CALLBACK_FAILED) draw_old_one = ConvertBooleanCallback(inds->grf_prop.grffile, CBID_INDTILE_DRAW_FOUNDATIONS, callback_res);
        }

        if (draw_old_one) DrawFoundation(ti, FOUNDATION_LEVELED);
    }

    // IndustryTileResolverObject object(gfx, ti->tile, i);
    IndustryTilePreviewResolverObject object(gfx, ti->tile, i);

    const SpriteGroup *group = object.Resolve();
    if (group == nullptr || group->type != SGT_TILELAYOUT) return false;

    /* Limit the building stage to the number of stages supplied. */
    const TileLayoutSpriteGroup *tlgroup = (const TileLayoutSpriteGroup *)group;

    IndustryDrawTileLayout(ti, tlgroup, i->random_colour, INDUSTRY_COMPLETED);
    return true;
}

void DrawIndustryTile(SpriteID palette, const TileInfo *ti, IndustryType ind_type, byte ind_layout, IndustryGfx gfx, TileIndexDiff tile_diff) {
    static Industry ind;
    const IndustryTileSpec *indts = GetIndustryTileSpec(gfx);
    if (gfx >= NEW_INDUSTRYTILEOFFSET) {
        TileInfo nti = *ti;
        ind.type = ind_type;
        ind.selected_layout = ind_layout + 1;
        ind.index = INVALID_INDUSTRY;
        ind.location.tile = ti->tile - tile_diff;

        /* Draw the tile using the specialized method of newgrf industrytile.
         * DrawNewIndustry will return false if ever the resolver could not
         * find any sprite to display.  So in this case, we will jump on the
         * substitute gfx instead. */
        if (indts->grf_prop.spritegroup[0] != nullptr && citymania::DrawNewIndustryTile(&nti, &ind, gfx, indts)) {
            return;
        } else {
            /* No sprite group (or no valid one) found, meaning no graphics associated.
             * Use the substitute one instead */
            if (indts->grf_prop.subst_id != INVALID_INDUSTRYTILE) {
                gfx = indts->grf_prop.subst_id;
                /* And point the industrytile spec accordingly */
                indts = GetIndustryTileSpec(gfx);
            }
        }
    }

    const DrawBuildingsTileStruct *dits = &_industry_draw_tile_data[gfx << 2 | INDUSTRY_COMPLETED];

    SpriteID image = dits->ground.sprite;

    /* DrawFoundation() modifies ti->z and ti->tileh */
    // if (ti->tileh != SLOPE_FLAT) DrawFoundation(ti, FOUNDATION_LEVELED);

    /* If the ground sprite is the default flat water sprite, draw also canal/river borders.
     * Do not do this if the tile's WaterClass is 'land'. */
    // if (image == SPR_FLAT_WATER_TILE && IsTileOnWater(ti->tile)) {
    //     DrawWaterClassGround(ti);
    // } else {
        DrawGroundSprite(image, GroundSpritePaletteTransform(image, palette, PALETTE_RECOLOUR_START));
    // }

    /* Add industry on top of the ground? */
    image = dits->building.sprite;
    if (image != 0) {
        AddSortableSpriteToDraw(image, palette,
            ti->x + dits->subtile_x,
            ti->y + dits->subtile_y,
            dits->width,
            dits->height,
            dits->dz,
            ti->z,
            false,
            0, 0, 0, nullptr,
            true);
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

void DrawSignal(SpriteID palette, const TileInfo *ti, RailType railtype, uint pos, SignalType type, SignalVariant variant) {
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
        sprite = (type == SIGTYPE_BLOCK && variant == SIG_ELECTRIC) ? SPR_ORIGINAL_SIGNALS_BASE : SPR_SIGNALS_BASE - 16;
        sprite += type * 16 + variant * 64 + image * 2 + condition + (type > SIGTYPE_LAST_NOPBS ? 64 : 0);
    }

    AddSortableSpriteToDraw(sprite, palette, x, y, 1, 1, BB_HEIGHT_UNDER_BRIDGE, GetSaveSlopeZ(x, y, track));
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

void DrawBridgeHead(SpriteID palette, const TileInfo *ti, RailType railtype, DiagDirection ddir, BridgeType type) {
    auto rti = GetRailTypeInfo(railtype);
    int base_offset = rti->bridge_offset;
    const PalSpriteID *psid;

    /* HACK Wizardry to convert the bridge ramp direction into a sprite offset */
    base_offset += (6 - ddir) % 4;

    /* Table number BRIDGE_PIECE_HEAD always refers to the bridge heads for any bridge type */
    if (ti->tileh == SLOPE_FLAT) base_offset += 4; // sloped bridge head
    psid = &GetBridgeSpriteTable(type, BRIDGE_PIECE_HEAD)[base_offset];

    AddSortableSpriteToDraw(psid->sprite, palette, ti->x, ti->y, 16, 16, ti->tileh == SLOPE_FLAT ? 0 : 8, ti->z);
    // DrawAutorailSelection(ti, (ddir == DIAGDIR_SW || ddir == DIAGDIR_NE ? HT_DIR_X : HT_DIR_Y), PAL_NONE);
}

void DrawTunnelHead(SpriteID palette, const TileInfo *ti, RailType railtype, DiagDirection ddir) {
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
    AddSortableSpriteToDraw(image, palette, ti->x, ti->y, 16, 16, 0, ti->z);
}

void DrawSelectionPoint(SpriteID palette, const TileInfo *ti) {
    int z = 0;
    FoundationPart foundation_part = FOUNDATION_PART_NORMAL;
    if (ti->tileh & SLOPE_N) {
        z += TILE_HEIGHT;
        if (RemoveHalftileSlope(ti->tileh) == SLOPE_STEEP_N) z += TILE_HEIGHT;
    }
    if (IsHalftileSlope(ti->tileh)) {
        Corner halftile_corner = GetHalftileSlopeCorner(ti->tileh);
        if ((halftile_corner == CORNER_W) || (halftile_corner == CORNER_E)) z += TILE_HEIGHT;
        if (halftile_corner != CORNER_S) {
            foundation_part = FOUNDATION_PART_HALFTILE;
            if (IsSteepSlope(ti->tileh)) z -= TILE_HEIGHT;
        }
    }
    DrawSelectionSprite(SPR_DOT, palette, ti, z, foundation_part);
}

void DrawBorderSprites(const TileInfo *ti, ZoningBorder border, SpriteID color) {
    auto b = (uint8)border & 15;
    auto tile_sprite = CM_SPR_BORDER_HIGHLIGHT_BASE + _tileh_to_sprite[ti->tileh] * 19;
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

TileHighlight ObjectHighlight::GetTileHighlight(const TileInfo *ti) {
    TileHighlight th;
    auto range = this->tiles.equal_range(ti->tile);
    for (auto t = range.first; t != range.second; t++) {
        t->second.SetTileHighlight(th, ti);
    }
    return th;
}

HighlightMap ObjectHighlight::GetHighlightMap(SpriteID palette) {
    // TODO remove the need to convert (maybe replace HighlightMap with multimap?)
    HighlightMap res;
    for (auto &[tile, oth] : this->tiles) {
        auto othp = oth;
        othp.palette = palette;
        res.Add(tile, othp);
    }
    return res;
}

std::optional<TileArea> ObjectHighlight::GetArea() {
    switch(this->type) {
        case Type::NONE:
        case Type::BLUEPRINT:
        case Type::POLYRAIL:
        case Type::INDUSTRY:
            return std::nullopt;
        case Type::RAIL_DEPOT:
        case Type::ROAD_DEPOT:
            return TileArea{this->tile, 1, 1};
        case Type::RAIL_STATION:
        case Type::ROAD_STOP:
            return TileArea{this->tile, this->end_tile};
        case Type::AIRPORT: {
            const AirportSpec *as = AirportSpec::Get(this->airport_type);
            if (!as->IsAvailable() || this->airport_layout >= as->num_table) return std::nullopt;
            return TileArea{this->tile, as->size_x, as->size_y};
        }
        case Type::DOCK: {
            if (this->ddir == INVALID_DIAGDIR) return std::nullopt;
            return TileArea{this->tile, TileAddByDiagDir(this->tile, this->ddir)};
        }
        default:
            NOT_REACHED();
    }
}

static void DrawObjectTileHighlight(const TileInfo *ti, const ObjectTileHighlight &oth) {
    switch (oth.type) {
        case ObjectTileHighlight::Type::RAIL_DEPOT:
            DrawTrainDepotSprite(oth.palette, ti, _cur_railtype, oth.u.rail.depot.ddir);
            break;
        case ObjectTileHighlight::Type::RAIL_TRACK: {
            DrawAutorailSelection(ti, (HighLightStyle)oth.u.rail.track, GetSelectionColourByTint(oth.palette));
            break;
        }
        case ObjectTileHighlight::Type::RAIL_STATION:
            DrawTrainStationSprite(oth.palette, ti, _cur_railtype, oth.u.rail.station.axis, oth.u.rail.station.section);
            break;
        case ObjectTileHighlight::Type::RAIL_SIGNAL:
            DrawSignal(oth.palette, ti, _cur_railtype, oth.u.rail.signal.pos, oth.u.rail.signal.type, oth.u.rail.signal.variant);
            break;
        case ObjectTileHighlight::Type::RAIL_BRIDGE_HEAD:
            DrawBridgeHead(oth.palette, ti, _cur_railtype, oth.u.rail.bridge_head.ddir, oth.u.rail.bridge_head.type);
            break;
        case ObjectTileHighlight::Type::RAIL_TUNNEL_HEAD:
            DrawTunnelHead(oth.palette, ti, _cur_railtype, oth.u.rail.tunnel_head.ddir);
            break;
        case ObjectTileHighlight::Type::ROAD_STOP:
            DrawRoadStop(oth.palette, ti, oth.u.road.stop.roadtype, oth.u.road.stop.ddir, oth.u.road.stop.is_truck);
            break;
        case ObjectTileHighlight::Type::ROAD_DEPOT:
            DrawRoadDepot(oth.palette, ti, oth.u.road.depot.roadtype, oth.u.road.depot.ddir);
            break;
        case ObjectTileHighlight::Type::DOCK_SLOPE:
            DrawDockSlope(oth.palette, ti, oth.u.dock_slope.ddir);
            break;
        case ObjectTileHighlight::Type::DOCK_FLAT:
            DrawDockFlat(oth.palette, ti, oth.u.dock_flat.axis);
            break;
        case ObjectTileHighlight::Type::AIRPORT_TILE:
            DrawAirportTile(oth.palette, ti, oth.u.airport_tile.gfx);
            break;
        case ObjectTileHighlight::Type::INDUSTRY_TILE:
            DrawIndustryTile(oth.palette, ti, oth.u.industry_tile.ind_type, oth.u.industry_tile.ind_layout, oth.u.industry_tile.gfx, oth.u.industry_tile.tile_diff);
            break;
        case ObjectTileHighlight::Type::POINT:
            DrawSelectionPoint(oth.palette, ti);
            break;
        case ObjectTileHighlight::Type::RECT:
            DrawTileSelectionRect(ti, oth.palette);
            break;
        case ObjectTileHighlight::Type::NUMBERED_RECT: {
            // TODO NUMBERED_RECT should not be used atm anyway
            // DrawTileSelectionRect(ti, oth.palette);
            // auto string_id = oth.u.numbered_rect.number ? CM_STR_LAYOUT_NUM : CM_STR_LAYOUT_RANDOM;
            // SetDParam(0, oth.u.numbered_rect.number);
            // std::string buffer = GetString(string_id);
            // auto bb = GetStringBoundingBox(buffer);
            // ViewportSign sign;
            // sign.width_normal = WidgetDimensions::scaled.fullbevel.left + Align(bb.width, 2) + WidgetDimensions::scaled.fullbevel.right;
            // Point pt = RemapCoords2(TileX(ti->tile) * TILE_SIZE + TILE_SIZE / 2, TileY(ti->tile) * TILE_SIZE + TILE_SIZE / 2);
            // sign.center = pt.x;
            // sign.top = pt.y - bb.height / 2;

            // ViewportAddString(_cur_dpi, ZOOM_LVL_OUT_8X, &sign,
            //                   string_id, STR_NULL, STR_NULL, oth.u.numbered_rect.number, 0, COLOUR_WHITE);
            break;
        }
        case ObjectTileHighlight::Type::BORDER: {
            DrawBorderSprites(ti, oth.u.border, oth.palette);
            break;
        }
        default:
            break;
    }
}

void ObjectHighlight::Draw(const TileInfo *ti) {
    auto range = this->tiles.equal_range(ti->tile);
    for (auto t = range.first; t != range.second; t++) {
        DrawObjectTileHighlight(ti, t->second);
    }
    // fprintf(stderr, "TILEH DRAW %d %d %d\n", ti->tile, (int)i, (int)this->tiles.size());
}

bool Intersects(Point tl, Point br, int left, int top, int right, int bottom) {
    return (
        right >= tl.x &&
        left <= br.x &&
        bottom >= tl.y &&
        top <= br.y
    );
}

bool Intersects(const Rect &rect, int left, int top, int right, int bottom) {
    return (
        right >= rect.left &&
        left <= rect.right &&
        bottom >= rect.top &&
        top <= rect.bottom
    );
}


void ObjectHighlight::DrawSelectionOverlay([[maybe_unused]] DrawPixelInfo *dpi) {
    for (auto &s : this->sprites) {
        DrawSpriteViewport(s.sprite_id, s.palette_id, s.pt.x, s.pt.y);
    }
    // for (auto &[tile, oth] : this->tiles) {
    //     switch (oth.type) {
    //         case ObjectTileHighlight::Type::RAIL_TRACK: {
    //             if (oth.z != -1) {
    //                 auto h = oth.z * TILE_HEIGHT + 7 /* z_offset */;
    //                 auto tx = TileX(tile) * TILE_SIZE, ty = TileY(tile) * TILE_SIZE;
    //                 auto tl = RemapCoords(tx + TILE_SIZE / 2, ty - TILE_SIZE / 2, h);
    //                 auto br = RemapCoords(tx + TILE_SIZE / 2, ty + 3 * TILE_SIZE / 2, h);
    //                 if (Intersects(tl, br, dpi.left, dpi.top, dpi.left + dpi.width, dpi.top + dpi.height)) {
    //                     auto sprite = SPR_AUTORAIL_BASE + _AutorailTilehSprite[0][oth.u.rail.track];
    //                     auto p = RemapCoords(tx, ty, h);
    //                     DrawSpriteViewport(sprite, oth.palette, p.x, p.y);
    //                 }
    //             }
    //             break;
    //         }
    //         default:
    //             break;
    //     }
    // }
}

void ObjectHighlight::DrawOverlay([[maybe_unused]] DrawPixelInfo *dpi) {
    if (!this->cost.Succeeded()) return;
}

static uint8 _industry_highlight_hash = 0;

void UpdateIndustryHighlight() {
    _industry_highlight_hash++;
}

bool CanBuildIndustryOnTileCached(IndustryType type, TileIndex tile) {
    // if (_mz[tile].industry_fund_type != type || !_mz[tile].industry_fund_result) {
    if (_mz[tile.base()].industry_fund_update != _industry_highlight_hash || !_mz[tile.base()].industry_fund_result) {
        bool res = CanBuildIndustryOnTile(type, tile);
        // _mz[tile].industry_fund_type = type;
        _mz[tile.base()].industry_fund_update = _industry_highlight_hash;
        _mz[tile.base()].industry_fund_result = res ? 2 : 1;
        return res;
    }
    return (_mz[tile.base()].industry_fund_result == 2);
}

SpriteID GetIndustryZoningPalette(TileIndex tile) {
    if (!IsTileType(tile, MP_INDUSTRY)) return PAL_NONE;
    Industry *ind = Industry::GetByTile(tile);
    auto n_produced = 0;
    auto n_serviced = 0;
    for (auto j = 0; j < INDUSTRY_NUM_OUTPUTS; j++) {
        if (ind->produced[j].cargo == CT_INVALID) continue;
        if (ind->produced[j].history[LAST_MONTH].production == 0 && ind->produced[j].history[THIS_MONTH].production == 0) continue;
        n_produced++;
        if (ind->produced[j].history[LAST_MONTH].transported > 0 || ind->produced[j].history[THIS_MONTH].transported > 0)
            n_serviced++;
    }
    if (n_serviced < n_produced)
        return (n_serviced == 0 ? CM_PALETTE_TINT_RED_DEEP : CM_PALETTE_TINT_ORANGE_DEEP);
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
                th.add_border(b.first, CM_SPR_PALETTE_ZONING_RED);
        }
        if (IsInsideSelectedRectangle(TileX(ti->tile) * TILE_SIZE, TileY(ti->tile) * TILE_SIZE)) {
            // th.ground_pal = GetTintBySelectionColour(color);
            if (_thd.make_square_red) {
                th.ground_pal = th.structure_pal = CM_PALETTE_TINT_RED;
                th.highlight_structure_pal = th.highlight_ground_pal = CM_PALETTE_TINT_RED;
            } else {
                th.ground_pal = PAL_NONE;
                th.structure_pal = CM_PALETTE_HIDE_SPRITE;
                th.highlight_structure_pal = th.highlight_ground_pal = PAL_NONE;
            }
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
        const SpriteID pal[] = {PAL_NONE, CM_SPR_PALETTE_ZONING_WHITE, PAL_NONE};
        th.add_border(b.first, pal[b.second]);
        const SpriteID pal2[] = {PAL_NONE, CM_PALETTE_TINT_WHITE, CM_PALETTE_TINT_BLUE};
        th.ground_pal = th.structure_pal = pal2[b.second];
    }

    if (_highlight_join_area.tile != INVALID_TILE) {
        auto b = CalcTileBorders(ti->tile, [](TileIndex t) {
            return _highlight_join_area.Contains(t) ? 1 : 0;
        });
        th.add_border(b.first, CM_SPR_PALETTE_ZONING_LIGHT_BLUE);
        if (b.second) {
            switch (th.ground_pal) {
                case CM_PALETTE_TINT_WHITE: th.ground_pal = th.structure_pal = CM_PALETTE_TINT_CYAN_WHITE; break;
                case CM_PALETTE_TINT_BLUE: break;
                default: th.ground_pal = th.structure_pal = CM_PALETTE_TINT_CYAN; break;
            }
        }
    }
}

void CalcCBAcceptanceBorders(TileHighlight &th, TileIndex tile, SpriteID border_pal, SpriteID ground_pal) {
    int tx = TileX(tile), ty = TileY(tile);
    uint16 radius = _settings_game.citymania.cb.acceptance_range;
    bool in_zone = false;
    ZoningBorder border = ZoningBorder::NONE;
    _town_kdtree.FindContained(
        (uint16)std::max<int>(0, tx - radius),
        (uint16)std::max<int>(0, ty - radius),
        (uint16)std::min<int>(tx + radius + 1, Map::SizeX()),
        (uint16)std::min<int>(ty + radius + 1, Map::SizeY()),
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
        if (4 * sq * n < Map::Size() * i) break;
        AddTownCBLimitBorder(tile, p.second, border, in_zone);
        i++;
    }
    uint radius = IntSqrt(sq);
    int tx = TileX(tile), ty = TileY(tile);
    _town_kdtree.FindContained(
        (uint16)std::max<int>(0, tx - radius),
        (uint16)std::max<int>(0, ty - radius),
        (uint16)std::min<int>(tx + radius + 1, Map::SizeX()),
        (uint16)std::min<int>(ty + radius + 1, Map::SizeY()),
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

TileHighlight GetTileHighlight(const TileInfo *ti, TileType tile_type) {
    TileHighlight th;

    auto hl = _at.tiles.GetForTile(ti->tile);
    if (hl.has_value()) {
        for (auto &oth : hl.value().get()) {
            oth.SetTileHighlight(th, ti);
        }
        return th;
    }

    th = _thd.cm.GetTileHighlight(ti);;
    if (ti->tile == INVALID_TILE || tile_type == MP_VOID) return th;
    if (_zoning.outer == CHECKTOWNZONES) {
        auto p = GetTownZoneBorder(ti->tile);
        auto color = PAL_NONE;
        switch (p.second) {
            default: break; // Tz0
            case 1: color = CM_SPR_PALETTE_ZONING_WHITE; break; // Tz0
            case 2: color = CM_SPR_PALETTE_ZONING_YELLOW; break; // Tz1
            case 3: color = CM_SPR_PALETTE_ZONING_ORANGE; break; // Tz2
            case 4: color = CM_SPR_PALETTE_ZONING_ORANGE; break; // Tz3
            case 5: color = CM_SPR_PALETTE_ZONING_RED; break; // Tz4 - center
        };
        th.add_border(p.first, color);
        th.ground_pal = th.structure_pal = GetTintBySelectionColour(color);
        if (CB_Enabled())
            CalcCBTownLimitBorder(th, ti->tile, CM_SPR_PALETTE_ZONING_RED, PAL_NONE);
    } else if (_zoning.outer == CHECKSTACATCH) {
        th.add_border(citymania::GetAnyStationCatchmentBorder(ti->tile),
                      CM_SPR_PALETTE_ZONING_LIGHT_BLUE);
    } else if (_zoning.outer == CHECKTOWNGROWTHTILES) {
        // if (tgt == TGTS_NEW_HOUSE) th.sprite = SPR_IMG_HOUSE_NEW;
        switch (_game->get_town_growth_tile(ti->tile)) {
            // case TGTS_CB_HOUSE_REMOVED_NOGROW:
            case TownGrowthTileState::RH_REMOVED:
                th.selection = CM_SPR_PALETTE_ZONING_LIGHT_BLUE;
                break;
            case TownGrowthTileState::RH_REBUILT:
                th.selection = CM_SPR_PALETTE_ZONING_WHITE;
                th.structure_pal = CM_PALETTE_TINT_WHITE;
                break;
            case TownGrowthTileState::NEW_HOUSE:
                th.selection = CM_SPR_PALETTE_ZONING_GREEN;
                th.structure_pal = CM_PALETTE_TINT_GREEN;
                break;
            case TownGrowthTileState::CS:
                th.selection = CM_SPR_PALETTE_ZONING_ORANGE;
                break;
            case TownGrowthTileState::HS:
                th.selection = CM_SPR_PALETTE_ZONING_YELLOW;
                break;
            case TownGrowthTileState::HR:
                th.selection = CM_SPR_PALETTE_ZONING_RED;
                break;
            default: th.selection = PAL_NONE;
        }
        if (th.selection) th.ground_pal = GetTintBySelectionColour(th.selection);
    } else if (_zoning.outer == CHECKBULUNSER) {
        if (IsTileType (ti->tile, MP_HOUSE)) {
            StationFinder stations(TileArea(ti->tile, 1, 1));

            // TODO check cargos
            if (stations.GetStations()->empty())
                th.ground_pal = th.structure_pal = CM_PALETTE_TINT_RED_DEEP;
        }
    } else if (_zoning.outer == CHECKINDUNSER) {
        auto pal = GetIndustryZoningPalette(ti->tile);
        if (pal) th.ground_pal = th.structure_pal = CM_PALETTE_TINT_RED_DEEP;
    } else if (_zoning.outer == CHECKTOWNADZONES) {
        auto getter = [](TileIndex t) { return _mz[t.base()].advertisement_zone; };
        auto b = CalcTileBorders(ti->tile, getter);
        const SpriteID pal[] = {PAL_NONE, CM_SPR_PALETTE_ZONING_YELLOW, CM_SPR_PALETTE_ZONING_ORANGE, CM_SPR_PALETTE_ZONING_RED};
        th.add_border(b.first, pal[b.second]);
        auto check_tile = ti->tile;
        if (IsTileType (ti->tile, MP_STATION)) {
            auto station =  Station::GetByTile(ti->tile);
            if (station) check_tile = station->xy;
        }
        auto z = getter(check_tile);
        if (z) th.ground_pal = th.structure_pal = GetTintBySelectionColour(pal[z]);
    } else if (_zoning.outer == CHECKCBACCEPTANCE) {
        CalcCBAcceptanceBorders(th, ti->tile, CM_SPR_PALETTE_ZONING_WHITE, CM_PALETTE_TINT_WHITE);
    } else if (_zoning.outer == CHECKCBTOWNLIMIT) {
        CalcCBTownLimitBorder(th, ti->tile, CM_SPR_PALETTE_ZONING_WHITE, CM_PALETTE_TINT_WHITE);
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
        const SpriteID pal[] = {PAL_NONE, CM_SPR_PALETTE_ZONING_GREEN, CM_SPR_PALETTE_ZONING_RED};
        th.add_border(b.first, pal[b.second]);
        auto z = getter(ti->tile);
        if (z) th.ground_pal = th.structure_pal = GetTintBySelectionColour(pal[z]);
    }

    if (_settings_client.gui.cm_show_industry_forbidden_tiles &&
            _industry_forbidden_tiles != INVALID_INDUSTRYTYPE) {
        auto b = CalcTileBorders(ti->tile, [](TileIndex t) { return !CanBuildIndustryOnTileCached(_industry_forbidden_tiles, t); });
        th.add_border(b.first, CM_SPR_PALETTE_ZONING_RED);
        if (!CanBuildIndustryOnTileCached(_industry_forbidden_tiles, ti->tile))
            th.ground_pal = th.structure_pal = CM_PALETTE_TINT_RED;
    }

    SetStationSelectionHighlight(ti, th);
    SetBlueprintHighlight(ti, th);

    return th;
}

void DrawTileZoning(const TileInfo *ti, const TileHighlight &th, TileType tile_type) {
    if (ti->tile == INVALID_TILE || tile_type == MP_VOID) return;
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

bool DrawTileSelection(const TileInfo *ti, [[maybe_unused]] const TileHighlightType &tht) {
    if (ti->tile == INVALID_TILE || IsTileType(ti->tile, MP_VOID)) return false;

    auto hl = _at.tiles.GetForTile(ti->tile);
    if (hl.has_value()) {
        for (auto &oth : hl.value().get()) {
            DrawObjectTileHighlight(ti, oth);
        }
        return true;
    }

    _thd.cm.Draw(ti);

    if (_thd.drawstyle == CM_HT_BLUEPRINT_PLACE) return true;

    if (_thd.select_proc == DDSP_BUILD_STATION || _thd.select_proc == DDSP_BUILD_BUSSTOP
        || _thd.select_proc == DDSP_BUILD_TRUCKSTOP || _thd.select_proc == CM_DDSP_BUILD_AIRPORT
        || _thd.select_proc == CM_DDSP_BUILD_ROAD_DEPOT || _thd.select_proc == CM_DDSP_BUILD_RAIL_DEPOT
        || _thd.select_proc == CM_DDSP_FUND_INDUSTRY) {
        // handled by DrawTileZoning
        return true;
    }
    if (_thd.cm_poly_terra) {
        return true;
    }

    return false;
}

void DrawSelectionOverlay(DrawPixelInfo *dpi) {
    _thd.cm.DrawSelectionOverlay(dpi);
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
    bool force_new = false;
    // fprintf(stderr, "UPDATE %d %d %d %d\n", tile, _thd.size.x, _thd.size.y, (int)((_thd.place_mode & HT_DRAG_MASK) == HT_RECT));
    if (_thd.place_mode == CM_HT_BLUEPRINT_PLACE) {
        UpdateBlueprintTileSelection(tile);
        new_drawstyle = CM_HT_BLUEPRINT_PLACE;
    } else if (pt.x == -1) {
    } else if (_thd.make_square_red) {
    } else if (_thd.select_proc == CM_DDSP_FUND_INDUSTRY) {
        _thd.cm_new = ObjectHighlight::make_industry(tile, _cm_funding_type, _cm_funding_layout);
        force_new = true;
        new_drawstyle = HT_RECT;
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
                std::min((_thd.new_pos.x + _thd.new_size.x) / TILE_SIZE, Map::SizeX()) - 1,
                std::min((_thd.new_pos.y + _thd.new_size.y) / TILE_SIZE, Map::SizeY()) - 1
            );
            if (_thd.select_proc == DDSP_BUILD_STATION)
                _thd.cm_new = ObjectHighlight::make_rail_station(start_tile, end_tile, _railstation.orientation);
            else if (_thd.select_proc == DDSP_BUILD_BUSSTOP || _thd.select_proc == DDSP_BUILD_TRUCKSTOP) {
                auto ddir = _roadstop_gui_settings.orientation;
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
                _thd.cm_new = ObjectHighlight::make_road_stop(
                    start_tile,
                    end_tile,
                    _cur_roadtype,
                    ddir,
                    _thd.select_proc == DDSP_BUILD_TRUCKSTOP,
                    _roadstop_gui_settings.roadstop_class,
                    _roadstop_gui_settings.roadstop_type
                );
            }
        }
        new_drawstyle = HT_RECT;
    } else if ((_thd.place_mode & HT_POLY) && _thd.cm_new_poly_terra) {
        _thd.cm_new = ObjectHighlight::make_polyrail(TileVirtXY(_thd.selstart.x, _thd.selstart.y),
                                                     TileVirtXY(_thd.selend.x, _thd.selend.y),
                                                     _thd.cm_poly_dir,
                                                     TileVirtXY(_thd.selstart2.x, _thd.selstart2.y),
                                                     TileVirtXY(_thd.selend2.x, _thd.selend2.y),
                                                     _thd.cm_poly_dir2);
    }
    if (force_new || _thd.cm != _thd.cm_new) {
        _thd.cm.MarkDirty();
        _thd.cm = _thd.cm_new;
        _thd.cm.UpdateTiles();
        _thd.cm.UpdateOverlay();
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

        if (_mz[tile.base()].town_zone != group)
            _mz[tile.base()].industry_fund_result = 0;

        if (_mz[tile.base()].town_zone > group) {
            if (recalc) {
                _mz[tile.base()].town_zone = GetAnyTownZone(tile);
                if (_zoning.outer == CHECKTOWNZONES)
                    MarkTileDirtyByTile(tile);
            }
        } else if (_mz[tile.base()].town_zone < group) {
            _mz[tile.base()].town_zone = group;
            if (_zoning.outer == CHECKTOWNZONES)
                MarkTileDirtyByTile(tile);
        }
    }
}

void UpdateAdvertisementZoning(TileIndex center, uint radius, uint8 zone) {
    uint16 x1, y1, x2, y2;
    x1 = (uint16)std::max<int>(0, TileX(center) - radius);
    x2 = (uint16)std::min<int>(TileX(center) + radius + 1, Map::SizeX());
    y1 = (uint16)std::max<int>(0, TileY(center) - radius);
    y2 = (uint16)std::min<int>(TileY(center) + radius + 1, Map::SizeY());
    for (uint16 y = y1; y < y2; y++) {
        for (uint16 x = x1; x < x2; x++) {
            auto tile = TileXY(x, y);
            if (DistanceManhattan(tile, center) > radius) continue;
            _mz[tile.base()].advertisement_zone = std::max(_mz[tile.base()].advertisement_zone, zone);
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
    return CalcTileBorders(tile, [](TileIndex t) { return _mz[t.base()].town_zone; });
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
    if (_settings_client.gui.cm_show_industry_forbidden_tiles &&
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
            return CM_PALETTE_SHADE_S;

        case SLOPE_NE:
            return CM_PALETTE_SHADE_SW;

        case SLOPE_E:
        case SLOPE_STEEP_E:
            return CM_PALETTE_SHADE_W;

        case SLOPE_SE:
            return CM_PALETTE_SHADE_NW;

        case SLOPE_STEEP_S:
        case SLOPE_S:
            return CM_PALETTE_SHADE_N;

        case SLOPE_SW:
            return CM_PALETTE_SHADE_NE;

        case SLOPE_STEEP_W:
        case SLOPE_W:
            return CM_PALETTE_SHADE_E;

        case SLOPE_NW:
            return CM_PALETTE_SHADE_SE;

        default:
            return PAL_NONE;
    }
}

ActiveTool _at;


static void ResetVanillaHighlight() {
    if (_thd.window_class != WC_INVALID) {
        /* Undo clicking on button and drag & drop */
        Window *w = _thd.GetCallbackWnd();
        /* Call the abort function, but set the window class to something
         * that will never be used to avoid infinite loops. Setting it to
         * the 'next' window class must not be done because recursion into
         * this function might in some cases reset the newly set object to
         * place or not properly reset the original selection. */
        _thd.window_class = WC_INVALID;
        if (w != nullptr) {
            w->OnPlaceObjectAbort();
            CloseWindowById(WC_TOOLTIPS, 0);
        }
    }

    /* Mark the old selection dirty, in case the selection shape or colour changes */
    if ((_thd.drawstyle & HT_DRAG_MASK) != HT_NONE) SetSelectionTilesDirty();

    SetTileSelectSize(1, 1);

    _thd.make_square_red = false;
}

void SetActiveTool(up<Tool> &&tool) {
    ResetVanillaHighlight();
    ResetActiveTool();
    _at.tool = std::move(tool);
}

void ResetActiveTool() {
    for (auto t : _at.tiles.GetAllTiles()) {
        MarkTileDirtyByTile(t);
    }
    _at.tool = nullptr;
    _at.tiles = {};
}

const up<Tool> &GetActiveTool() {
    return _at.tool;
}


void UpdateActiveTool() {
    Point pt = GetTileBelowCursor();
    auto tile = pt.x == -1 ? INVALID_TILE : TileVirtXY(pt.x, pt.y);

    ToolGUIInfo info;
    if (citymania::StationBuildTool::active_highlight.has_value()) {
        info = GetSelectedStationGUIInfo();
    } else if (_at.tool != nullptr) {
        _at.tool->Update(pt, tile);
        info = _at.tool->GetGUIInfo();
    }
    auto [hlmap, overlay_data, cost] = info;
    auto tiles_changed = _at.tiles.UpdateWithMap(hlmap);
    for (auto t : tiles_changed)
        MarkTileDirtyByTile(t);

    if (cost.GetExpensesType() != INVALID_EXPENSES || cost.GetErrorMessage() != INVALID_STRING_ID) {
        // Add CommandCost info
        auto err = cost.GetErrorMessage();
        if (cost.Succeeded()) {
            auto money = cost.GetCost();
            if (money != 0) {
                SetDParam(0, money);
                overlay_data.emplace_back(0, PAL_NONE, GetString(CM_STR_BUILD_INFO_OVERLAY_COST_OK));
            }
        } else if (err == STR_ERROR_NOT_ENOUGH_CASH_REQUIRES_CURRENCY) {
            SetDParam(0, cost.GetCost());
            overlay_data.emplace_back(0, PAL_NONE, GetString(CM_STR_BUILD_INFO_OVERLAY_COST_NO_MONEY));
        } else {
            if (err == INVALID_STRING_ID) {
                overlay_data.emplace_back(0, PAL_NONE, GetString(CM_STR_BUILD_INFO_OVERLAY_ERROR_UNKNOWN));
            } else {
                SetDParam(0, err);
                overlay_data.emplace_back(0, PAL_NONE, GetString(CM_STR_BUILD_INFO_OVERLAY_ERROR));
            }
            auto extra_err = cost.GetExtraErrorMessage();
            if (extra_err != INVALID_STRING_ID) {
                SetDParam(0, extra_err);
                overlay_data.emplace_back(0, PAL_NONE, GetString(CM_STR_BUILD_INFO_OVERLAY_ERROR));
            }
        }
    }

    /* Update overlay */
    if (overlay_data.size() > 0) {
        auto w = FindWindowFromPt(_cursor.pos.x, _cursor.pos.y);
        if (w == nullptr) { HideBuildInfoOverlay(); return; }
        auto vp = IsPtInWindowViewport(w, _cursor.pos.x, _cursor.pos.y);
        if (vp == nullptr) { HideBuildInfoOverlay(); return; }
        Point pto = RemapCoords2(TileX(tile) * TILE_SIZE, TileY(tile) * TILE_SIZE);
        pto.x = UnScaleByZoom(pto.x - vp->virtual_left, vp->zoom) + vp->left;
        pto.y = UnScaleByZoom(pto.y - vp->virtual_top, vp->zoom) + vp->top;
        ShowBuildInfoOverlay(pto.x, pto.y, overlay_data);
    } else {
        HideBuildInfoOverlay();
    }
}

bool _prev_left_button_down = false;
// const Window *_click_window = nullptr;
bool _keep_mouse_click = false;
// const Window *_keep_mouse_window;

bool HandleMouseMove() {
    bool changed = _left_button_down != _prev_left_button_down;
    _prev_left_button_down = _left_button_down;
    // Window *w = FindWindowFromPt(_cursor.pos.x, _cursor.pos.y);
    bool released = !_left_button_down && changed && _keep_mouse_click;
    if (!_left_button_down) _keep_mouse_click = false;

    if (_at.tool == nullptr) return false;
    // Viewport *vp = IsPtInWindowViewport(w, );

    auto pt = GetTileBelowCursor();
    if (pt.x == -1) return false;
    auto tile = pt.x == -1 ? INVALID_TILE : TileVirtXY(pt.x, pt.y);
    _at.tool->Update(pt, tile);
    _at.tool->HandleMouseMove();
    if (_left_button_down) {
        if (changed && _at.tool->HandleMousePress()) {
            _keep_mouse_click = true;
        }
        if (_keep_mouse_click) return true;
    }
    if (released) {
        _at.tool->HandleMouseRelease();
    }
    return false;
}

bool HandleMouseClick(Viewport *vp, bool double_click) {
    if (_at.tool == nullptr) return false;
    auto pt = GetTileBelowCursor();
    auto tile = pt.x == -1 ? INVALID_TILE : TileVirtXY(pt.x, pt.y);
    _at.tool->Update(pt, tile);
    return _at.tool->HandleMouseClick(vp, pt, tile, double_click);
}

bool HandlePlacePushButton(Window *w, WidgetID widget, up<Tool> tool) {
    if (w->IsWidgetDisabled(widget)) return false;

    if (_settings_client.sound.click_beep) SndPlayFx(SND_15_BEEP);
    w->SetDirty();

    if (w->IsWidgetLowered(widget)) {
        ResetObjectToPlace();
        return false;
    }

    w->LowerWidget(widget);

    auto icon = tool->GetCursor();
    if ((icon & ANIMCURSOR_FLAG) != 0) {
        SetAnimatedMouseCursor(_animcursors[icon & ~ANIMCURSOR_FLAG]);
    } else {
        SetMouseCursor(icon, PAL_NONE);
    }
    citymania::SetActiveTool(std::move(tool));
    _thd.window_class = w->window_class;
    _thd.window_number = w->window_number;

    return true;

}

}  // namespace citymania
