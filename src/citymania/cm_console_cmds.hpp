#ifndef CM_CONSOLE_CMDS_HPP
#define CM_CONSOLE_CMDS_HPP

#include "../timer/timer_game_tick.h"

namespace citymania {

void SkipFakeCommands(TimerGameTick::TickCounter counter);
void SetReplaySaveInterval(uint32 interval);
void CheckIntervalSave();
bool IsReplayingCommands();

bool ConGameSpeed(uint8_t argc, char *argv[]);
bool ConStep(uint8_t argc, char *argv[]);
bool ConExport(uint8_t argc, char *argv[]);
bool ConTreeMap(uint8_t argc, char *argv[]);
bool ConResetTownGrowth(uint8_t argc, char *argv[]);
bool ConLoadCommands(uint8_t argc, char *argv[]);
void ExecuteFakeCommands(TimerGameTick::TickCounter counter);

bool ConStartRecord(uint8_t argc, char *argv[]);
bool ConStopRecord(uint8_t argc, char *argv[]);
bool ConGameStats(uint8_t argc, char *argv[]);

} // namespace citymania

#endif


