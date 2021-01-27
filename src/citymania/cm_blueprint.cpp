#include "../stdafx.h"

#include "cm_blueprint.hpp"

#include "cm_highlight.hpp"

#include "../command_func.h"
#include "../direction_type.h"
#include "../rail_map.h"
#include "../station_map.h"
#include "../station_base.h"
#include "../tilearea_type.h"
#include "../tunnelbridge_map.h"
#include "../network/network.h"

#include <functional>
#include <map>

extern TileHighlightData _thd;
extern RailType _cur_railtype;

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

typedef std::tuple<TileIndex, uint32, uint32, uint32> CommandTuple;
typedef std::function<void(bool)> CommandCallback;
std::map<CommandTuple, std::vector<CommandCallback>> _command_callbacks;

void AddCommandCallback(TileIndex tile, uint32 p1, uint32 p2, uint32 cmd, CommandCallback callback) {
    _command_callbacks[std::make_tuple(tile, p1, p2, cmd)].push_back(callback);
}

void DoCommandWithCallback(TileIndex tile, uint32 p1, uint32 p2, uint32 cmd, CommandCallback callback) {
    AddCommandCallback(tile, p1, p2, cmd, callback);
    DoCommandP(tile, p1, p2, cmd);
}

void CommandExecuted(bool res, TileIndex tile, uint32 p1, uint32 p2, uint32 cmd) {
    CommandTuple ct {tile, p1, p2, cmd};
    auto p = _command_callbacks.find(ct);
    if (p == _command_callbacks.end()) return;
    for (auto &cb : p->second)
        cb(res);
    _command_callbacks.erase(p);
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
        case Item::Type::RAIL_BRIDGE:
        case Item::Type::RAIL_TUNNEL:
            this->source_tiles.insert(TILE_ADDXY(source_tile, item.u.rail.tunnel.other_end.x, item.u.rail.tunnel.other_end.y));
            FALLTHROUGH;
        case Item::Type::RAIL_DEPOT:
        case Item::Type::RAIL_STATION:
        case Item::Type::RAIL_STATION_PART:
            this->source_tiles.insert(source_tile);
            break;
        case Item::Type::RAIL_SIGNAL: // tile is added for track anyway
            break;
        default:
            NOT_REACHED();
    }
}

