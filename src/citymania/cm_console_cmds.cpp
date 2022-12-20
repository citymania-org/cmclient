#include "../stdafx.h"

#include "cm_console_cmds.hpp"

#include "cm_commands.hpp"
#include "cm_export.hpp"

#include "../command_func.h"
#include "../console_func.h"
#include "../console_type.h"
#include "../date_func.h"
#include "../fileio_type.h"
#include "../map_type.h"
#include "../map_func.h"
#include "../network/network_server.h"
#include "../strings_func.h"
#include "../town.h"
#include "../tree_map.h"

#include <fstream>
#include <sstream>
#include <queue>

#include "../safeguards.h"

bool ReadHeightMap(DetailedFileType dft, const char *filename, uint *x, uint *y, byte **map);

namespace citymania {

uint32 _replay_save_interval = 0;
uint32 _replay_last_save = 0;
uint32 _replay_ticks = 0;
bool _replay_started = false;

static void IConsoleHelp(const char *str)
{
    IConsolePrint(CC_WARNING, "- {}", str);
}

bool ConGameSpeed(byte argc, char *argv[]) {
    if (argc == 0 || argc > 2) {
        IConsoleHelp("Changes game speed. Usage: 'cmgamespeed [n]'");
        return true;
    }
    _game_speed = (argc > 1 ? atoi(argv[1]) : 100);

    return true;
}

bool ConStep(byte argc, char *argv[]) {
    if (argc == 0 || argc > 2) {
        IConsoleHelp("Advances the game for a certain amount of ticks (default 1). Usage: 'cmstep [n]'");
        return true;
    }
    auto n = (argc > 1 ? atoi(argv[1]) : 1);

    // FIXME (n << 1)
    cmd::Pause(PM_PAUSED_NORMAL, 0).post();

    return true;
}

bool ConExport(byte argc, char *argv[]) {
    if (argc == 0) {
        IConsoleHelp("Exports various game data in json format to openttd.json file");
        return true;
    }

    auto filename = "openttd.json";
    citymania::ExportOpenttdData(filename);
    IConsolePrint(CC_DEFAULT, "Data successfully saved to {}", filename);
    return true;
}

bool ConTreeMap(byte argc, char *argv[]) {
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
    byte *map = nullptr;

    if (!ReadHeightMap(dft, filename.c_str(), &x, &y, &map)) {
        free(map);
        return true;
    }

    for (TileIndex tile = 0; tile < MapSize(); tile++) {
        auto mx = x - x * TileX(tile) / MapSizeX() - 1;
        auto my = y * TileY(tile) / MapSizeY();
        auto t = map[mx + my * x];
        auto tree_count = std::min(t / 51, 4);
        // auto tree_growth = (uint)(t % 51) * 7 / 50;
        for (auto i = 0; i < tree_count; i++) {
            // FIXME
//            DoCommand(tile, TREE_INVALID, tile, DC_EXEC, CMD_PLANT_TREE);
        }
    }

    free(map);
    return true;
}

extern void (*UpdateTownGrowthRate)(Town *t);

bool ConResetTownGrowth(byte argc, char *argv[]) {
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

struct FakeCommand {
    Date date;
    DateFract date_fract;
    uint res;
    uint32 seed;
    uint company_id;
    uint cmd;
    TileIndex tile;
    uint32 p1, p2;
    std::string text;
};

static std::queue<FakeCommand> _fake_commands;

void MakeReplaySave() {
    char *filename = str_fmt("replay_%d.sav", _replay_ticks);
    if (SaveOrLoad(filename, SLO_SAVE, DFT_GAME_FILE, SAVE_DIR) != SL_OK) {
        IConsolePrint(CC_ERROR, "Replay save failed");
    } else {
        IConsolePrint(CC_DEFAULT, "Replay saved to {}", filename);
    }
    _replay_last_save = _replay_ticks;
}

bool DatePredate(Date date1, DateFract date_fract1, Date date2, DateFract date_fract2) {
    return date1 < date2 || (date1 == date2 && date_fract1 < date_fract2);
}

void SkipFakeCommands(Date date, DateFract date_fract) {
    uint commands_skipped = 0;

    while (!_fake_commands.empty() && DatePredate(_fake_commands.front().date, _fake_commands.front().date_fract, date, date_fract)) {
        _fake_commands.pop();
        commands_skipped++;
    }

    if (commands_skipped) {
        fprintf(stderr, "Skipped %u commands that predate the current date (%d/%hu)\n", commands_skipped, date, date_fract);
    }
}

void ExecuteFakeCommands(Date date, DateFract date_fract) {
    if (!_replay_started) {
        SkipFakeCommands(_date, _date_fract);
        _replay_started = true;
    }

    auto backup_company = _current_company;
    while (!_fake_commands.empty() && !DatePredate(date, date_fract, _fake_commands.front().date, _fake_commands.front().date_fract)) {
        auto &x = _fake_commands.front();

        // FIXME
        // fprintf(stderr, "Executing command: company=%u cmd=%u(%s) tile=%u p1=%u p2=%u text=%s ... ", x.company_id, x.cmd, GetCommandName(x.cmd), x.tile, x.p1, x.p2, x.text.c_str());
        if (x.res == 0) {
            fprintf(stderr, "REJECTED\n");
            _fake_commands.pop();
            continue;
        }

        if (_networking) {
            /* FIXME
            CommandPacket cp;
            cp.tile = x.tile;
            cp.p1 = x.p1;
            cp.p2 = x.p2;
            cp.cmd = x.cmd;
            cp.text = x.text;
            cp.company = (CompanyID)x.company_id;
            cp.frame = _frame_counter_max + 1;
            cp.callback = nullptr;
            cp.my_cmd = false;

            for (NetworkClientSocket *cs : NetworkClientSocket::Iterate()) {
                if (cs->status >= NetworkClientSocket::STATUS_MAP) {
                    cs->outgoing_queue.Append(&cp);
                }
            }*/
        }
        _current_company = (CompanyID)x.company_id;
        /* FIXME
        auto res = DoCommandPInternal(x.tile, x.p1, x.p2, x.cmd | CMD_NETWORK_COMMAND, nullptr, x.text.c_str(), false, false);
        if (res.Failed() != (x.res != 1)) {
            if (!res.Failed()) {
                fprintf(stderr, "FAIL (Failing command succeeded)\n");
            } else if (res.GetErrorMessage() != INVALID_STRING_ID) {
                char buf[DRAW_STRING_BUFFER];
                GetString(buf, res.GetErrorMessage(), lastof(buf));
                fprintf(stderr, "FAIL (Successful command failed: %s)\n", buf);
            } else {
                fprintf(stderr, "FAIL (Successful command failed)\n");
            }
        } else {
            fprintf(stderr, "OK\n");
        }
        if (x.seed != (_random.state[0] & 255)) {
            fprintf(stderr, "*** DESYNC expected seed %u vs current %u ***\n", x.seed, _random.state[0] & 255);
        }*/
        _fake_commands.pop();
    }
    _current_company = backup_company;
}

void CheckIntervalSave() {
    if (_pause_mode == PM_UNPAUSED) {
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

void LoadCommands(const std::string &filename) {
    std::queue<FakeCommand>().swap(_fake_commands);  // clear queue

    std::ifstream file(filename, std::ios::in);
    std::string str;
    while(std::getline(file, str)) {
        std::istringstream ss(str);
        FakeCommand cmd;
        // FIXME ss >> cmd.date >> cmd.date_fract >> cmd.res >> cmd.seed >> cmd.company_id >> cmd.cmd  >> cmd.tile >> cmd.p1 >> cmd.p2;
        std::string s;
        ss.get();
        std::getline(ss, cmd.text);
        _fake_commands.push(cmd);
    }

    _replay_started = false;
}

bool IsReplayingCommands() {
    return !_fake_commands.empty();
}


bool ConLoadCommands(byte argc, char *argv[]) {
    if (argc == 0) {
        IConsoleHelp("Loads a file with command queue to execute");
        IConsoleHelp("Usage: 'cmloadcommands <file>'");
        return true;
    }

    if (argc > 3) return false;

    LoadCommands(argv[1]);
    SetReplaySaveInterval(argc > 2 ? atoi(argv[2]) : 0);

    return true;
}

bool ConStartRecord(byte argc, char *argv[]) {
    StartRecording();
    return true;
}

bool ConStopRecord(byte argc, char *argv[]) {
    StopRecording();
    return true;
}

} // namespace citymania
