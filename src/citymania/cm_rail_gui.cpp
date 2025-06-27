#include "../stdafx.h"

#include "cm_rail_gui.hpp"

#include "cm_commands.hpp"
#include "cm_hotkeys.hpp"

#include "../rail_cmd.h"
#include "../tilehighlight_func.h"
#include "../tile_map.h"

#include "../safeguards.h"

extern TileIndex _cm_rail_track_endtile; // rail_cmd.cpp

namespace citymania {

static sp<Command> GenericPlaceRail(TileIndex tile, RailType railtype, Track track, bool remove_mode)
{
    if (remove_mode) {
        auto res = std::make_shared<cmd::RemoveSingleRail>(tile, track);
        res->with_error(STR_ERROR_CAN_T_REMOVE_RAILROAD_TRACK);
        return res;
    } else {
        auto res = std::make_shared<cmd::BuildSingleRail>(tile, railtype, track, _settings_client.gui.auto_remove_signals);
        res->with_error(STR_ERROR_CAN_T_BUILD_RAILROAD_TRACK);
        return res;
    }
}

static sp<Command> DoRailroadTrack(TileIndex start_tile, TileIndex end_tile, RailType railtype, Track track, bool remove_mode)
{
    if (remove_mode) {
        auto res = std::make_shared<cmd::RemoveRailroadTrack>(end_tile, start_tile, track);
        res->with_error(STR_ERROR_CAN_T_REMOVE_RAILROAD_TRACK);
        return res;
    } else {
        auto res = std::make_shared<cmd::BuildRailroadTrack>(end_tile, start_tile, railtype, track, _settings_client.gui.auto_remove_signals, false);
        res->with_error(STR_ERROR_CAN_T_BUILD_RAILROAD_TRACK);
        return res;
    }
}


static bool DoAutodirTerraform(bool diagonal, TileIndex start_tile, TileIndex /* end_tile */, Track track, sp<Command> rail_cmd, TileIndex s1, TileIndex e1, TileIndex s2, TileIndex e2, bool /* remove_mode */) {
    auto rail_callback = [rail_cmd, start_tile, track, estimate=citymania::_estimate_mod](bool res) -> bool{
        if (rail_cmd->call({DoCommandFlag::Auto, DoCommandFlag::NoWater}).GetErrorMessage() != STR_ERROR_ALREADY_BUILT ||
                _cm_rail_track_endtile == INVALID_TILE) {
            if (!rail_cmd->post()) return false;
        }
        if (!estimate && _cm_rail_track_endtile != INVALID_TILE)
            StoreRailPlacementEndpoints(start_tile, _cm_rail_track_endtile, track, true);
        return res;
    };

    auto h1 = TileHeight(s1);
    auto h2 = TileHeight(s2);
    auto cmd1 = cmd::LevelLand(e1, s1, diagonal, h1 < h2 ? LM_RAISE : LM_LEVEL);
    auto cmd2 = cmd::LevelLand(e2, s2, diagonal, h2 < h1 ? LM_RAISE : LM_LEVEL);
    auto c1_fail = cmd1.call({DoCommandFlag::Auto, DoCommandFlag::NoWater}).Failed();
    auto c2_fail = cmd2.call({DoCommandFlag::Auto, DoCommandFlag::NoWater}).Failed();
    if (c1_fail && c2_fail) return rail_callback(true);
    if (c2_fail) return cmd1.with_callback(rail_callback).post();
    if (!c1_fail) cmd1.post();
    return cmd2.with_callback(rail_callback).post();
}

static bool HandleAutodirTerraform(TileIndex start_tile, TileIndex end_tile, Track track, sp<Command> rail_cmd, bool remove_mode) {
    bool eq = (TileX(end_tile) - TileY(end_tile) == TileX(start_tile) - TileY(start_tile));
    bool ez = (TileX(end_tile) + TileY(end_tile) == TileX(start_tile) + TileY(start_tile));
    // StoreRailPlacementEndpoints(start_tile, end_tile, track, true);
    switch (_thd.cm_poly_dir) {
        case TRACKDIR_X_NE:
            return DoAutodirTerraform(false, start_tile, end_tile, track, std::move(rail_cmd),
                TileAddXY(start_tile, 1, 0), end_tile,
                TileAddXY(start_tile, 1, 1), TileAddXY(end_tile, 0, 1), remove_mode);
            break;
        case TRACKDIR_X_SW:
            return DoAutodirTerraform(false, start_tile, end_tile, track, std::move(rail_cmd),
                start_tile, TileAddXY(end_tile, 1, 0),
                TileAddXY(start_tile, 0, 1), TileAddXY(end_tile, 1, 1), remove_mode);
            break;
        case TRACKDIR_Y_SE:
            return DoAutodirTerraform(false, start_tile, end_tile, track, std::move(rail_cmd),
                start_tile, TileAddXY(end_tile, 0, 1),
                TileAddXY(start_tile, 1, 0), TileAddXY(end_tile, 1, 1), remove_mode);
            break;
        case TRACKDIR_Y_NW:
            return DoAutodirTerraform(false, start_tile, end_tile, track, std::move(rail_cmd),
                TileAddXY(start_tile, 0, 1), end_tile,
                TileAddXY(start_tile, 1, 1), TileAddXY(end_tile, 1, 0), remove_mode);
            break;
        case TRACKDIR_LEFT_N: {
            return DoAutodirTerraform(true, start_tile, end_tile, track, std::move(rail_cmd),
                TileAddXY(start_tile, 1, 0), TileAddXY(end_tile, eq, 0),
                TileAddXY(start_tile, 1, 1), TileAddXY(end_tile, 0, !eq), remove_mode);
            break;
        }
        case TRACKDIR_RIGHT_N: {
            return DoAutodirTerraform(true, start_tile, end_tile, track, std::move(rail_cmd),
                TileAddXY(start_tile, 0, 1), TileAddXY(end_tile, 0, eq),
                TileAddXY(start_tile, 1, 1), TileAddXY(end_tile, !eq, 0), remove_mode);
            break;
        }
        case TRACKDIR_LEFT_S: {
            return DoAutodirTerraform(true, start_tile, end_tile, track, std::move(rail_cmd),
                TileAddXY(start_tile, 1, 0), TileAddXY(end_tile, 1, !eq),
                start_tile, TileAddXY(end_tile, eq, 1), remove_mode);
            break;
        }
        case TRACKDIR_RIGHT_S: {
            return DoAutodirTerraform(true, start_tile, end_tile, track, std::move(rail_cmd),
                TileAddXY(start_tile, 0, 1), TileAddXY(end_tile, !eq, 1),
                start_tile, TileAddXY(end_tile, 1, eq), remove_mode);
            break;
        }
        case TRACKDIR_UPPER_E: {
            return DoAutodirTerraform(true, start_tile, end_tile, track, std::move(rail_cmd),
                start_tile, TileAddXY(end_tile, 0, !ez),
                TileAddXY(start_tile, 1, 0), TileAddXY(end_tile, !ez, 1), remove_mode);
            break;
        }
        case TRACKDIR_LOWER_E: {
            return DoAutodirTerraform(true, start_tile, end_tile, track, std::move(rail_cmd),
                TileAddXY(start_tile, 1, 1), TileAddXY(end_tile, ez, 1),
                TileAddXY(start_tile, 1, 0), TileAddXY(end_tile, 0, ez), remove_mode);
            break;
        }
        case TRACKDIR_UPPER_W: {
            return DoAutodirTerraform(true, start_tile, end_tile, track, std::move(rail_cmd),
                start_tile, TileAddXY(end_tile, !ez, 0),
                TileAddXY(start_tile, 0, 1), TileAddXY(end_tile, 1, !ez), remove_mode);
            break;
        }
        case TRACKDIR_LOWER_W: {
            return DoAutodirTerraform(true, start_tile, end_tile, track, std::move(rail_cmd),
                TileAddXY(start_tile, 1, 1), TileAddXY(end_tile, 1, ez),
                TileAddXY(start_tile, 0, 1), TileAddXY(end_tile, ez, 0), remove_mode);
            break;
        }
        default:
            break;
    }
    return true;
}

void HandleAutodirPlacement(RailType railtype, bool remove_mode) {
    Track trackstat = static_cast<Track>( _thd.drawstyle & HT_DIR_MASK); // 0..5
    TileIndex start_tile = TileVirtXY(_thd.selstart.x, _thd.selstart.y);
    TileIndex end_tile = TileVirtXY(_thd.selend.x, _thd.selend.y);

    auto cmd = (_thd.drawstyle & HT_RAIL)
        ? GenericPlaceRail(end_tile, railtype, trackstat, remove_mode)
        : DoRailroadTrack(start_tile, end_tile, railtype, trackstat, remove_mode);

    /* When overbuilding existing tracks in polyline mode we want to move the
     * snap point over the last overbuilt track piece. In such case we don't
     * wan't to show any errors to the user. Don't execute the command right
     * away, first check if overbuilding. */
    if (citymania::_estimate_mod || !(_thd.place_mode & HT_POLY) || remove_mode) {
        if (!cmd->post(CcPlaySound_CONSTRUCTION_RAIL)) return;
    } else if (_thd.cm_poly_terra) {
        Track trackstat = static_cast<Track>( _thd.drawstyle & HT_DIR_MASK); // 0..5
        citymania::HandleAutodirTerraform(start_tile, end_tile, trackstat, std::move(cmd), remove_mode);
        return;
    } else if (cmd->call({DoCommandFlag::Auto, DoCommandFlag::NoWater}).GetErrorMessage() != STR_ERROR_ALREADY_BUILT ||
                _cm_rail_track_endtile == INVALID_TILE) {
        if (!cmd->post(CcPlaySound_CONSTRUCTION_RAIL)) return;
    }
    /* Save new snap points for the polyline tool, no matter if the command
     * succeeded, the snapping will be extended over overbuilt track pieces. */
    if (!citymania::_estimate_mod && _cm_rail_track_endtile != INVALID_TILE) {
        StoreRailPlacementEndpoints(start_tile, _cm_rail_track_endtile, trackstat, true);
    }
}

}  // namaespace citymania

