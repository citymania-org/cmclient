#ifndef CM_CONSOLE_CMDS_HPP
#define CM_CONSOLE_CMDS_HPP

#include "../timer/timer_game_tick.h"

namespace citymania {

void SkipFakeCommands(TimerGameTick::TickCounter counter);
void SetReplaySaveInterval(uint32 interval);
void CheckIntervalSave();
bool IsReplayingCommands();

bool ConGameSpeed(std::span<std::string_view> argv);
bool ConStep(std::span<std::string_view> argv);
bool ConExport(std::span<std::string_view> argv);
bool ConTreeMap(std::span<std::string_view> argv);
bool ConResetTownGrowth(std::span<std::string_view> argv);
bool ConLoadCommands(std::span<std::string_view> argv);
void ExecuteFakeCommands(TimerGameTick::TickCounter counter);

bool ConStartRecord(std::span<std::string_view> argv);
bool ConStopRecord(std::span<std::string_view> argv);
bool ConGameStats(std::span<std::string_view> argv);
bool ConGfxDebug(std::span<std::string_view> argv);

} // namespace citymania

#endif


