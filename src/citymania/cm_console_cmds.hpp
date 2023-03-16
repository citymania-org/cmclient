#ifndef CM_CONSOLE_CMDS_HPP
#define CM_CONSOLE_CMDS_HPP

#include "../date_type.h"

namespace citymania {

void SkipFakeCommands(Date date, DateFract date_fract);
void SetReplaySaveInterval(uint32 interval);
void CheckIntervalSave();
bool IsReplayingCommands();

bool ConGameSpeed(byte argc, char *argv[]);
bool ConStep(byte argc, char *argv[]);
bool ConExport(byte argc, char *argv[]);
bool ConTreeMap(byte argc, char *argv[]);
bool ConResetTownGrowth(byte argc, char *argv[]);
bool ConLoadCommands(byte argc, char *argv[]);
void ExecuteFakeCommands(Date date, DateFract date_fract);

bool ConStartRecord(byte argc, char *argv[]);
bool ConStopRecord(byte argc, char *argv[]);
bool ConGameStats(byte argc, char *argv[]);

} // namespace citymania

#endif


