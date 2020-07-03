#include "../stdafx.h"

#include "polyrail.hpp"

#include "../command_type.h"
#include "../rail_type.h"
#include "../tilehighlight_type.h"
#include "../viewport_func.h"
#include "../table/sprites.h"

#include <unordered_map>
#include <queue>

#include "../safeguards.h"

extern TileHighlightData _thd;
extern void (*StaticDrawAutorailSelection)(const TileInfo *ti, HighLightStyle autorail_type, PaletteID pal);
extern CommandCost CmdBuildSingleRail(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text);
extern RailType _cur_railtype;

namespace citymania {


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

static const HighLightStyle _highlight_style_from_ddirs[4][4] = {
    //     NE          SE         SW          NW
    {   HT_DIR_X,  HT_DIR_HL, HT_DIR_END,  HT_DIR_VL },
    {  HT_DIR_HU,   HT_DIR_Y,  HT_DIR_VL, HT_DIR_END },
    { HT_DIR_END,  HT_DIR_VR,   HT_DIR_X,  HT_DIR_HU },
    {  HT_DIR_VR, HT_DIR_END,  HT_DIR_HL,   HT_DIR_Y }
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

Polyrail MakePolyrail(TileIndex start, TileIndex end) {
    auto polyrail = Polyrail(start, end);
    uint16 max_curve = 250;
    uint16 max_slowdown_curve = _settings_game.vehicle.max_train_length * 2;
    if (start == end)
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
    for (auto ddir: {DIAGDIR_NE, DIAGDIR_SE, DIAGDIR_SW, DIAGDIR_NW}) {
        auto v = Vertex(start, INVALID_DIR, ddir, max_curve, max_curve);
        q.push(Item{
            .weight = (RailDistance(TileAddByDiagDir(start, ddir), end) << 8),
            .vertex = v,
        });
        g.insert(std::make_pair(v, 0));
    }

    auto res = Vertex(start, INVALID_DIR, INVALID_DIAGDIR, max_curve, max_curve);
    auto min_distance = UINT32_MAX;
    uint checked = 0;
    auto max_weight = (4 * RailDistance(start, end) / 3 + 20) << 8;

    while (!q.empty()) {
        auto x = q.top();
        checked++;
        q.pop();

        auto gp = g.find(x.vertex);
        auto xw = gp->second;

        // fprintf(stderr, "Q %d %d %d\n", (int)x.weight, (int)x.vertex.tile, (int)x.vertex.ddir);

        // TODO check borders
        auto tile = TileAddByDiagDir(x.vertex.tile, x.vertex.side);
        if (tile == end) {
            res = x.vertex;
            break;
        }

        auto d = RailDistance(tile, end) << 8;
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
                .weight = w + (RailDistance(TileAddByDiagDir(v.tile, v.side), end) << 8),
                .vertex = v
            });

            auto pp = prev.insert(std::make_pair(v, x.vertex));
            if (!pp.second) pp.first->second = x.vertex;
        }
    }

    if (res.side == INVALID_DIAGDIR)
        return polyrail;

    while (res.tile != start) {
        auto p = prev.find(res);
        if (p == prev.end()) break;

        polyrail.tiles.push_back(std::make_pair(res.tile, _highlight_style_from_ddirs[(*p).second.side][res.side]));
        res = (*p).second;
    }

    fprintf(stderr, "POLY %d %d %u\n", (int)start, (int)end, checked);

    return polyrail;
}

void UpdatePolyrailDrawstyle(Point pt) {
    if (pt.x == -1) return;
    _thd.selend = pt;
    auto start = TileVirtXY(_thd.selstart.x, _thd.selstart.y);
    if (!start) return;
    auto end = TileVirtXY(pt.x, pt.y);
    _thd.selend = pt;
    if (_thd.cm_polyrail.start == start && _thd.cm_polyrail.end == end)
        return;
    _thd.cm_new_polyrail = MakePolyrail(start, end);
    fprintf(stderr, "NEW POLY %d %d %d\n", (int)start, (int)end, (int)_thd.cm_new_polyrail.tiles.size());
}

void SetPolyrailSelectionTilesDirty() {
    for (auto p : _thd.cm_polyrail.tiles) {
        MarkTileDirtyByTile(p.first);
    }
}

void DrawPolyrailTileSelection(const TileInfo *ti) {
    if (_thd.drawstyle != CM_HT_RAIL) return;
    // TODO increase effeciency
    for (auto p : _thd.cm_polyrail.tiles) {
        if (p.first == ti->tile) {
            StaticDrawAutorailSelection(ti, p.second, PALETTE_SEL_TILE_BLUE);
        }
    }
}

} // namespace citymania
