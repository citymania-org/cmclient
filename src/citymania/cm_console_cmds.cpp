#include "../stdafx.h"

#include "cm_console_cmds.hpp"

#include "cm_export.hpp"

#include "../console_func.h"
#include "../console_type.h"

#include "../safeguards.h"

namespace citymania {

static void IConsoleHelp(const char *str)
{
    IConsolePrint(CC_WARNING, "- {}", str);
}

bool ConExport(uint8_t argc, [[maybe_unused]] char *argv[]) {
    if (argc == 0) {
        IConsoleHelp("Exports various game data in json format to openttd.json file");
        return true;
    }

    auto filename = "openttd.json";
    citymania::ExportOpenttdData(filename);
    IConsolePrint(CC_DEFAULT, "Data successfully saved to {}", filename);
    return true;
}

} // namespace citymania
