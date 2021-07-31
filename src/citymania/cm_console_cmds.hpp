#ifndef CM_CONSOLE_CMDS_HPP
#define CM_CONSOLE_CMDS_HPP

#include "../date_type.h"

namespace citymania {

void SkipFakeCommands(Date date, DateFract date_fract);
void SetReplaySaveInterval(uint32 interval);
void LoadCommands(const std::string &filename);
void CheckIntervalSave();

bool ConGameSpeed(byte argc, char *argv[]);
bool ConStep(byte argc, char *argv[]);
bool ConExport(byte argc, char *argv[]);
bool ConTreeMap(byte argc, char *argv[]);
bool ConResetTownGrowth(byte argc, char *argv[]);
bool ConLoadCommands(byte argc, char *argv[]);
void ExecuteFakeCommands(Date date, DateFract date_fract);

} // namespace citymania

#endif