std::multimap<TileIndex, ObjectTileHighlight> Blueprint::GetTiles(TileIndex tile) {
    std::multimap<TileIndex, ObjectTileHighlight> res;
    if (tile == INVALID_TILE) return res;
    auto add_tile = [&res](TileIndex tile, const ObjectTileHighlight &ohl) {
        if (tile == INVALID_TILE) return;
        res.emplace(tile, ohl);
    };
    for (auto &o: this->items) {
        auto otile = AddTileIndexDiffCWrap(tile, o.tdiff);
        switch(o.type) {
            case Item::Type::RAIL_TRACK: {
                auto end_tile = otile;
                auto tdir = o.u.rail.track.start_dir;
                for (auto i = 0; i < o.u.rail.track.length; i++) {
                    add_tile(end_tile, ObjectTileHighlight::make_rail_track(TrackdirToTrack(tdir)));
                    end_tile = TileAddByDiagDir(end_tile, TrackdirToExitdir(tdir));
                    tdir = NextTrackdir(tdir);
                }
                break;
            }
            case Item::Type::RAIL_BRIDGE:
                add_tile(otile, ObjectTileHighlight::make_rail_bridge_head(o.u.rail.bridge.ddir, o.u.rail.bridge.type));
                add_tile(AddTileIndexDiffCWrap(tile, o.u.rail.bridge.other_end), ObjectTileHighlight::make_rail_bridge_head(ReverseDiagDir(o.u.rail.bridge.ddir), o.u.rail.bridge.type));
                break;
            case Item::Type::RAIL_TUNNEL:
                add_tile(otile, ObjectTileHighlight::make_rail_tunnel_head(o.u.rail.tunnel.ddir));
                add_tile(AddTileIndexDiffCWrap(tile, o.u.rail.tunnel.other_end), ObjectTileHighlight::make_rail_tunnel_head(ReverseDiagDir(o.u.rail.tunnel.ddir)));
                break;
            case Item::Type::RAIL_DEPOT:
                add_tile(otile, ObjectTileHighlight::make_rail_depot(o.u.rail.depot.ddir));
                break;
            case Item::Type::RAIL_STATION_PART:
                add_tile(otile, ObjectTileHighlight::make_rail_station(o.u.rail.station_part.axis, 0));
                break;
            case Item::Type::RAIL_SIGNAL:
                add_tile(otile, ObjectTileHighlight::make_rail_signal(o.u.rail.signal.pos, o.u.rail.signal.type, o.u.rail.signal.variant));
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
            case Item::Type::RAIL_STATION_PART:
                bi.u.rail.station_part.id = o.u.rail.station_part.id;
                bi.u.rail.station_part.axis = OtherAxis(o.u.rail.station_part.axis);
                break;
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
        if (!IsSignalPresent(tile, x)) return;
        Blueprint::Item bi(Blueprint::Item::Type::RAIL_SIGNAL, tdiff);
        bi.u.rail.signal.pos = pos;
        bi.u.rail.signal.type = GetSignalType(tile, track);
        bi.u.rail.signal.variant = GetSignalVariant(tile, track);
        blueprint->Add(tile, bi);
    };
    auto rails = GetTrackBits(tile);
    if (!(rails & TRACK_BIT_Y)) {
        if (!(rails & TRACK_BIT_X)) {
            if (rails & TRACK_BIT_LEFT) {
                add(TRACK_LEFT, 2, 0);
                add(TRACK_LEFT, 3, 1);
            }
            if (rails & TRACK_BIT_RIGHT) {
                add(TRACK_RIGHT, 0, 2);
                add(TRACK_RIGHT, 1, 3);
            }
            if (rails & TRACK_BIT_UPPER) {
                add(TRACK_UPPER, 3, 4);
                add(TRACK_UPPER, 2, 5);
            }
            if (rails & TRACK_BIT_LOWER) {
                add(TRACK_LOWER, 1, 6);
                add(TRACK_LOWER, 0, 7);
            }
        } else {
            add(TRACK_X, 3, 8);
            add(TRACK_X, 2, 9);
        }
    } else {
        add(TRACK_Y, 3, 10);
        add(TRACK_Y, 2, 11);
    }
}

// void BlueprintCopyArea(TileIndex start, TileIndex end) {
//     TileArea ta{start, end};

//     auto blueprint = std::make_shared<Blueprint>();
//     _active_blueprint = std::make_pair(start, blueprint);

//     TILE_AREA_LOOP(tile, ta) {
//         TileIndexDiff td = tile - start;
//         switch (GetTileType(tile)) {
//             case MP_STATION:
//                 if (IsRailStation(tile))
//                     blueprint->Add(td, ObjectTileHighlight::make_rail_station(GetRailStationAxis(tile)));
//                 break;

//             case MP_RAILWAY:
//                 switch (GetRailTileType(tile)) {
//                     case RAIL_TILE_DEPOT:
//                         blueprint->Add(td, ObjectTileHighlight::make_rail_depot(GetRailDepotDirection(tile)));
//                         break;
//                     case RAIL_TILE_SIGNALS:
//                         BlueprintAddSignals(blueprint, tile, td);
//                         FALLTHROUGH;
//                     case RAIL_TILE_NORMAL: {
//                         auto tb = GetTrackBits(tile);
//                         for (Track track = TRACK_BEGIN; track < TRACK_END; track++) {
//                             if (!HasBit(tb, track)) continue;
//                             blueprint->Add(td, ObjectTileHighlight::make_rail_track(track));
//                         }
//                         break;
//                     }
//                     default:
//                         NOT_REACHED();
//                 }
//                 break;
//             case MP_TUNNELBRIDGE: {
//                 if (GetTunnelBridgeTransportType(tile) != TRANSPORT_RAIL) break;
//                 if (IsTunnel(tile)) break;
//                 auto ddir = GetTunnelBridgeDirection(tile);
//                 auto delta = TileOffsByDiagDir(ddir);
//                 auto other = GetOtherTunnelBridgeEnd(tile);
//                 auto axis = DiagDirToAxis(ddir);
//                 auto type = GetBridgeType(tile);
//                 blueprint->Add(td, ObjectTileHighlight::make_rail_bridge_head(ddir, other, type));
//                 for (auto t = tile + delta; t != other; t += delta) {
//                     // blueprint->Add(t - start, ObjectTileHighlight::make_rail_tunnelbridge(axis));
//                 }
//                 blueprint->Add(other - start, ObjectTileHighlight::make_rail_bridge_head(ReverseDiagDir(ddir), INVALID_TILE, type));
//                 break;
//             }
//             default:
//                 break;
//         }
//     }
// }

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
        // fprintf(stderr, "TTTTTT %u %u %u\n", c_tile, c_track, GetTrackBits(c_tile));
        while (IsPlainRailTile(c_tile) && HasBit(GetTrackBits(c_tile), c_track)) {
            if (HasBit(track_tiles[c_tile], c_track)) break;
            length++;
            SetBit(track_tiles[c_tile], c_track);
            end_tile = c_tile;
            c_tile = TileAddByDiagDir(c_tile, TrackdirToExitdir(c_tdir));
            if (!area.Contains(c_tile)) break;
            c_tdir = NextTrackdir(c_tdir);
            c_track = TrackdirToTrack(c_tdir);
            // fprintf(stderr, "TTTTTTI %u %u %u\n", c_tile, c_track, GetTrackBits(c_tile));
        }
        if (end_tile == INVALID_TILE) continue;
        Blueprint::Item bi(Blueprint::Item::Type::RAIL_TRACK, tdiff);
        bi.u.rail.track.length = length;
        bi.u.rail.track.start_dir = _track_iterate_dir[track];
        // fprintf(stderr, "TTTTTTEE %u %u %u\n", tdiff, bi.u.rail.track.end_diff, bi.u.rail.track.start_dir);
        blueprint->Add(tile, bi);
    }
}

