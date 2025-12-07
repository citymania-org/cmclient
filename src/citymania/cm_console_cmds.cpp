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
#include "../core/string_consumer.hpp"

#include <fstream>
#include <sstream>
#include <queue>

#include "../safeguards.h"

bool ReadHeightMap(DetailedFileType dft, std::string_view filename, uint *x, uint *y, std::vector<uint8_t> *map);
extern uint32 _gfx_debug_flags;

namespace citymania {

uint32 _replay_save_interval = 0;
uint32 _replay_last_save = 0;
uint32 _replay_ticks = 0;
extern uint32 _pause_countdown;

static void IConsoleHelp(std::string_view str) {
    IConsolePrint(CC_WARNING, "- {}", str);
}

template <typename ... Args>
void IConsoleError(fmt::format_string<Args...> format, Args&&... args) {
    IConsolePrint(CC_ERROR, fmt::format(format, std::forward<Args>(args)...));
}

bool ConGameSpeed(std::span<std::string_view> argv) {
    if (argv.empty() == 0 || argv.size() > 2) {
        IConsoleHelp("Changes game speed. Usage: 'cmgamespeed [n]'");
        return true;
    }
    uint new_speed = 100;
    if (argv.size() > 1) {
        auto t = ParseInteger<uint>(argv[1]);
        if (!t.has_value()) {
            IConsoleError("Invalid number '{}'", argv[1]);
            return true;
        }
        new_speed = t.value();
    }
    _game_speed = new_speed;

    return true;
}

bool ConStep(std::span<std::string_view> argv) {
    if (argv.empty()) {
        IConsoleHelp("Advances the game for a certain number of ticks (default 1). Usage: 'cmstep [n]'");
        return true;
    }
    if (argv.size() > 2) return false;

    uint n = 1;
    if (argv.size() > 1) {
        auto t = ParseInteger<uint>(argv[1]);
        if (!t.has_value()) {
            IConsoleError("Invalid number '{}'", argv[1]);
            return true;
        }
        n = t.value();
    }

    _pause_countdown = n;
    cmd::Pause(PauseMode::Normal, 0).post();

    return true;
}

bool ConExport(std::span<std::string_view> argv) {
    if (argv.empty()) {
        IConsoleHelp("Exports various game data in json format to openttd.json file");
        return true;
    }

    auto filename = "openttd.json";
    citymania::ExportOpenttdData(filename);
    IConsolePrint(CC_DEFAULT, "Data successfully saved to {}", filename);
    return true;
}

bool ConTreeMap(std::span<std::string_view> argv) {
    if (argv.empty()) {
        IConsoleHelp("Loads heighmap-like file and plants trees according to it, values 0-256 ore scaled to 0-4 trees.");
        IConsoleHelp("Usage: 'cmtreemap <file>'");
        IConsoleHelp("Default lookup path is in scenario/heightmap in your openttd directory");
        return true;
    }

    if (argv.size() != 2) return false;

    std::string_view filename = std::string_view(argv[1]);

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
        IConsolePrint(CC_ERROR, "Unknown treemap extension {}, should be .bmp or .png.", ext);
#else
        IConsolePrint(CC_ERROR, "Unknown treemap extension {}, should be .bmp (game was compiled without PNG support).", ext);
#endif
        return true;
    }

    uint x, y;
    std::vector<uint8_t> map;

    if (!ReadHeightMap(dft, filename, &x, &y, &map)) {
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

bool ConResetTownGrowth(std::span<std::string_view> argv) {
    if (argv.empty()) {
        IConsoleHelp("Resets growth to normal for all towns.");
        IConsoleHelp("Usage: 'cmresettowngrowth'");
        return true;
    }

    if (argv.size() > 1) return false;

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

bool ConLoadCommands(std::span<std::string_view> argv) {
    if (argv.empty()) {
        IConsoleHelp("Loads a file with command queue to execute");
        IConsoleHelp("Usage: 'cmloadcommands <file>'");
        return true;
    }

    if (argv.size() > 3) return false;

    load_replay_commands(argv[1], [](auto error) {
        IConsolePrint(CC_ERROR, "{}", error);
    });
    SetReplaySaveInterval(argv.size() > 2 ? ParseInteger(argv[2]).value_or(0) : 0);

    return true;
}

bool ConStartRecord(std::span<std::string_view> /* argv */) {
    StartRecording();
    return true;
}

bool ConStopRecord(std::span<std::string_view> /* argv */) {
    StopRecording();
    return true;
}

bool ConGameStats(std::span<std::string_view> argv) {
    if (argv.empty()) {
        IConsoleHelp("Prints total number of vehicles");
        IConsoleHelp("Usage: 'cmgamestats'");
        return true;
    }

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

// From jgrpp viewports
bool ConGfxDebug(std::span<std::string_view> argv) {
    if (argv.empty()) {
        IConsolePrint(CC_HELP, "Debug: gfx flags.  Usage: 'gfx_debug [<flags>]'");
        IConsolePrint(CC_HELP, "  1: GDF_SHOW_WINDOW_DIRTY");
        IConsolePrint(CC_HELP, "  2: GDF_SHOW_WIDGET_DIRTY");
        IConsolePrint(CC_HELP, "  4: GDF_SHOW_RECT_DIRTY");
        return true;
    }

    if (argv.size() > 2) return false;

    if (argv.size() == 1) {
        IConsolePrint(CC_DEFAULT, "Gfx debug flags: {:X}", _gfx_debug_flags);
    } else {
        auto t = ParseInteger(argv[1], 16);
        if (t.has_value()) _gfx_debug_flags = t.value();
        else IConsolePrint(CC_ERROR, "Invalid hex: '{}'", argv[1]);
    }

    return true;
}

} // namespace citymania
