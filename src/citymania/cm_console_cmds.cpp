#include "../stdafx.h"

#include "cm_console_cmds.hpp"

#include "cm_export.hpp"

#include "../console_func.h"
#include "../console_type.h"

#include "../safeguards.h"

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

} // namespace citymania
