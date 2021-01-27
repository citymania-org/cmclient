#ifndef CITYMANIA_HIGHLIGHT_TYPE_HPP
#define CITYMANIA_HIGHLIGHT_TYPE_HPP

#include "../bridge.h"
#include "../direction_type.h"
#include "../map_func.h"
#include "../signal_type.h"
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
        END,
    };

    Type type;
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
    } u;

    ObjectTileHighlight(Type type): type{type} {}
    static ObjectTileHighlight make_rail_depot(DiagDirection ddir);
    static ObjectTileHighlight make_rail_track(Track track);
    static ObjectTileHighlight make_rail_station(Axis axis, byte section);
    static ObjectTileHighlight make_rail_signal(uint pos, SignalType type, SignalVariant variant);
    static ObjectTileHighlight make_rail_bridge_head(DiagDirection ddir, BridgeType type);
    static ObjectTileHighlight make_rail_tunnel_head(DiagDirection ddir);
};

class TileIndexDiffCCompare{
public:
    bool operator()(const TileIndexDiffC &a, const TileIndexDiffC &b) {
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
        BLUEPRINT = 3,
    };

    Type type;
    TileIndex tile = INVALID_TILE;
    TileIndex end_tile = INVALID_TILE;
    Axis axis = INVALID_AXIS;
    DiagDirection ddir = INVALID_DIAGDIR;
    sp<Blueprint> blueprint = nullptr;

protected:
    bool tiles_updated = false;
    std::multimap<TileIndex, ObjectTileHighlight> tiles;
    void UpdateTiles();
    void PlaceExtraDepotRail(TileIndex tile, DiagDirection dir, Track track);

public:
    ObjectHighlight(Type type = Type::NONE): type{type} {}
    bool operator==(const ObjectHighlight& oh);
    bool operator!=(const ObjectHighlight& oh);

    static ObjectHighlight make_rail_depot(TileIndex tile, DiagDirection ddir);
    static ObjectHighlight make_rail_station(TileIndex start_tile, TileIndex end_tile, Axis axis);
    static ObjectHighlight make_blueprint(TileIndex tile, sp<Blueprint> blueprint);

    void Draw(const TileInfo *ti);
    void MarkDirty();
};


}  // namespace citymania

#endif
