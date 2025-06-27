#ifndef CITYMANIA_HIGHLIGHT_TYPE_HPP
#define CITYMANIA_HIGHLIGHT_TYPE_HPP

#include "../bridge.h"
#include "../direction_type.h"
#include "../map_func.h"
#include "../newgrf_roadstop.h"
#include "../road_type.h"
#include "../signal_type.h"
#include "../station_map.h"
#include "../station_type.h"
#include "../station_gui.h"  // StationCoverageType
#include "../tile_cmd.h"
#include "../tile_type.h"
#include "../track_type.h"

#include <map>
#include <set>
#include <type_traits>
#include <unordered_set>
#include <vector>

#include "cm_command_type.hpp"


namespace citymania {

typedef std::function<up<Command>(TileIndex start_tile, TileIndex end_tile)> HighlightGenerator;


enum ZoningBorder: uint8 {
    NONE = 0,
    TOP_LEFT = 1,
    TOP_RIGHT = 2,
    BOTTOM_RIGHT = 4,
    BOTTOM_LEFT = 8,
    TOP_CORNER = 16,
    RIGHT_CORNER = 32,
    BOTTOM_CORNER = 64,
    LEFT_CORNER = 128,
    FULL = TOP_LEFT | TOP_RIGHT | BOTTOM_LEFT | BOTTOM_RIGHT,
};
DECLARE_ENUM_AS_BIT_SET(ZoningBorder);


class TileHighlight {
public:
    SpriteID ground_pal = PAL_NONE;
    SpriteID structure_pal = PAL_NONE;
    SpriteID highlight_ground_pal = PAL_NONE;
    SpriteID highlight_structure_pal = PAL_NONE;
    SpriteID sprite = 0;
    SpriteID selection = PAL_NONE;
    ZoningBorder border[4] = {};
    SpriteID border_color[4] = {};
    uint border_count = 0;

    void add_border(ZoningBorder border, SpriteID color) {
        if (border == ZoningBorder::NONE || !color) return;
        this->border[this->border_count] = border;
        this->border_color[this->border_count] = color;
        this->border_count++;
    }

    void tint_all(SpriteID color) {
        if (!color) return;
        this->ground_pal = this->structure_pal = color;
    }

    void clear_borders() {
        this->border_count = 0;
    }
};


class TileIndexWrapper {
public:
    TileIndex tile;
    TileIndexWrapper() {}
    TileIndexWrapper(TileIndex tile)
        :tile{tile} {}

    inline operator TileIndex () const
    {
        return this->tile;
    }
};

class ObjectTileHighlight {
public:
    enum class Type : uint8_t {
        BEGIN = 0,
        RAIL_DEPOT = BEGIN,
        RAIL_TRACK,
        RAIL_STATION,
        RAIL_SIGNAL,
        RAIL_BRIDGE_HEAD,
        RAIL_TUNNEL_HEAD,
        ROAD_STOP,
        ROAD_DEPOT,
        DOCK_SLOPE,
        DOCK_FLAT,

