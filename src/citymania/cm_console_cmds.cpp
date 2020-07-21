#include "../stdafx.h"

#include "cm_console_cmds.hpp"

#include "cm_export.hpp"

#include "../command_func.h"
#include "../console_func.h"
#include "../console_type.h"
#include "../fileio_type.h"
#include "../map_type.h"
#include "../map_func.h"
#include "../tree_map.h"

#include "../safeguards.h"

bool ReadHeightMap(DetailedFileType dft, const char *filename, uint *x, uint *y, byte **map);

namespace citymania {

static void IConsoleHelp(const char *str)
{
    IConsolePrintF(CC_WARNING, "- %s", str);
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
        auto mx = x * TileX(tile) / MapSizeX();
        auto my = y * TileY(tile) / MapSizeY();
        auto t = map[mx + my * x];
        auto tree_count = min(t / 51, 4);
        // auto tree_growth = (uint)(t % 51) * 7 / 50;
        for (auto i = 0; i < tree_count; i++) {
            DoCommand(tile, TREE_INVALID, tile, DC_EXEC, CMD_PLANT_TREE);
        }
    }

    free(map);
    return true;
}

} // namespace citymania
