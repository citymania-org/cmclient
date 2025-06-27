#include "../stdafx.h"

#include "cm_console_cmds.hpp"

#include "cm_commands.hpp"
#include "cm_command_log.hpp"
#include "cm_export.hpp"

#include "../aircraft.h"
#include "../command_func.h"
#include "../console_func.h"
#include "../console_type.h"
#include "../fileio_type.h"
#include "../map_type.h"
#include "../map_func.h"
#include "../roadveh.h"
#include "../strings_func.h"
#include "../ship.h"
#include "../town.h"
#include "../train.h"
#include "../tree_map.h"

#include <fstream>
#include <sstream>
#include <queue>

#include "../safeguards.h"

bool ReadHeightMap(DetailedFileType dft, const char *filename, uint *x, uint *y, std::vector<uint8_t> *map);

namespace citymania {

uint32 _replay_save_interval = 0;
uint32 _replay_last_save = 0;
uint32 _replay_ticks = 0;
extern uint32 _pause_countdown;

static void IConsoleHelp(const char *str)
{
    IConsolePrint(CC_WARNING, "- {}", str);
}

bool ConGameSpeed([[maybe_unused]] uint8_t argc, [[maybe_unused]] char *argv[]) {
    if (argc == 0 || argc > 2) {
        IConsoleHelp("Changes game speed. Usage: 'cmgamespeed [n]'");
        return true;
    }
    _game_speed = (argc > 1 ? atoi(argv[1]) : 100);

    return true;
}

bool ConStep([[maybe_unused]] uint8_t argc, [[maybe_unused]] char *argv[]) {
    if (argc == 0 || argc > 2) {
        IConsoleHelp("Advances the game for a certain amount of ticks (default 1). Usage: 'cmstep [n]'");
        return true;
    }
    auto n = (argc > 1 ? atoi(argv[1]) : 1);

    _pause_countdown = n;
    cmd::Pause(PauseMode::Normal, 0).post();

    return true;
}

bool ConExport([[maybe_unused]] uint8_t argc, [[maybe_unused]] char *argv[]) {
    if (argc == 0) {
        IConsoleHelp("Exports various game data in json format to openttd.json file");
        return true;
    }

    auto filename = "openttd.json";
    citymania::ExportOpenttdData(filename);
    IConsolePrint(CC_DEFAULT, "Data successfully saved to {}", filename);
    return true;
}

bool ConTreeMap([[maybe_unused]] uint8_t argc, [[maybe_unused]] char *argv[]) {
    if (argc == 0) {
        IConsoleHelp("Loads heighmap-like file and plants trees according to it, values 0-256 ore scaled to 0-4 trees.");
        IConsoleHelp("Usage: 'cmtreemap <file>'");
        IConsoleHelp("Default lookup path is in scenario/heightmap in your openttd directory");
        return true;
    }

    if (argc != 2) return false;

    std::string filename = argv[1];

    if (_game_mode != GM_EDITOR) {
        IConsolePrint(CC_ERROR, "This command is only available in scenario editor.");
        return true;
    }


    if (filename.size() < 4) {
        IConsolePrint(CC_ERROR, "Unknown treemap extension should be .bmp or .png.");
        return true;
    }

    auto ext = filename.substr(filename.length() - 4, 4);
    DetailedFileType dft;
    if (ext == ".bmp") dft = DFT_HEIGHTMAP_BMP;
#ifdef WITH_PNG
    else if (ext == ".png") dft = DFT_HEIGHTMAP_PNG;
#endif
    else {
#ifdef WITH_PNG
        IConsolePrint(CC_ERROR, "Unknown treemap extension {}, should be .bmp or .png.", ext.c_str());
#else
        IConsolePrint(CC_ERROR, "Unknown treemap extension {}, should be .bmp (game was compiled without PNG support).", ext.c_str());
#endif
        return true;
    }

    uint x, y;
    std::vector<uint8_t> map;

    if (!ReadHeightMap(dft, filename.c_str(), &x, &y, &map)) {
        return true;
    }

    for (auto tile : Map::Iterate()) {
        auto mx = x - x * TileX(tile) / Map::SizeX() - 1;
        auto my = y * TileY(tile) / Map::SizeY();
        auto t = map[mx + my * x];
        auto tree_count = std::min(t / 51, 4);
        // auto tree_growth = (uint)(t % 51) * 7 / 50;
        for (auto i = 0; i < tree_count; i++) {
            ::Command<CMD_PLANT_TREE>::Post(tile, tile, TREE_INVALID, false);
        }
    }

    return true;
}

extern void (*UpdateTownGrowthRate)(Town *t);

bool ConResetTownGrowth([[maybe_unused]] uint8_t argc, [[maybe_unused]] char *argv[]) {
    if (argc == 0) {
        IConsoleHelp("Resets growth to normal for all towns.");
        IConsoleHelp("Usage: 'cmresettowngrowth'");
        return true;
    }

    if (argc > 1) return false;

    for (Town *town : Town::Iterate()) {
        ClrBit(town->flags, TOWN_CUSTOM_GROWTH);
        UpdateTownGrowthRate(town);
    }
    return true;
}

void MakeReplaySave() {
    auto filename = fmt::format("replay_{}.sav", _replay_ticks);
    if (SaveOrLoad(filename, SLO_SAVE, DFT_GAME_FILE, SAVE_DIR) != SL_OK) {
        IConsolePrint(CC_ERROR, "Replay save failed");
    } else {
        IConsolePrint(CC_DEFAULT, "Replay saved to {}", filename);
    }
    _replay_last_save = _replay_ticks;
}

void CheckIntervalSave() {
    if (_pause_mode.None()) {
        _replay_ticks++;
        if (_replay_save_interval && _replay_ticks - _replay_last_save  >= _replay_save_interval) {
            MakeReplaySave();
        }
    }
}

void SetReplaySaveInterval(uint32 interval) {
    _replay_save_interval = interval;
    _replay_last_save = 0;
    _replay_ticks = 0;

    if (_replay_save_interval) MakeReplaySave();
}

bool ConLoadCommands([[maybe_unused]] uint8_t argc, [[maybe_unused]] char *argv[]) {
    if (argc == 0) {
        IConsoleHelp("Loads a file with command queue to execute");
        IConsoleHelp("Usage: 'cmloadcommands <file>'");
        return true;
    }

    if (argc > 3) return false;

    load_replay_commands(argv[1], [](auto error) {
        IConsolePrint(CC_ERROR, "{}", error);
    });
    SetReplaySaveInterval(argc > 2 ? atoi(argv[2]) : 0);

    return true;
}

bool ConStartRecord([[maybe_unused]] uint8_t argc, [[maybe_unused]] char *argv[]) {
    StartRecording();
    return true;
}

bool ConStopRecord([[maybe_unused]] uint8_t argc, [[maybe_unused]] char *argv[]) {
    StopRecording();
    return true;
}

bool ConGameStats([[maybe_unused]] uint8_t argc, [[maybe_unused]] char *argv[]) {
    auto num_trains = 0u;
    auto num_rvs = 0u;
    auto num_ships = 0u;
    auto num_aircraft = 0u;

    for (Vehicle *v : Vehicle::Iterate()) {
        if (!v->IsPrimaryVehicle()) continue;
        switch (v->type) {
            default: break;
            case VEH_TRAIN: num_trains++; break;
            case VEH_ROAD: num_rvs++; break;
            case VEH_AIRCRAFT: num_aircraft++; break;
            case VEH_SHIP: num_ships++; break;
        }
    }

    IConsolePrint(CC_INFO, "Number of trains: {}", num_trains);
    IConsolePrint(CC_INFO, "Number of rvs: {}", num_rvs);
    IConsolePrint(CC_INFO, "Number of ships: {}", num_ships);
    IConsolePrint(CC_INFO, "Number of aircraft: {}", num_aircraft);
    IConsolePrint(CC_INFO, "Total number of vehicles: {}", num_trains + num_rvs + num_ships + num_aircraft);

    return true;
}

} // namespace citymania
