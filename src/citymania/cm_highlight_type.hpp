#ifndef CITYMANIA_HIGHLIGHT_TYPE_HPP
#define CITYMANIA_HIGHLIGHT_TYPE_HPP

#include "cm_polyrail.hpp"


#include "../bridge.h"
#include "../direction_type.h"
#include "../map_func.h"
#include "../road_type.h"
#include "../signal_type.h"
#include "../station_map.h"
#include "../station_type.h"
#include "../tile_cmd.h"
#include "../tile_type.h"
#include "../track_type.h"

#include <map>
#include <set>
#include <vector>


namespace citymania {

class ObjectTileHighlight {
public:
    enum class Type {
        BEGIN = 0,
        RAIL_DEPOT = BEGIN,
        RAIL_TRACK,
        RAIL_STATION,
        RAIL_SIGNAL,
        RAIL_BRIDGE_HEAD,
        RAIL_TUNNEL_HEAD,
        ROAD_STOP,
        ROAD_DEPOT,
        AIRPORT_TILE,
        POINT,
        END,
    };

    Type type;
    SpriteID palette;

    union {
        struct {
            struct {
                DiagDirection ddir;
            } depot;
            Track track;
            struct {
                Axis axis;
                byte section;
            } station;
            struct {
                uint pos;
                SignalType type;
                SignalVariant variant;
            } signal;
            struct {
                DiagDirection ddir;
                TileIndex other_end;
                BridgeType type;
            } bridge_head;
            struct {
                DiagDirection ddir;
            } tunnel_head;
        } rail;
        struct {
            struct {
                RoadType roadtype;
                DiagDirection ddir;
                bool is_truck;
            } stop;
            struct {
                RoadType roadtype;
                DiagDirection ddir;
            } depot;
        } road;
        struct {
            StationGfx gfx;
        } airport_tile;
    } u;

    ObjectTileHighlight(Type type, SpriteID palette): type{type}, palette{palette} {}
    static ObjectTileHighlight make_rail_depot(SpriteID palette, DiagDirection ddir);
    static ObjectTileHighlight make_rail_track(SpriteID palette, Track track);
    static ObjectTileHighlight make_rail_station(SpriteID palette, Axis axis, byte section);
    static ObjectTileHighlight make_rail_signal(SpriteID palette, uint pos, SignalType type, SignalVariant variant);
    static ObjectTileHighlight make_rail_bridge_head(SpriteID palette, DiagDirection ddir, BridgeType type);
    static ObjectTileHighlight make_rail_tunnel_head(SpriteID palette, DiagDirection ddir);

    static ObjectTileHighlight make_road_stop(SpriteID palette, RoadType roadtype, DiagDirection ddir, bool is_truck);
    static ObjectTileHighlight make_road_depot(SpriteID palette, RoadType roadtype, DiagDirection ddir);
    static ObjectTileHighlight make_airport_tile(SpriteID palette, StationGfx gfx);
    static ObjectTileHighlight make_point(SpriteID palette);
};


class DetachedHighlight {
public:
    Point pt;
    SpriteID sprite_id;
    SpriteID palette_id;
    DetachedHighlight(Point pt, SpriteID sprite_id, SpriteID palette_id)
        :pt{pt}, sprite_id{sprite_id}, palette_id{palette_id} {}
};

class TileIndexDiffCCompare{
public:
    bool operator()(const TileIndexDiffC &a, const TileIndexDiffC &b) const {
        if (a.x < b.x) return true;
        if (a.x == b.x && a.y < b.y) return true;
        return false;
    }
};

class Blueprint {
public:
    class Item {
    public:
        enum class Type {
            BEGIN = 0,
            RAIL_DEPOT = BEGIN,
            RAIL_TRACK,
            RAIL_STATION,
            RAIL_STATION_PART,
            RAIL_SIGNAL,
            RAIL_BRIDGE,
            RAIL_TUNNEL,
            ROAD_STOP,
            ROAD_DEPOT,
            END,
        };
        Type type;
        TileIndexDiffC tdiff;
        union {
            struct {
                struct {
                    DiagDirection ddir;
                } depot;
                struct {
                    uint16 length;
                    Trackdir start_dir;
                } track;
                struct {
                    StationID id;
                    bool has_part;
                } station;
                struct {
                    Axis axis;
                    StationID id;
                    byte numtracks;
                    byte plat_len;
                } station_part;
                struct {
                    uint pos;
                    SignalType type;
                    SignalVariant variant;
                    bool twoway;
                } signal;
                struct {
                    DiagDirection ddir;
                    TileIndexDiffC other_end;
                    BridgeType type;
                } bridge;
                struct {
                    DiagDirection ddir;
                    TileIndexDiffC other_end;
                } tunnel;
            } rail;
            struct {
                struct {
                    DiagDirection ddir;
                    TileIndexDiffC other_end;
                } stop;
                struct {
                    DiagDirection ddir;
                } depot;
            } road;
        } u;
        Item(Type type, TileIndexDiffC tdiff)
            : type{type}, tdiff{tdiff} {}
    };

