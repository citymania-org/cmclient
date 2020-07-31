#include "../stdafx.h"

#include "cm_polyrail.hpp"

#include "cm_hotkeys.hpp"

#include "../command_type.h"
#include "../command_func.h"
#include "../gfx_func.h"
#include "../rail_type.h"
#include "../tilehighlight_type.h"
#include "../track_func.h"
#include "../viewport_func.h"
#include "../table/sprites.h"

#include <unordered_map>
#include <queue>

#include "../safeguards.h"

enum FoundationPart {
    FOUNDATION_PART_NONE     = 0xFF,  ///< Neither foundation nor groundsprite drawn yet.
    FOUNDATION_PART_NORMAL   = 0,     ///< First part (normal foundation or no foundation)
    FOUNDATION_PART_HALFTILE = 1,     ///< Second part (halftile foundation)
    FOUNDATION_PART_END
};
extern TileHighlightData _thd;
extern CommandCost CmdBuildSingleRail(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text);
extern RailType _cur_railtype;
extern bool _shift_pressed;
void DrawSelectionSprite(SpriteID image, PaletteID pal, const TileInfo *ti, int z_offset, FoundationPart foundation_part);

namespace citymania {

extern void (*DrawAutorailSelection)(const TileInfo *ti, HighLightStyle autorail_type, PaletteID pal);
extern void (*AddTileSpriteToDraw)(SpriteID image, PaletteID pal, int32 x, int32 y, int z, const SubSprite *sub, int extra_offs_x, int extra_offs_y);


// +static HighLightStyle GetPartOfAutoLine(int px, int py, const Point &selstart, const Point &selend, HighLightStyle dir)
//  {
// -       px -= _thd.selstart.x;
// -       py -= _thd.selstart.y;
// -
// -       if ((_thd.drawstyle & HT_DRAG_MASK) != HT_LINE) return false;
// -
// -       switch (_thd.drawstyle & HT_DIR_MASK) {
// -               case HT_DIR_X:  return py == 0; // x direction
// -               case HT_DIR_Y:  return px == 0; // y direction
// -               case HT_DIR_HU: return px == -py || px == -py - 16; // horizontal upper
// -               case HT_DIR_HL: return px == -py || px == -py + 16; // horizontal lower
// -               case HT_DIR_VL: return px == py || px == py + 16; // vertical left
// -               case HT_DIR_VR: return px == py || px == py - 16; // vertical right
// -               default:
// -                       NOT_REACHED();
// +       if (!IsInRangeInclusive(selstart.x & ~TILE_UNIT_MASK, selend.x & ~TILE_UNIT_MASK, px)) return HT_DIR_END;
// +       if (!IsInRangeInclusive(selstart.y & ~TILE_UNIT_MASK, selend.y & ~TILE_UNIT_MASK, py)) return HT_DIR_END;
// +
// +       px -= selstart.x & ~TILE_UNIT_MASK;
// +       py -= selstart.y & ~TILE_UNIT_MASK;
// +
// +       switch (dir) {
// +               case HT_DIR_X: return (py == 0) ? HT_DIR_X : HT_DIR_END;
// +               case HT_DIR_Y: return (px == 0) ? HT_DIR_Y : HT_DIR_END;
// +               case HT_DIR_HU: return (px == -py) ? HT_DIR_HU : (px == -py - (int)TILE_SIZE) ? HT_DIR_HL : HT_DIR_END;
// +               case HT_DIR_HL: return (px == -py) ? HT_DIR_HL : (px == -py + (int)TILE_SIZE) ? HT_DIR_HU : HT_DIR_END;
// +               case HT_DIR_VL: return (px ==  py) ? HT_DIR_VL : (px ==  py + (int)TILE_SIZE) ? HT_DIR_VR : HT_DIR_END;
// +               case HT_DIR_VR: return (px ==  py) ? HT_DIR_VR : (px ==  py - (int)TILE_SIZE) ? HT_DIR_VL : HT_DIR_END;
// +               default: NOT_REACHED(); break;
//         }
// -}


// for (auto ths: _thd.segments) {

// type = GetPartOfAutoLine(ti->x, ti->y, ths.selstart, ths.selend, ths.dir);

// if (type >= HT_DIR_END) break;

// DrawAutorailSelection(ti, type, PALETTE_SEL_TILE_BLUE);

// HighLightStyle Polyrail::GetTileHighlightStyle(TileIndex tile) {
//     for (auto p : this->tiles) {
//         if (p.first == tile)
//             return p.second;
//     }
// }

typedef uint32 Weight;

// static const HighLightStyle _highlight_style_from_ddirs[4][4] = {
//     //     NE          SE         SW          NW
//     {   HT_DIR_X,  HT_DIR_HL, HT_DIR_END,  HT_DIR_VL },
//     {  HT_DIR_HU,   HT_DIR_Y,  HT_DIR_VL, HT_DIR_END },
//     { HT_DIR_END,  HT_DIR_VR,   HT_DIR_X,  HT_DIR_HU },
//     {  HT_DIR_VR, HT_DIR_END,  HT_DIR_HL,   HT_DIR_Y }
// };

static const Trackdir _trackdir_from_sides[4][4] = {
    //     NE          SE         SW          NW
    {    TRACKDIR_X_NE, TRACKDIR_LOWER_E, INVALID_TRACKDIR,  TRACKDIR_LEFT_N },
    { TRACKDIR_UPPER_E,    TRACKDIR_Y_SE,  TRACKDIR_LEFT_S, INVALID_TRACKDIR },
    { INVALID_TRACKDIR, TRACKDIR_RIGHT_S,    TRACKDIR_X_SW, TRACKDIR_UPPER_W },
    { TRACKDIR_RIGHT_N, INVALID_TRACKDIR, TRACKDIR_LOWER_W,    TRACKDIR_Y_NW }
};


static const Direction _trackdir_to_direction[TRACKDIR_END] = {
    DIR_NE, DIR_SE, DIR_E, DIR_E, DIR_S, DIR_S, DIR_NE, DIR_SE,
    DIR_SW, DIR_NW, DIR_W, DIR_W, DIR_N, DIR_N, DIR_SW, DIR_NW
};

struct {
    Direction dir;
    DiagDirection ddir;
    Track track;
    Weight weight;
} _neighbors[][3] = {
    {
        {.dir =  DIR_N, .ddir = DIAGDIR_NW, .track= TRACK_LEFT, .weight = 2},
        {.dir = DIR_NE, .ddir = DIAGDIR_NE, .track=    TRACK_X, .weight = 3},
        {.dir =  DIR_E, .ddir = DIAGDIR_SE, .track=TRACK_LOWER, .weight = 2}
    }, {
        {.dir =  DIR_E, .ddir = DIAGDIR_NE, .track=TRACK_UPPER, .weight = 2},
        {.dir = DIR_SE, .ddir = DIAGDIR_SE, .track=    TRACK_Y, .weight = 3},
        {.dir =  DIR_S, .ddir = DIAGDIR_SW, .track= TRACK_LEFT, .weight = 2}
    }, {
        {.dir =  DIR_S, .ddir = DIAGDIR_SE, .track=TRACK_RIGHT, .weight = 2},
        {.dir = DIR_SW, .ddir = DIAGDIR_SW, .track=    TRACK_X, .weight = 3},
        {.dir =  DIR_W, .ddir = DIAGDIR_NW, .track=TRACK_UPPER, .weight = 2}
    }, {
        {.dir =  DIR_W, .ddir = DIAGDIR_SW, .track=TRACK_LOWER, .weight = 2},
        {.dir = DIR_NW, .ddir = DIAGDIR_NW, .track=    TRACK_Y, .weight = 3},
        {.dir =  DIR_N, .ddir = DIAGDIR_NE, .track=TRACK_RIGHT, .weight = 2}
    },
};

PolyrailPoint _polyrail_start;

PolyrailPoint PolyrailPoint::Reverse() {
    return PolyrailPoint(TileAddByDiagDir(this->tile, this->side), ReverseDiagDir(this->side));
}

PolyrailPoint PolyrailPoint::Normalize() {
    return ((this->side == DIAGDIR_NW || this->side == DIAGDIR_NE) ? *this : this->Reverse());
}


static bool CanBuildRail(TileIndex tile, Track track) {
    if (tile > MapSize()) return false;
    auto res = CmdBuildSingleRail(tile, DC_NO_WATER | DC_AUTO, _cur_railtype, track, nullptr);
    return res.Succeeded();
}

uint RailDistance(TileIndex a, TileIndex b) {
    uint dx = abs((int)TileX(a) - (int)TileX(b));
    uint dy = abs((int)TileY(a) - (int)TileY(b));
    return 3 * max(dx, dy) + min(dx, dy);
}

Polyrail MakePolyrail(PolyrailPoint start, PolyrailPoint end) {
    fprintf(stderr, "POLYNEW %d:%d %d:%d\n", (int)start.tile, (int)start.side, (int)end.tile, (int)end.side);
    auto polyrail = Polyrail(start, end);
    uint16 max_curve = 250;
    uint16 max_slowdown_curve = _settings_game.vehicle.max_train_length * 2;
    if (start == end || !start.IsValid() || !end.IsValid())
        return polyrail;

    struct Vertex {
        TileIndex tile;
        Direction dir;
        DiagDirection side;
        uint8 last_left;
        uint8 last_right;
        Vertex(TileIndex tile, Direction dir, DiagDirection side, uint8 last_left, uint8 last_right): tile{tile}, dir{dir}, side{side}, last_left{last_left}, last_right{last_right} {};
        Vertex(const Vertex &x): Vertex{x.tile, x.dir, x.side, x.last_left, x.last_right} {};
        Vertex &operator=(const Vertex &v) { this->tile = v.tile; this->dir = v.dir; this->side = v.side; this->last_left = v.last_left; this->last_right = v.last_right; return *this; }
        bool operator==(const Vertex &v) const { return this->tile == v.tile && this->dir == v.dir && this->side == v.side && this->last_left == v.last_left && this->last_right == v.last_right; }
    };

    auto vertex_hash = [](const Vertex &v) { return (size_t)v.tile ^ ((size_t)v.side << 30) ^ ((size_t)v.last_left << 32) ^ ((size_t)v.last_right << 40) ^ ((size_t)v.dir << 48) ; };

    struct Item {
        Weight weight;
        Vertex vertex;
    };

    auto item_cmp = [](const Item &a, const Item &b) { return a.weight > b.weight; };
    std::priority_queue<Item, std::vector<Item>, decltype(item_cmp)> q(item_cmp);
    std::unordered_map<Vertex, Vertex, decltype(vertex_hash)> prev(10, vertex_hash);
    std::unordered_map<Vertex, Weight, decltype(vertex_hash)> g(10, vertex_hash);

    auto start_weight = RailDistance(start.tile, end.tile) << 8;
    auto rev_start = start.Reverse();
    auto sv1 = Vertex(start.tile, INVALID_DIR, start.side, max_curve, max_curve);
    auto sv2 = Vertex(rev_start.tile, INVALID_DIR, rev_start.side, max_curve, max_curve);
    q.push(Item{start_weight, sv1});
    g.insert(std::make_pair(sv1, 0));
    q.push(Item{start_weight, sv2});
    g.insert(std::make_pair(sv2, 0));

    auto res = Vertex(start.tile, INVALID_DIR, INVALID_DIAGDIR, max_curve, max_curve);
    auto min_distance = UINT32_MAX;
    uint checked = 0;
    auto max_weight = (4 * RailDistance(start.tile, end.tile) / 3 + 20) << 8;

    while (!q.empty()) {
        auto x = q.top();
        checked++;
        q.pop();

        auto gp = g.find(x.vertex);
        auto xw = gp->second;

        // fprintf(stderr, "Q %d %d %d\n", (int)x.weight, (int)x.vertex.tile, (int)x.vertex.ddir);

        // TODO check borders
        if (PolyrailPoint(x.vertex.tile, x.vertex.side).Normalize() == end) {
            res = x.vertex;
            break;
        }
        auto tile = TileAddByDiagDir(x.vertex.tile, x.vertex.side);

        auto d = RailDistance(tile, end.tile) << 8;
        if (d < min_distance) {
            res = x.vertex;
            min_distance = d;
        }

        for (size_t i = 0; i < 3; i++) {
            auto &n = _neighbors[x.vertex.side][i];
            if (!CanBuildRail(tile, n.track)) continue;
            uint16 curve = max_curve;
            uint8 track_len = (IsDiagonalDirection(n.dir) ? 2 : 1);
            uint16 last_left = min(x.vertex.last_left + track_len, max_curve);
            uint16 last_right = min(x.vertex.last_right + track_len, max_curve);
            if (x.vertex.dir != INVALID_DIR) {
                if (n.dir == ChangeDir(x.vertex.dir, DIRDIFF_45LEFT)) {
                    curve = last_left;
                    last_left = 0;
                } else if (n.dir == ChangeDir(x.vertex.dir, DIRDIFF_45RIGHT)) {
                    curve = last_right;
                    last_right = 0;
                } else if (n.dir != x.vertex.dir) {
                    /* TODO _settings_game.pf.forbid_90_deg */
                    continue;
                }
            }
            auto v = Vertex(tile, n.dir, n.ddir, last_left, last_right);
            auto w = xw + (n.weight << 8);
            if (curve < max_slowdown_curve) w += (Weight)(max_slowdown_curve - curve) << 8;
            else w += max_curve - curve;

            if (w > max_weight) continue;

            auto gp = g.find(v);
            if (gp != g.end()) {
                if (gp->second <= w) continue;
                gp->second = w;
            } else
                g.insert(gp, std::make_pair(v, w));

            q.push(Item{
                .weight = w + (RailDistance(TileAddByDiagDir(v.tile, v.side), end.tile) << 8),
                .vertex = v
            });

            auto pp = prev.insert(std::make_pair(v, x.vertex));
            if (!pp.second) pp.first->second = x.vertex;
        }
        if (checked > 10000) break;
    }

    if (res.side == INVALID_DIAGDIR)
        return polyrail;

    while (!(res == sv1 || res == sv2)) {
        auto p = prev.find(res);
        if (p == prev.end()) break;

        // polyrail.tiles.push_back(std::make_pair(res.tile, _highlight_style_from_ddirs[(*p).second.side][res.side]));
        polyrail.tiles.push_back(std::make_pair(res.tile, _trackdir_from_sides[(*p).second.side][res.side]));
        res = (*p).second;
    }
    std::reverse(polyrail.tiles.begin(), polyrail.tiles.end());

    return polyrail;
}

PolyrailPoint GetPolyrailPoint(int x, int y) {
    auto s = (x + y) >> 4;
    auto e = (y - x) >> 4;
    auto side = ((s + e) & 1) ? DIAGDIR_NW : DIAGDIR_NE;
    auto tx = (s - e) >> 1;
    auto ty = (s + e + 1) >> 1;
    return PolyrailPoint(TileXY(tx, ty), side);
}

Point SnapToTileEdge(Point p) {
    auto s = (p.x + p.y) >> 4;
    auto e = (p.y - p.x) >> 4;
    auto x_edge = ((s + e) & 1) ? true : false;
    auto tx = (s - e) >> 1;
    auto ty = (s + e + 1) >> 1;
    return Point{
        (int)(tx * TILE_SIZE + (x_edge ? TILE_SIZE / 2 : 0)),
        (int)(ty * TILE_SIZE + (x_edge ? 0 : TILE_SIZE / 2))
    };
}

void UpdatePolyrailDrawstyle(Point pt) {
    if (pt.x == -1) return;
    if (!_polyrail_start.IsValid()) return;
    auto end = GetPolyrailPoint(pt.x, pt.y);
    // fprintf(stderr, "POLYUPD %d:%d %d:%d\n", (int)start.tile, (int)start.side, (int)end.tile, (int)end.side);
    _thd.selend = pt;
    if (_thd.cm_polyrail.start == _polyrail_start && _thd.cm_polyrail.end == end)
        return;
    _thd.cm_new_polyrail = MakePolyrail(_polyrail_start, end);
}

void SetPolyrailSelectionTilesDirty() {
    MarkTileDirtyByTile(TileVirtXY(_thd.pos.x, _thd.pos.y));
    for (auto p : _thd.cm_polyrail.tiles) {
        MarkTileDirtyByTile(p.first);
    }
}

HighLightStyle UpdatePolyrailTileSelection() {
    Point pt = GetTileBelowCursor();
    if (pt.x == -1) return HT_NONE;

    _thd.new_pos = citymania::SnapToTileEdge(pt);
    auto end = GetPolyrailPoint(pt.x, pt.y);
    _thd.cm_new_polyrail = MakePolyrail(_polyrail_start, end);

    return CM_HT_RAIL;
}

static const int SLOPE_TO_X_EDGE_Z_OFFSET[SLOPE_STEEP_E + 1] = {0, 4, 0, 4, 0, 0, 0, 4, 4, 8, 0, 8, 4, 8, 4, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 12, 0, 12, 4};
static const int SLOPE_TO_Y_EDGE_Z_OFFSET[SLOPE_STEEP_E + 1] = {0, 0, 0, 0, 4, 0, 4, 4, 4, 4, 0, 4, 8, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 4, 0, 12, 12};

void DrawPolyrailTileSelection(const TileInfo *ti) {
    if (_thd.drawstyle != CM_HT_RAIL) return;

    if (SeparateFnPressed() || !_thd.cm_polyrail.IsValid()) {
        if (ti->tile == TileVirtXY(_thd.pos.x, _thd.pos.y)) {
            auto zofs = (((_thd.pos.x & TILE_UNIT_MASK) ? SLOPE_TO_X_EDGE_Z_OFFSET : SLOPE_TO_Y_EDGE_Z_OFFSET)[(size_t)ti->tileh]);
            AddTileSpriteToDraw(_cur_dpi->zoom <= ZOOM_LVL_DETAIL ? SPR_DOT : SPR_DOT_SMALL,
                                PAL_NONE, _thd.pos.x, _thd.pos.y, ti->z + zofs, nullptr, 0, 0);
        }
        return;
    }

    bool is_first_segment = true;
    Direction first_dir = INVALID_DIR;
    // TODO increase effeciency
    for (auto p : _thd.cm_polyrail.tiles) {
        auto dir = _trackdir_to_direction[p.second];
        if (first_dir == INVALID_DIR) first_dir = dir;
        else if (first_dir != dir) is_first_segment = false;
        if (p.first == ti->tile) {
            auto hs = (HighLightStyle)TrackdirToTrack(p.second);
            DrawAutorailSelection(ti, hs, is_first_segment ? PAL_NONE : PALETTE_SEL_TILE_BLUE);
        }
    }
}

// Mostly copied from rail_gui.cpp
static CommandContainer DoRailroadTrackCmd(TileIndex start_tile, TileIndex end_tile, Track track, bool remove_mode)
{
    CommandContainer ret = {
        start_tile,                             // tile
        end_tile,                               // p1
        (uint32)(_cur_railtype | (track << 6)), // p2
        remove_mode ?
                CMD_REMOVE_RAILROAD_TRACK | CMD_MSG(STR_ERROR_CAN_T_REMOVE_RAILROAD_TRACK) :
                CMD_BUILD_RAILROAD_TRACK  | CMD_MSG(STR_ERROR_CAN_T_BUILD_RAILROAD_TRACK), // cmd
        CcPlaySound_SPLAT_RAIL,                 // callback
        ""                                      // text
    };
    return ret;
}

void PlaceRail_Polyrail(Point pt, bool remove_mode) {
    auto end = GetPolyrailPoint(pt.x, pt.y);

    // fprintf(stderr, "%d %d\n", );

    if (!_polyrail_start.IsValid() || SeparateFnPressed()) {
        _polyrail_start = end;
        _thd.cm_polyrail = MakePolyrail(_polyrail_start, PolyrailPoint());
        return;
    }

    auto polyrail = MakePolyrail(_polyrail_start, end);
    if (polyrail.tiles.empty()) return;

    Trackdir trackdir = polyrail.tiles[0].second;
    Direction dir = _trackdir_to_direction[trackdir];
    TileIndex segment_start = polyrail.tiles[0].first;
    TileIndex segment_end = segment_start;
    Trackdir trackdir_end = trackdir;
    for (auto p : polyrail.tiles) {
        if (dir != _trackdir_to_direction[p.second]) break;
        segment_end = p.first;
        trackdir_end = p.second;
    }

    auto cmd = DoRailroadTrackCmd(segment_start, segment_end, TrackdirToTrack(trackdir), remove_mode);

    if (_estimate_mod) {
        DoCommandP(&cmd);
        return;
    }
    if (DoCommand(&cmd, DC_AUTO | DC_NO_WATER).GetErrorMessage() != STR_ERROR_ALREADY_BUILT) {
        if (!DoCommandP(&cmd)) return;
    }
    _polyrail_start = PolyrailPoint(segment_end, TrackdirToExitdir(trackdir_end)).Normalize();
}

void SetPolyrailToPlace() {
    if (SeparateFnPressed()) _polyrail_start = PolyrailPoint();
    _thd.cm_polyrail = MakePolyrail(_polyrail_start, PolyrailPoint());
}

} // namespace citymania
