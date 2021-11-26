#include "../stdafx.h"

#include "cm_blueprint.hpp"

#include "cm_commands.hpp"
#include "cm_highlight.hpp"

#include "../command_func.h"
#include "../debug.h"
#include "../direction_type.h"
#include "../rail_map.h"
#include "../station_map.h"
#include "../station_base.h"
#include "../tilearea_type.h"
#include "../tunnelbridge_map.h"
#include "../network/network.h"

#include <map>

extern TileHighlightData _thd;
extern RailType _cur_railtype;
extern void GetStationLayout(byte *layout, uint numtracks, uint plat_len, const StationSpec *statspec);

namespace citymania {


std::pair<TileIndex, sp<Blueprint>> _active_blueprint = std::make_pair(INVALID_TILE, nullptr);

TileIndexDiffC operator+(const TileIndexDiffC &a, const TileIndexDiffC &b) {
    return TileIndexDiffC{(int16)(a.x + b.x), (int16)(a.y + b.y)};
}

bool operator==(const TileIndexDiffC &a, const TileIndexDiffC &b) {
    return a.x == b.x && a.y == b.y;
}

bool operator!=(const TileIndexDiffC &a, const TileIndexDiffC &b) {
    return a.x != b.x || a.y != b.y;
}

template<typename Func>
void IterateStation(TileIndex start_tile, Axis axis, byte numtracks, byte plat_len, Func visitor) {
    auto plat_delta = (axis == AXIS_X ? TileDiffXY(1, 0) : TileDiffXY(0, 1));
    auto track_delta = (axis == AXIS_Y ? TileDiffXY(1, 0) : TileDiffXY(0, 1));
    TileIndex tile_track = start_tile;
    do {
        TileIndex tile = tile_track;
        int w = plat_len;
        do {
            visitor(tile);
            tile += plat_delta;
        } while (--w);
        tile_track += track_delta;
    } while (--numtracks);
}

void Blueprint::Add(TileIndex source_tile, Blueprint::Item item) {
    this->items.push_back(item);
    switch (item.type) {
        case Item::Type::RAIL_TRACK: {
            auto tdir = item.u.rail.track.start_dir;
            for (auto i = 0; i < item.u.rail.track.length; i++) {
                this->source_tiles.insert(source_tile);
                source_tile = TileAddByDiagDir(source_tile, TrackdirToExitdir(tdir));
                tdir = NextTrackdir(tdir);
            }
            break;
        }
        case Item::Type::RAIL_STATION_PART:
            IterateStation(source_tile, item.u.rail.station_part.axis, item.u.rail.station_part.numtracks, item.u.rail.station_part.plat_len,
                [&](TileIndex tile) {
                    this->source_tiles.insert(tile);
                }
            );
            break;
        case Item::Type::RAIL_BRIDGE:
            this->source_tiles.insert(TILE_ADDXY(source_tile, item.u.rail.bridge.other_end.x, item.u.rail.bridge.other_end.y));
            this->source_tiles.insert(source_tile);
            break;
        case Item::Type::RAIL_TUNNEL:
            this->source_tiles.insert(TILE_ADDXY(source_tile, item.u.rail.tunnel.other_end.x, item.u.rail.tunnel.other_end.y));
            this->source_tiles.insert(source_tile);
            break;
        case Item::Type::RAIL_DEPOT:
        case Item::Type::RAIL_STATION:
            this->source_tiles.insert(source_tile);
            break;
        case Item::Type::RAIL_SIGNAL: // tile is added for track anyway
            break;
        default:
            NOT_REACHED();
    }
}

CommandContainer GetBlueprintCommand(TileIndex start, const Blueprint::Item &item) {
    static const Track SIGNAL_POS_TRACK[] = {
        TRACK_LEFT, TRACK_LEFT, TRACK_RIGHT, TRACK_RIGHT,
        TRACK_UPPER, TRACK_UPPER, TRACK_LOWER, TRACK_LOWER,
        TRACK_X, TRACK_X, TRACK_Y, TRACK_Y,
    };
    static const uint SIGNAL_POS_NUM[] = {
        1, 0, 1, 0,
        0, 1, 0, 1,
        0, 1, 0, 1,
    };

    switch (item.type) {
        case Blueprint::Item::Type::RAIL_TRACK: {
            // TODO precalc
            auto start_tile = AddTileIndexDiffCWrap(start, item.tdiff);
            auto end_tile = start_tile;
            auto tdir = item.u.rail.track.start_dir;
            for (auto i = 1; i < item.u.rail.track.length; i++) {
                auto new_tile = AddTileIndexDiffCWrap(end_tile, TileIndexDiffCByDiagDir(TrackdirToExitdir(tdir)));
                if (new_tile == INVALID_TILE) break;
                end_tile = new_tile;
                tdir = NextTrackdir(tdir);
            }
            return CommandContainer {
                start_tile,
                end_tile,
                (uint32)(_cur_railtype) | ((uint32)TrackdirToTrack(item.u.rail.track.start_dir) << 6),
                CMD_BUILD_RAILROAD_TRACK,
                nullptr, ""
            };
        }
        case Blueprint::Item::Type::RAIL_DEPOT:
            return CommandContainer {
                AddTileIndexDiffCWrap(start, item.tdiff),
                _cur_railtype,
                item.u.rail.depot.ddir,
                CMD_BUILD_TRAIN_DEPOT,
                nullptr, ""
            };
        case Blueprint::Item::Type::RAIL_TUNNEL:
            // TODO check that other end is where it should be
            return CommandContainer {
                AddTileIndexDiffCWrap(start, item.tdiff),
                (uint32)_cur_railtype | ((uint32)TRANSPORT_RAIL << 8),
                0,
                CMD_BUILD_TUNNEL,
                nullptr, ""
            };
        case Blueprint::Item::Type::RAIL_BRIDGE:
            return CommandContainer {
                AddTileIndexDiffCWrap(start, item.u.rail.bridge.other_end),
                AddTileIndexDiffCWrap(start, item.tdiff),
                item.u.rail.bridge.type | (_cur_railtype << 8) | (TRANSPORT_RAIL << 15),
                CMD_BUILD_BRIDGE,
                nullptr, ""
            };
        case Blueprint::Item::Type::RAIL_STATION_PART:
            return CommandContainer {
                AddTileIndexDiffCWrap(start, item.tdiff),
                (uint32)_cur_railtype
                    | ((uint32)item.u.rail.station_part.axis << 6)
                    | ((uint32)item.u.rail.station_part.numtracks << 8)
                    | ((uint32)item.u.rail.station_part.plat_len << 16),
                (uint32)NEW_STATION << 16,
                CMD_BUILD_RAIL_STATION,
                nullptr, ""
            };
        case Blueprint::Item::Type::RAIL_STATION:
            // TODO station types
            return CommandContainer {
                AddTileIndexDiffCWrap(start, item.tdiff),
                (uint32)_cur_railtype | ((uint32)1 << 8) | ((uint32)1 << 16) | ((uint32)1 << 24),
                (uint32)NEW_STATION << 16,
                CMD_BUILD_RAIL_STATION,
                nullptr, ""
            };
        case Blueprint::Item::Type::RAIL_SIGNAL:
            return CommandContainer {
                AddTileIndexDiffCWrap(start, item.tdiff),
                SIGNAL_POS_TRACK[item.u.rail.signal.pos]
                    | (item.u.rail.signal.variant << 4)
                    | (item.u.rail.signal.type << 5)
                    | (SIGNAL_POS_NUM[item.u.rail.signal.pos] + (item.u.rail.signal.type <= SIGTYPE_LAST_NOPBS && !item.u.rail.signal.twoway ? 1 : 0)) << 15
                    | 1 << 17,
                0,
                CMD_BUILD_SIGNALS,
                nullptr, ""
            };
        default:
            NOT_REACHED();
    }
    NOT_REACHED();
}

std::multimap<TileIndex, ObjectTileHighlight> Blueprint::GetTiles(TileIndex tile) {
    std::multimap<TileIndex, ObjectTileHighlight> res;
    if (tile == INVALID_TILE) return res;
    auto add_tile = [&res](TileIndex tile, const ObjectTileHighlight &ohl) {
        if (tile == INVALID_TILE) return;
        res.emplace(tile, ohl);
    };

    std::set<StationID> can_build_station_sign;
    for (auto &item: this->items) {
        if (item.type != Item::Type::RAIL_STATION) continue;
        if (CanBuild(GetBlueprintCommand(tile, item)))
            can_build_station_sign.insert(item.u.rail.station.id);
    }

    for (auto &o: this->items) {
        auto otile = AddTileIndexDiffCWrap(tile, o.tdiff);
        auto palette = CM_PALETTE_TINT_WHITE;
        if (o.type != Item::Type::RAIL_SIGNAL && !CanBuild(GetBlueprintCommand(tile, o)))
            palette = CM_PALETTE_TINT_RED_DEEP;

        switch(o.type) {
            case Item::Type::RAIL_TRACK: {
                auto end_tile = otile;
                auto tdir = o.u.rail.track.start_dir;
                for (auto i = 0; i < o.u.rail.track.length; i++) {
                    add_tile(end_tile, ObjectTileHighlight::make_rail_track(palette, TrackdirToTrack(tdir)));
                    auto new_tile = AddTileIndexDiffCWrap(end_tile, TileIndexDiffCByDiagDir(TrackdirToExitdir(tdir)));
                    if (new_tile == INVALID_TILE) break;
                    end_tile = new_tile;
                    tdir = NextTrackdir(tdir);
                }
                break;
            }
            case Item::Type::RAIL_BRIDGE:
                add_tile(otile, ObjectTileHighlight::make_rail_bridge_head(palette, o.u.rail.bridge.ddir, o.u.rail.bridge.type));
                add_tile(AddTileIndexDiffCWrap(tile, o.u.rail.bridge.other_end), ObjectTileHighlight::make_rail_bridge_head(palette, ReverseDiagDir(o.u.rail.bridge.ddir), o.u.rail.bridge.type));
                break;
            case Item::Type::RAIL_TUNNEL:
                add_tile(otile, ObjectTileHighlight::make_rail_tunnel_head(palette, o.u.rail.tunnel.ddir));
                add_tile(AddTileIndexDiffCWrap(tile, o.u.rail.tunnel.other_end), ObjectTileHighlight::make_rail_tunnel_head(palette, ReverseDiagDir(o.u.rail.tunnel.ddir)));
                break;
            case Item::Type::RAIL_DEPOT:
                add_tile(otile, ObjectTileHighlight::make_rail_depot(palette, o.u.rail.depot.ddir));
                break;
            case Item::Type::RAIL_STATION_PART: {
                auto layout_ptr = AllocaM(byte, (int)o.u.rail.station_part.numtracks * o.u.rail.station_part.plat_len);
                GetStationLayout(layout_ptr, o.u.rail.station_part.numtracks, o.u.rail.station_part.plat_len, nullptr);
                if (palette == CM_PALETTE_TINT_WHITE && can_build_station_sign.find(o.u.rail.station_part.id) == can_build_station_sign.end())
                    palette = CM_PALETTE_TINT_ORANGE_DEEP;
                IterateStation(otile, o.u.rail.station_part.axis, o.u.rail.station_part.numtracks, o.u.rail.station_part.plat_len,
                    [&](TileIndex tile) {
                        byte layout = *layout_ptr++;
                        add_tile(tile, ObjectTileHighlight::make_rail_station(palette, o.u.rail.station_part.axis, layout & ~1));
                    }
                );
                break;
            }
            case Item::Type::RAIL_SIGNAL:
                add_tile(otile, ObjectTileHighlight::make_rail_signal(CM_PALETTE_TINT_WHITE, o.u.rail.signal.pos, o.u.rail.signal.type, o.u.rail.signal.variant));
                if (o.u.rail.signal.twoway)
                    add_tile(otile, ObjectTileHighlight::make_rail_signal(CM_PALETTE_TINT_WHITE, o.u.rail.signal.pos | 1, o.u.rail.signal.type, o.u.rail.signal.variant));
                break;
            case Item::Type::RAIL_STATION:
                break;
            default:
                NOT_REACHED();
        }
    }
    return res;
}

sp<Blueprint> Blueprint::Rotate() {
    static const Trackdir ROTATE_TRACKDIR[] = {
        TRACKDIR_Y_SE, TRACKDIR_X_SW,
        TRACKDIR_RIGHT_S, TRACKDIR_LEFT_S,
        TRACKDIR_UPPER_W, TRACKDIR_LOWER_W,
        TRACKDIR_RVREV_SE, TRACKDIR_RVREV_SW,
        TRACKDIR_Y_NW, TRACKDIR_X_NE,
        TRACKDIR_RIGHT_N, TRACKDIR_LEFT_N,
        TRACKDIR_UPPER_E, TRACKDIR_LOWER_E,
        TRACKDIR_RVREV_NW, TRACKDIR_RVREV_NE
    };
    static const uint ROTATE_SIGNAL_POS[] = {
        // 5, 4, 7, 6,
        // 2, 3, 0, 1,
        // 11, 10, 8, 9
        5, 4, 7, 6,
        2, 3, 0, 1,
        10, 11, 9, 8
    };
    auto res = std::make_shared<Blueprint>();

    auto rotate = [](TileIndexDiffC td) -> TileIndexDiffC {
        return {static_cast<int16>(td.y), static_cast<int16>(-td.x)};
    };

    auto rotate_dir = [](DiagDirection ddir) -> DiagDirection {
        return ChangeDiagDir(ddir, DIAGDIRDIFF_90RIGHT);
    };

    for (auto &o: this->items) {
        auto odiff = rotate(o.tdiff);
        Blueprint::Item bi(o.type, odiff);
        switch(o.type) {
            case Item::Type::RAIL_TRACK:
                bi.u.rail.track.length = o.u.rail.track.length;
                bi.u.rail.track.start_dir = ROTATE_TRACKDIR[o.u.rail.track.start_dir];
                break;
            case Item::Type::RAIL_BRIDGE:
                bi.u.rail.bridge.type = o.u.rail.bridge.type;
                bi.u.rail.bridge.ddir = rotate_dir(o.u.rail.bridge.ddir);
                bi.u.rail.bridge.other_end = rotate(o.u.rail.bridge.other_end);
                break;
            case Item::Type::RAIL_TUNNEL:
                bi.u.rail.tunnel.ddir = rotate_dir(o.u.rail.tunnel.ddir);
                bi.u.rail.tunnel.other_end = rotate(o.u.rail.tunnel.other_end);
                break;
            case Item::Type::RAIL_DEPOT:
                bi.u.rail.depot.ddir = rotate_dir(o.u.rail.depot.ddir);
                break;
            case Item::Type::RAIL_STATION:
                bi.u.rail.station.id = o.u.rail.station.id;
                bi.u.rail.station.has_part = o.u.rail.station.has_part;
                break;
            case Item::Type::RAIL_STATION_PART: {
                auto ediff = rotate({
                    (int16)(o.tdiff.x + (o.u.rail.station_part.axis == AXIS_X ? o.u.rail.station_part.plat_len : o.u.rail.station_part.numtracks) - 1),
                    (int16)(o.tdiff.y + (o.u.rail.station_part.axis == AXIS_X ? o.u.rail.station_part.numtracks : o.u.rail.station_part.plat_len) - 1),
                });
                bi.tdiff = {std::min(odiff.x, ediff.x), std::min(odiff.y, ediff.y)};
                bi.u.rail.station_part.id = o.u.rail.station_part.id;
                bi.u.rail.station_part.axis = OtherAxis(o.u.rail.station_part.axis);
                bi.u.rail.station_part.numtracks = o.u.rail.station_part.numtracks;
                bi.u.rail.station_part.plat_len = o.u.rail.station_part.plat_len;
                break;
            }
            case Item::Type::RAIL_SIGNAL:
                bi.u.rail.signal.pos = ROTATE_SIGNAL_POS[o.u.rail.signal.pos]; // TODO rotate
                bi.u.rail.signal.type = o.u.rail.signal.type;
                bi.u.rail.signal.variant = o.u.rail.signal.variant;
                break;
            default:
                NOT_REACHED();
        }
        res->items.push_back(bi);
    }
    res->source_tiles = this->source_tiles;
    return res;
}


static void BlueprintAddSignals(sp<Blueprint> &blueprint, TileIndex tile, TileIndexDiffC tdiff) {
    // reference: DrawSignals @ rail_cmd.cpp

    auto add = [&](Track track, uint x, uint pos) {
        auto a = IsSignalPresent(tile, x);
        auto b = IsSignalPresent(tile, x ^ 1);
        if (!a && !b) return;
        if (!a) pos = pos | 1;
        Blueprint::Item bi(Blueprint::Item::Type::RAIL_SIGNAL, tdiff);
        bi.u.rail.signal.pos = pos;
        bi.u.rail.signal.type = GetSignalType(tile, track);
        bi.u.rail.signal.variant = GetSignalVariant(tile, track);
        bi.u.rail.signal.twoway = a && b;
        blueprint->Add(tile, bi);
    };
    auto rails = GetTrackBits(tile);
    if (!(rails & TRACK_BIT_Y)) {
        if (!(rails & TRACK_BIT_X)) {
            if (rails & TRACK_BIT_LEFT) add(TRACK_LEFT, 2, 0);
            if (rails & TRACK_BIT_RIGHT) add(TRACK_RIGHT, 0, 2);
            if (rails & TRACK_BIT_UPPER) add(TRACK_UPPER, 3, 4);
            if (rails & TRACK_BIT_LOWER) add(TRACK_LOWER, 1, 6);
        } else {
            add(TRACK_X, 3, 8);
        }
    } else {
        add(TRACK_Y, 3, 10);
    }
}

static void BlueprintAddTracks(sp<Blueprint> &blueprint, TileIndex tile, TileIndexDiffC tdiff, TileArea &area,
                               std::map<TileIndex, Track> &track_tiles) {
    // tilearea is iterated by x and y so chose direction to go in uniterated area
    static const Trackdir _track_iterate_dir[TRACK_END] = { TRACKDIR_X_SW, TRACKDIR_Y_SE, TRACKDIR_UPPER_E, TRACKDIR_LOWER_E, TRACKDIR_LEFT_S, TRACKDIR_RIGHT_S};

    for (Track track = TRACK_BEGIN; track < TRACK_END; track++) {
        TileIndex c_tile = tile;
        TileIndex end_tile = INVALID_TILE;
        uint16 length = 0;
        Track c_track = track;
        Trackdir c_tdir = _track_iterate_dir[track];
        while (IsPlainRailTile(c_tile) && HasBit(GetTrackBits(c_tile), c_track)) {
            if (HasBit(track_tiles[c_tile], c_track)) break;
            length++;
            SetBit(track_tiles[c_tile], c_track);
            end_tile = c_tile;
            c_tile = TileAddByDiagDir(c_tile, TrackdirToExitdir(c_tdir));
            if (!area.Contains(c_tile)) break;
            c_tdir = NextTrackdir(c_tdir);
            c_track = TrackdirToTrack(c_tdir);
        }
        if (end_tile == INVALID_TILE) continue;
        Blueprint::Item bi(Blueprint::Item::Type::RAIL_TRACK, tdiff);
        bi.u.rail.track.length = length;
        bi.u.rail.track.start_dir = _track_iterate_dir[track];
        blueprint->Add(tile, bi);
    }
}

void BlueprintCopyArea(TileIndex start, TileIndex end) {
    // if (start > end) Swap(start, end);
    TileArea ta{start, end};
    start = ta.tile;

    ResetActiveBlueprint();

    auto blueprint = std::make_shared<Blueprint>();

    std::map<TileIndex, Track> track_tiles;
    std::multimap<StationID, TileIndex> station_tiles;
    std::set<StationID> stations;

    for (TileIndex tile : ta) {
        TileIndexDiffC td = TileIndexToTileIndexDiffC(tile, start);
        switch (GetTileType(tile)) {
            case MP_STATION:
                if (IsRailStation(tile)) {
                    stations.insert(GetStationIndex(tile));
                    if (stations.size() > 10) return;
                }
                break;

            case MP_RAILWAY:
                switch (GetRailTileType(tile)) {
                    case RAIL_TILE_DEPOT: {
                        Blueprint::Item bi(Blueprint::Item::Type::RAIL_DEPOT, td);
                        bi.u.rail.depot.ddir = GetRailDepotDirection(tile);
                        blueprint->Add(tile, bi);
                        break;
                    }
                    case RAIL_TILE_SIGNALS:
                        BlueprintAddSignals(blueprint, tile, td);
                        FALLTHROUGH;
                    case RAIL_TILE_NORMAL:
                        BlueprintAddTracks(blueprint, tile, td, ta, track_tiles);
                        break;
                    default:
                        NOT_REACHED();
                }
                break;
            case MP_TUNNELBRIDGE: {
                if (GetTunnelBridgeTransportType(tile) != TRANSPORT_RAIL) break;
                auto other = GetOtherTunnelBridgeEnd(tile);
                if (!ta.Contains(other)) break;
                if (other < tile) break;
                Blueprint::Item bi(Blueprint::Item::Type::RAIL_TUNNEL, td);
                if (!IsTunnel(tile)) {
                    bi.type = Blueprint::Item::Type::RAIL_BRIDGE;
                    bi.u.rail.bridge.type = GetBridgeType(tile);
                    bi.u.rail.bridge.ddir = GetTunnelBridgeDirection(tile);
                    bi.u.rail.bridge.other_end = TileIndexToTileIndexDiffC(other, start);
                } else {
                    bi.u.rail.tunnel.ddir = GetTunnelBridgeDirection(tile);
                    bi.u.rail.tunnel.other_end = TileIndexToTileIndexDiffC(other, start);
                }
                blueprint->Add(tile, bi);
                break;
            }
            default:
                break;
        }
        if (blueprint->items.size() > 200) return;
    }

    for (auto sid : stations) {
        auto st = Station::Get(sid);

        if (!ta.Contains(st->xy)) continue;

        TileArea sta(TileXY(st->rect.left, st->rect.top), TileXY(st->rect.right, st->rect.bottom));
        bool in_area = true;
        bool sign_part = false;
        std::vector<TileIndex> tiles;
        for (TileIndex tile : sta) {
            if (!IsTileType(tile, MP_STATION) || GetStationIndex(tile) != sid || !IsRailStation(tile)) continue;
            if (!ta.Contains(tile)) {
                in_area = false;
                break;
            }
            tiles.push_back(tile);
            if (tile == st->xy) sign_part = true;
        }

        if (!in_area) continue;

        Blueprint::Item bi(Blueprint::Item::Type::RAIL_STATION, TileIndexToTileIndexDiffC(st->xy, start));
        bi.u.rail.station.id = sid;
        bi.u.rail.station.has_part = sign_part;
        blueprint->Add(st->xy, bi);

        std::set<TileIndex> added_tiles;
        for (auto tile : tiles) {
            if (added_tiles.find(tile) != added_tiles.end())
                continue;

            auto axis = GetRailStationAxis(tile);
            auto platform_delta = (axis == AXIS_X ? TileDiffXY(1, 0) : TileDiffXY(0, 1));
            auto track_delta = (axis == AXIS_Y ? TileDiffXY(1, 0) : TileDiffXY(0, 1));
            auto plat_len = 0;

            auto can_add_tile = [=](TileIndex tile) {
                return (IsValidTile(tile)
                    && IsTileType(tile, MP_STATION)
                    && GetStationIndex(tile) == sid
                    && IsRailStation(tile)
                    && GetRailStationAxis(tile) == axis);
            };

            auto cur_tile = tile;
            do {
                plat_len++;
                cur_tile = TILE_ADD(cur_tile, platform_delta);
            } while (can_add_tile(cur_tile));

            auto numtracks = 0;
            cur_tile = tile;
            bool can_add = false;
            do {
                numtracks++;
                cur_tile = TILE_ADD(cur_tile, track_delta);
                can_add = true;
                auto plat_tile = cur_tile;
                for (auto i = 0; i < plat_len; i++) {
                    if (!can_add_tile(plat_tile)) {
                        can_add = false;
                        break;
                    }
                    plat_tile = TILE_ADD(plat_tile, platform_delta);
                }
            } while (can_add);

            cur_tile = tile;
            for (auto i = 0; i < numtracks; i++) {
                auto plat_tile = cur_tile;
                for (auto j = 0; j < plat_len; j++) {
                    added_tiles.insert(plat_tile);
                    plat_tile = TILE_ADD(plat_tile, platform_delta);
                }
                cur_tile = TILE_ADD(cur_tile, track_delta);
            }

            Blueprint::Item bi(Blueprint::Item::Type::RAIL_STATION_PART, TileIndexToTileIndexDiffC(tile, start));
            bi.u.rail.station_part.id = sid;
            bi.u.rail.station_part.axis = axis;
            bi.u.rail.station_part.numtracks = numtracks;
            bi.u.rail.station_part.plat_len = plat_len;
            blueprint->Add(tile, bi);
        }
    }

    _active_blueprint = std::make_pair(start, blueprint);
}


void UpdateBlueprintTileSelection(Point pt, TileIndex tile) {
    if (tile == INVALID_TILE || _active_blueprint.first == INVALID_TILE || !_active_blueprint.second) {
        _thd.cm_new = ObjectHighlight{};
        return;
    }
    _thd.cm_new = ObjectHighlight::make_blueprint(tile, _active_blueprint.second);
}

void ResetActiveBlueprint() {
    _active_blueprint = std::make_pair(INVALID_TILE, nullptr);
}

void SetBlueprintHighlight(const TileInfo *ti, TileHighlight &th) {
    if (_active_blueprint.first == INVALID_TILE || !_active_blueprint.second)
        return;

    if (_active_blueprint.second->HasSourceTile(ti->tile)) {
        th.ground_pal = th.structure_pal = CM_PALETTE_TINT_BLUE;
    }
}

void BuildBlueprint(sp<Blueprint> &blueprint, TileIndex start) {
    CommandContainer last_rail = {INVALID_TILE, 0, 0, CMD_END, nullptr, ""};
    for (auto &item : blueprint->items) {
        switch (item.type) {
            case Blueprint::Item::Type::RAIL_TRACK:
            case Blueprint::Item::Type::RAIL_DEPOT:
            case Blueprint::Item::Type::RAIL_TUNNEL:
            case Blueprint::Item::Type::RAIL_BRIDGE: {
                auto cc = GetBlueprintCommand(start, item);
                if (item.type == Blueprint::Item::Type::RAIL_TRACK) last_rail = cc;
                DoCommandP(&cc);
                break;
            }
            case Blueprint::Item::Type::RAIL_STATION: {
                // TODO station types
                TileIndex tile = AddTileIndexDiffCWrap(start, item.tdiff);
                auto cc = GetBlueprintCommand(start, item);
                DoCommandWithCallback(cc,
                    [blueprint, tile, start, sign_part=item.u.rail.station.has_part, sid=item.u.rail.station.id] (bool res)->bool {
                        if (!res) return false;
                        StationID station_id = GetStationIndex(tile);
                        for (auto &item : blueprint->items) {
                            if (item.type != Blueprint::Item::Type::RAIL_STATION_PART) continue;
                            if (item.u.rail.station_part.id != sid) continue;
                            auto cc = GetBlueprintCommand(start, item);
                            DoCommandP(cc.tile, cc.p1 | (1 << 24), station_id << 16, cc.cmd);
                        }
                        if (!sign_part) DoCommandP(tile, 0, 0, CMD_REMOVE_FROM_RAIL_STATION);
                        return true;
                    }
                );
                break;
            }
            default:
                break;
        }
    }

    auto signal_callback = [start, blueprint](bool res) {
        for (auto &item : blueprint->items) {
            if (item.type != Blueprint::Item::Type::RAIL_SIGNAL) continue;
            auto cc = GetBlueprintCommand(start, item);
            DoCommandP(&cc);
        }
        return true;
    };
    if (last_rail.cmd != CMD_END) {  // there can't be any signals if there are no rails
        AddCommandCallback(last_rail.tile, last_rail.p1, last_rail.p2, last_rail.cmd, "", signal_callback);
    }
}

void RotateActiveBlueprint() {
    if (!_active_blueprint.second) return;
    _active_blueprint.second = _active_blueprint.second->Rotate();
}

void BuildActiveBlueprint(TileIndex start) {
    if (!_active_blueprint.second) return;
    BuildBlueprint(_active_blueprint.second, start);
}


}  // namespace citymania