    std::vector<Item> items;
    std::set<TileIndex> source_tiles;

    Blueprint() {}

    void Clear() {
        this->items.clear();
        this->source_tiles.clear();
    }

    void Add(TileIndex source_tile, Item item);

    bool HasSourceTile(TileIndex tile) {
        return (this->source_tiles.find(tile) != this->source_tiles.end());
    }

    sp<Blueprint> Rotate();

    std::multimap<TileIndex, ObjectTileHighlight> GetTiles(TileIndex tile);
};


class ObjectHighlight {
public:
    enum class Type {
        NONE = 0,
        RAIL_DEPOT = 1,
        RAIL_STATION = 2,
        ROAD_STOP = 3,
        ROAD_DEPOT = 4,
        AIRPORT = 5,
        BLUEPRINT = 6,
        POLYRAIL = 7,
    };

    Type type = Type::NONE;
    TileIndex tile = INVALID_TILE;
    TileIndex end_tile = INVALID_TILE;
    Trackdir trackdir = INVALID_TRACKDIR;
    TileIndex tile2 = INVALID_TILE;
    TileIndex end_tile2 = INVALID_TILE;
    Trackdir trackdir2 = INVALID_TRACKDIR;
    Axis axis = INVALID_AXIS;
    DiagDirection ddir = INVALID_DIAGDIR;
    RoadType roadtype = INVALID_ROADTYPE;
    bool is_truck = false;
    int airport_type = 0;
    byte airport_layout = 0;
    sp<Blueprint> blueprint = nullptr;

protected:
    bool tiles_updated = false;
    std::multimap<TileIndex, ObjectTileHighlight> tiles;
    std::vector<DetachedHighlight> sprites = {};
    void AddTile(TileIndex tile, ObjectTileHighlight &&oh);
    // void AddSprite(TileIndex tile, ObjectTileHighlight &&oh);
    void UpdateTiles();
    void PlaceExtraDepotRail(TileIndex tile, DiagDirection dir, Track track);

public:
    ObjectHighlight(Type type = Type::NONE): type{type} {}
    bool operator==(const ObjectHighlight& oh);
    bool operator!=(const ObjectHighlight& oh);

    static ObjectHighlight make_rail_depot(TileIndex tile, DiagDirection ddir);
    static ObjectHighlight make_rail_station(TileIndex start_tile, TileIndex end_tile, Axis axis);
    static ObjectHighlight make_road_stop(TileIndex start_tile, TileIndex end_tile, RoadType roadtype, DiagDirection orientation, bool is_truck);
    static ObjectHighlight make_road_depot(TileIndex tile, RoadType roadtype, DiagDirection orientation);
    static ObjectHighlight make_airport(TileIndex start_tile, int airport_type, byte airport_layout);
    static ObjectHighlight make_blueprint(TileIndex tile, sp<Blueprint> blueprint);
    static ObjectHighlight make_polyrail(TileIndex start_tile, TileIndex end_tile, Trackdir trackdir,
                                         TileIndex start_tile2, TileIndex end_tile2, Trackdir trackdir2);

    void Draw(const TileInfo *ti);
    void DrawOverlay(DrawPixelInfo *dpi);
    void MarkDirty();
};


}  // namespace citymania

#endif
