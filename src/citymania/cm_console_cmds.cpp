#include "../stdafx.h"

#include "cm_console_cmds.hpp"

#include "cm_export.hpp"

#include "../command_func.h"
#include "../console_func.h"
#include "../console_type.h"
#include "../fileio_type.h"
#include "../map_type.h"
#include "../map_func.h"
#include "../strings_func.h"
#include "../town.h"
#include "../tree_map.h"

#include <fstream>
#include <sstream>
#include <queue>

#include "../safeguards.h"

bool ReadHeightMap(DetailedFileType dft, const char *filename, uint *x, uint *y, byte **map);

namespace citymania {

static void IConsoleHelp(const char *str)
{
    IConsolePrintF(CC_WARNING, "- %s", str);
}

bool ConStep(byte argc, char *argv[]) {
    if (argc == 0 || argc > 2) {
        IConsoleHelp("Advances the game for a certain amount of ticks (default 1). Usage: 'cmstep [n]'");
        return true;
    }
    auto n = (argc > 1 ? atoi(argv[1]) : 1);

    DoCommandP(0, PM_PAUSED_NORMAL, 0 | (n << 1), CMD_PAUSE);

    return true;
}

bool ConExport(byte argc, char *argv[]) {
    if (argc == 0) {
        IConsoleHelp("Exports various game data in json format to openttd.json file");
        return true;
    }

    auto filename = "openttd.json";
    citymania::ExportOpenttdData(filename);
    IConsolePrintF(CC_DEFAULT, "Data successfully saved to %s", filename);
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
        IConsolePrintF(CC_ERROR, "This command is only available in scenario editor.");
        return true;
    }


    if (filename.size() < 4) {
        IConsolePrintF(CC_ERROR, "Unknown treemap extension should be .bmp or .png.");
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
        IConsolePrintF(CC_ERROR, "Unknown treemap extension %s, should be .bmp or .png.", ext.c_str());
#else
        IConsolePrintF(CC_ERROR, "Unknown treemap extension %s, should be .bmp (game was compiled without PNG support).", ext.c_str());
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
            DoCommand(tile, TREE_INVALID, tile, DC_EXEC, CMD_PLANT_TREE);
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
    uint company_id;
    uint cmd;
    TileIndex tile;
    uint32 p1, p2;
    std::string text;
};

static std::queue<FakeCommand> _fake_commands;

void ExecuteFakeCommands(Date date, DateFract date_fract) {
    auto backup_company = _current_company;
    while (!_fake_commands.empty() && _fake_commands.front().date <= date && _fake_commands.front().date_fract <= date_fract) {
        auto &x = _fake_commands.front();
        if (x.date < date || x.date_fract < date_fract) IConsolePrintF(CC_WARNING,
                                                                       "Queued command is earlier than execution date: %d/%hu vs %d/%hu",
                                                                       x.date, x.date_fract, date, date_fract);
        fprintf(stderr, "Executing command: company=%u cmd=%u tile=%u p1=%u p2=%u text=%s ... ", x.company_id, x.cmd, x.tile, x.p1, x.p2, x.text.c_str());
        _current_company = (CompanyID)x.company_id;
        auto res = DoCommandPInternal(x.tile, x.p1, x.p2, x.cmd | CMD_NETWORK_COMMAND, nullptr, x.text.c_str(), false, false);
        if (res.Failed()) {
            if (res.GetErrorMessage() != INVALID_STRING_ID) {
                char buf[DRAW_STRING_BUFFER];
                GetString(buf, res.GetErrorMessage(), lastof(buf));
                fprintf(stderr, "%s\n", buf);
            } else {
                fprintf(stderr, "FAIL\n");
            }
        } else {
            fprintf(stderr, "OK\n");
        }
        _fake_commands.pop();
    }
    _current_company = backup_company;
}

bool ConLoadCommands(byte argc, char *argv[]) {
    if (argc == 0) {
        IConsoleHelp("Loads a file with command queue to execute");
        IConsoleHelp("Usage: 'cmloadcommands <file>'");
        return true;
    }

    if (argc != 2) return false;

    std::queue<FakeCommand>().swap(_fake_commands);  // clear queue

    std::ifstream file(argv[1], std::ios::in);
    std::string str;
    while(std::getline(file, str))
    {
        std::istringstream ss(str);
        FakeCommand cmd;
        ss >> cmd.date >> cmd.date_fract >> cmd.company_id >> cmd.cmd >> cmd.p1 >> cmd.p2 >> cmd.tile;
        std::string s;
        ss.get();
        std::getline(ss, cmd.text);
        // fprintf(stderr, "PARSED: company=%u cmd=%u tile=%u p1=%u p2=%u text=%s\n", cmd.company_id, cmd.cmd, cmd.tile, cmd.p1, cmd.p2, cmd.text.c_str());
        _fake_commands.push(cmd);
    }
    return true;
}

} // namespace citymania