        AIRPORT_TILE,
        INDUSTRY_TILE,
        POINT,
        RECT,
        NUMBERED_RECT,
        BORDER,
        TINT,
        STRUCT_TINT,
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
                uint8_t section;
            } station;
            struct {
                uint pos;
                SignalType type;
                SignalVariant variant;
            } signal;
            struct {
                DiagDirection ddir;
                uint32 other_end;
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
                RoadStopClassID spec_class;
                uint16_t spec_index;
            } stop;
            struct {
                RoadType roadtype;
                DiagDirection ddir;
            } depot;
        } road;
        struct {
            StationGfx gfx;
        } airport_tile;
        struct {
            IndustryType ind_type;
            uint8_t ind_layout;
            TileIndexDiff tile_diff;
            IndustryGfx gfx;  // TODO remove?
        } industry_tile;
        struct {
            uint32 number;
        } numbered_rect;
        struct {
            DiagDirection ddir;
        } dock_slope;
        struct {
            Axis axis;
        } dock_flat;
        ZoningBorder border;
    } u;

    ObjectTileHighlight(Type type, SpriteID palette): type{type}, palette{palette} {}

    static ObjectTileHighlight make_rail_depot(SpriteID palette, DiagDirection ddir);
    static ObjectTileHighlight make_rail_track(SpriteID palette, Track track);
    static ObjectTileHighlight make_rail_station(SpriteID palette, Axis axis, uint8_t section);
    static ObjectTileHighlight make_rail_signal(SpriteID palette, uint pos, SignalType type, SignalVariant variant);
    static ObjectTileHighlight make_rail_bridge_head(SpriteID palette, DiagDirection ddir, BridgeType type);
    static ObjectTileHighlight make_rail_tunnel_head(SpriteID palette, DiagDirection ddir);

    static ObjectTileHighlight make_road_stop(SpriteID palette, RoadType roadtype, DiagDirection ddir, bool is_truck, RoadStopClassID spec_class, uint16_t spec_index);
    static ObjectTileHighlight make_road_depot(SpriteID palette, RoadType roadtype, DiagDirection ddir);

    static ObjectTileHighlight make_dock_slope(SpriteID palette, DiagDirection ddir);
    static ObjectTileHighlight make_dock_flat(SpriteID palette, Axis axis);

    static ObjectTileHighlight make_airport_tile(SpriteID palette, StationGfx gfx);

    static ObjectTileHighlight make_industry_tile(SpriteID palette, IndustryType ind_type, uint8_t ind_layout, TileIndexDiff tile_diff, IndustryGfx gfx);
    static ObjectTileHighlight make_point(SpriteID palette);
    static ObjectTileHighlight make_rect(SpriteID palette);
    static ObjectTileHighlight make_numbered_rect(SpriteID palette, uint32 number);
    static ObjectTileHighlight make_border(SpriteID palette, ZoningBorder border);
    static ObjectTileHighlight make_tint(SpriteID palette);
    static ObjectTileHighlight make_struct_tint(SpriteID palette);

    bool operator==(const ObjectTileHighlight &oh) const;
    bool SetTileHighlight(TileHighlight &th, const TileInfo *ti) const;
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
                    uint8_t numtracks;
                    uint8_t plat_len;
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
    enum Type : uint8_t {
        NONE = 0,
        RAIL_DEPOT = 1,
        RAIL_STATION = 2,
        ROAD_STOP = 3,
        ROAD_DEPOT = 4,
        AIRPORT = 5,
        BLUEPRINT = 6,
        POLYRAIL = 7,
        INDUSTRY = 8,
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
    RoadStopClassID road_stop_spec_class;
    uint16_t road_stop_spec_index;
    int airport_type = 0;
    uint8_t airport_layout = 0;
    sp<Blueprint> blueprint = nullptr;
    IndustryType ind_type = IT_INVALID;
    uint32 ind_layout = 0;
    CommandCost cost;
    Dimension overlay_dim;

protected:
    bool tiles_updated = false;
    std::multimap<TileIndex, ObjectTileHighlight> tiles;
    std::vector<DetachedHighlight> sprites = {};
    std::vector<std::pair<SpriteID, std::string>> overlay_data = {};
    // Point overlay_pos = {0, 0};
    void AddTile(TileIndex tile, ObjectTileHighlight &&oh);
    // void AddSprite(TileIndex tile, ObjectTileHighlight &&oh);
    void PlaceExtraDepotRail(TileIndex tile, DiagDirection dir, Track track);

public:
    ObjectHighlight(Type type = Type::NONE): type{type} {}
    bool operator==(const ObjectHighlight& oh) const;
    bool operator!=(const ObjectHighlight& oh) const;

    static ObjectHighlight make_rail_depot(TileIndex tile, DiagDirection ddir);
    static ObjectHighlight make_rail_station(TileIndex start_tile, TileIndex end_tile, Axis axis);
    static ObjectHighlight make_road_stop(TileIndex start_tile, TileIndex end_tile, RoadType roadtype, DiagDirection orientation, bool is_truck, RoadStopClassID spec_class, uint16_t spec_index);
    static ObjectHighlight make_road_depot(TileIndex tile, RoadType roadtype, DiagDirection orientation);
    static ObjectHighlight make_airport(TileIndex start_tile, int airport_type, uint8_t airport_layout);
    static ObjectHighlight make_blueprint(TileIndex tile, sp<Blueprint> blueprint);
    static ObjectHighlight make_polyrail(TileIndex start_tile, TileIndex end_tile, Trackdir trackdir,
                                         TileIndex start_tile2, TileIndex end_tile2, Trackdir trackdir2);

    static ObjectHighlight make_industry(TileIndex tile, IndustryType ind_type, uint32 ind_layout);

    TileHighlight GetTileHighlight(const TileInfo *ti);
    void Draw(const TileInfo *ti);
    void DrawSelectionOverlay(DrawPixelInfo *dpi);
    void DrawOverlay(DrawPixelInfo *dpi);
    void AddStationOverlayData(int w, int h, int rad,  StationCoverageType sct);
    void UpdateTiles();
    void UpdateOverlay();
    void MarkDirty();
};

class Preview {
public:
    typedef std::map<TileIndex, std::vector<ObjectTileHighlight>> TileMap;
    virtual ~Preview() {}
    virtual void Update(Point pt, TileIndex tile) = 0;
    virtual void HandleMouseMove() {};
    virtual bool HandleMousePress() { return false; };
    virtual void HandleMouseRelease() {};
    virtual bool HandleMouseClick(Viewport* /* vp */, Point /* pt */, TileIndex /* tile */, bool /* double_click */) { return false; };
    virtual TileMap GetTiles() = 0;
    virtual CursorID GetCursor() = 0;
    virtual void OnStationRemoved(const Station* /* station */) {};
};

// enum class ActiveHighlightState {
//     None,
//     Place,
//     DragStart,
//     DragStop,
// };

struct ActivePreview {
    up<Preview> preview;
    Preview::TileMap tiles;
};

extern ActivePreview _ap;

}  // namespace citymania

#endif
