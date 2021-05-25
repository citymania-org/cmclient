#ifndef CM_CONSOLE_CMDS_HPP
#define CM_CONSOLE_CMDS_HPP

#include "../date_type.h"

namespace citymania {

bool ConStep(byte argc, char *argv[]);
bool ConExport(byte argc, char *argv[]);
bool ConTreeMap(byte argc, char *argv[]);
bool ConResetTownGrowth(byte argc, char *argv[]);
bool ConLoadCommands(byte argc, char *argv[]);
void ExecuteFakeCommands(Date date, DateFract date_fract);

} // namespace citymania

#endif


