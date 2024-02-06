#ifndef CM_CONSOLE_CMDS_HPP
#define CM_CONSOLE_CMDS_HPP

#include "../timer/timer_game_tick.h"

namespace citymania {

void SkipFakeCommands(TimerGameTick::TickCounter counter);
void SetReplaySaveInterval(uint32 interval);
void CheckIntervalSave();
bool IsReplayingCommands();

bool ConGameSpeed(byte argc, char *argv[]);
bool ConStep(byte argc, char *argv[]);
bool ConExport(byte argc, char *argv[]);
bool ConTreeMap(byte argc, char *argv[]);
bool ConResetTownGrowth(byte argc, char *argv[]);
bool ConLoadCommands(byte argc, char *argv[]);
void ExecuteFakeCommands(TimerGameTick::TickCounter counter);

bool ConStartRecord(byte argc, char *argv[]);
bool ConStopRecord(byte argc, char *argv[]);
bool ConGameStats(byte argc, char *argv[]);

} // namespace citymania

#endif