void BlueprintCopyArea(TileIndex start, TileIndex end) {
    // if (start > end) Swap(start, end);
    TileArea ta{start, end};
    start = ta.tile;

    auto blueprint = std::make_shared<Blueprint>();
    _active_blueprint = std::make_pair(start, blueprint);

    std::map<TileIndex, Track> track_tiles;
    std::multimap<StationID, TileIndex> station_tiles;
    std::set<StationID> stations;

    TILE_AREA_LOOP(tile, ta) {
        TileIndexDiffC td = TileIndexToTileIndexDiffC(tile, start);
        switch (GetTileType(tile)) {
            case MP_STATION:
                if (IsRailStation(tile)) stations.insert(GetStationIndex(tile));
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
    }

    for (auto sid : stations) {
        auto st = Station::Get(sid);

        if (!ta.Contains(st->xy)) continue;

        TileArea sta(TileXY(st->rect.left, st->rect.top), TileXY(st->rect.right, st->rect.bottom));
        bool in_area = true;
        bool sign_part = false;
        std::vector<TileIndex> tiles;
        TILE_AREA_LOOP(tile, sta) {
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

        for (auto tile : tiles) {
            Blueprint::Item bi(Blueprint::Item::Type::RAIL_STATION_PART, TileIndexToTileIndexDiffC(tile, start));
            bi.u.rail.station_part.id = sid;
            bi.u.rail.station_part.axis = GetRailStationAxis(tile);
            blueprint->Add(tile, bi);
        }
    }
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
        th.ground_pal = th.structure_pal = PALETTE_TINT_BLUE;
    }
}

void BuildBlueprint(sp<Blueprint> &blueprint, TileIndex start) {
    TileIndex last_tile;
    uint32 last_p1, last_p2, last_cmd = CMD_END;
    for (auto &item : blueprint->items) {
        switch (item.type) {
            case Blueprint::Item::Type::RAIL_TRACK: {
                auto start_tile = AddTileIndexDiffCWrap(start, item.tdiff);
                auto end_tile = start_tile;
                auto tdir = item.u.rail.track.start_dir;
                for (auto i = 1; i < item.u.rail.track.length; i++) {
                    end_tile = TileAddByDiagDir(end_tile, TrackdirToExitdir(tdir));
                    tdir = NextTrackdir(tdir);
                }
                DoCommandP(
                    last_tile = start_tile,
                    last_p1 = end_tile,
                    last_p2 = (uint32)(_cur_railtype | (TrackdirToTrack(item.u.rail.track.start_dir) << 6)),
                    last_cmd = CMD_BUILD_RAILROAD_TRACK
                );
                break;
            }
            case Blueprint::Item::Type::RAIL_DEPOT:
                DoCommandP(
                    AddTileIndexDiffCWrap(start, item.tdiff),
                    _cur_railtype,
                    item.u.rail.depot.ddir,
                    CMD_BUILD_TRAIN_DEPOT
                );
                break;
            case Blueprint::Item::Type::RAIL_TUNNEL:
                // TODO check that other end is where it should be
                DoCommandP(
                    AddTileIndexDiffCWrap(start, item.tdiff),
                    _cur_railtype | (TRANSPORT_RAIL << 8),
                    0,
                    CMD_BUILD_TUNNEL
                );
                break;
            case Blueprint::Item::Type::RAIL_BRIDGE: {
                DoCommandP(
                    AddTileIndexDiffCWrap(start, item.u.rail.bridge.other_end),
                    AddTileIndexDiffCWrap(start, item.tdiff),
                    item.u.rail.bridge.type | (_cur_railtype << 8) | (TRANSPORT_RAIL << 15),
                    CMD_BUILD_BRIDGE
                );
                break;
            }
            case Blueprint::Item::Type::RAIL_STATION: {
                // TODO station types
                TileIndex tile = AddTileIndexDiffCWrap(start, item.tdiff);
                DoCommandWithCallback(
                    tile,
                    _cur_railtype | (1 << 8) | (1 << 16) | (1 << 24),
                    NEW_STATION << 16,
                    CMD_BUILD_RAIL_STATION,
                    [&blueprint, tile, start, sign_part=item.u.rail.station.has_part, sid=item.u.rail.station.id] (bool res) {
                        if (!res) return;
                        StationID station_id = GetStationIndex(tile);
                        for (auto &item : blueprint->items) {
                            if (item.type != Blueprint::Item::Type::RAIL_STATION_PART) continue;
                            if (item.u.rail.station_part.id != sid) continue;
                            DoCommandP(
                                AddTileIndexDiffCWrap(start, item.tdiff),
                                _cur_railtype | (item.u.rail.station_part.axis << 6) | (1 << 8) | (1 << 16) | (1 << 24),
                                station_id << 16,
                                CMD_BUILD_RAIL_STATION
                            );
                        }
                        if (!sign_part) DoCommandP(tile, 0, 0, CMD_REMOVE_FROM_RAIL_STATION);
                    }
                );
                break;
            }
            default:
                break;
        }
    }

    auto signal_callback = [start, &blueprint](bool res) {
        static const Track SIGNAL_POS_TRACK[] = {
            TRACK_LEFT, TRACK_LEFT, TRACK_RIGHT, TRACK_RIGHT,
            TRACK_UPPER, TRACK_UPPER, TRACK_LOWER, TRACK_LOWER,
            TRACK_X, TRACK_X, TRACK_Y, TRACK_Y,
        };
        for (auto &item : blueprint->items) {
            if (item.type != Blueprint::Item::Type::RAIL_SIGNAL) continue;
            DoCommandP(
                AddTileIndexDiffCWrap(start, item.tdiff),
                SIGNAL_POS_TRACK[item.u.rail.signal.pos] | (item.u.rail.signal.variant << 4) | (item.u.rail.signal.type << 5)
                    | ((item.u.rail.signal.pos % 2) << 15),
                0,
                CMD_BUILD_SIGNALS
            );
        }
    };
    if (last_cmd != CMD_END) {  // there can't be any signals if there are no rails
        if (_networking) AddCommandCallback(last_tile, last_p1, last_p2, last_cmd, signal_callback);
        else signal_callback(true);
    }
}

void RotateActiveBlueprint() {
    _active_blueprint.second = _active_blueprint.second->Rotate();
}

void BuildActiveBlueprint(TileIndex start) {
    BuildBlueprint(_active_blueprint.second, start);
}


}  // namespace citymania
