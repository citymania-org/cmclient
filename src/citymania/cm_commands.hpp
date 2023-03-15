#ifndef CM_COMMANDS_HPP
#define CM_COMMANDS_HPP

#include "../command_type.h"
#include "../tile_type.h"

#include "generated/cm_gen_commands.hpp"

struct CommandPacket;

namespace citymania {

// void HandleCommandExecution(bool res, TileIndex tile, uint32 p1, uint32 p2, uint32 cmd, const std::string &text);
void AddCommandCallback(const CommandPacket *cp);
// void ExecuteCurrentCallback(const CommandCost &cost);
void BeforeNetworkCommandExecution(const CommandPacket* cp);
void AfterNetworkCommandExecution(const CommandPacket* cp);

void InitCommandQueue();
void HandleNextClientFrame();
void SendClientCommand(const CommandPacket *cp);
int get_average_command_lag();

}  // namespace citymania

#endif
