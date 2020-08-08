#ifndef CITYMANIA_HIGHLIGHT_TYPE_HPP
#define CITYMANIA_HIGHLIGHT_TYPE_HPP

#include "../direction_type.h"
#include "../tile_cmd.h"
#include "../tile_type.h"
#include "../track_type.h"

#include <map>

namespace citymania {


class ObjectTileHighlight {
public:
    enum class Type {
        RAIL_DEPOT = 0,
        RAIL_TRACK = 1,
    };

    Type type;
    union {
        struct {
            DiagDirection ddir;
        } depot;
        struct {
            Track track;
        } rail;
    } u;

    ObjectTileHighlight(Type type): type{type} {}
    static ObjectTileHighlight make_depot(DiagDirection ddir);
    static ObjectTileHighlight make_rail(Track track);
};

class ObjectHighlight {
public:
    enum class Type {
        NONE = 0,
        RAIL_DEPOT = 1,
    };

protected:
    Type type;
    union {
        struct {
            TileIndex tile;
            DiagDirection ddir;
        } depot;
    } u;

    bool tiles_updated = false;
    std::multimap<TileIndex, ObjectTileHighlight> tiles;
    void UpdateTiles();
    void PlaceExtraDepotRail(TileIndex tile, DiagDirection dir, Track track);

public:
    ObjectHighlight(Type type = Type::NONE): type{type} { /* get rid of uninitualized warning */ this->u.depot.tile = INVALID_TILE; }
    bool operator==(const ObjectHighlight& oh);
    bool operator!=(const ObjectHighlight& oh);

    static ObjectHighlight make_depot(TileIndex tile, DiagDirection ddir);

    void Draw(const TileInfo *ti);
    void MarkDirty();
};


}  // namespace citymania

#endif
