#ifndef CM_COMMANDS_HPP
#define CM_COMMANDS_HPP

#include "../command_type.h"
#include "../tile_type.h"
#include "../network/network_internal.h"

namespace citymania {

typedef std::function<bool(bool)> CommandCallback;
void AddCommandCallback(TileIndex tile, uint32 p1, uint32 p2, uint32 cmd, const std::string &text, CommandCallback callback);
bool DoCommandWithCallback(TileIndex tile, uint32 p1, uint32 p2, uint32 cmd, ::CommandCallback *callback, const std::string &text, CommandCallback cm_callback);
bool DoCommandWithCallback(const CommandContainer &cc, CommandCallback callback);
void HandleCommandExecution(bool res, TileIndex tile, uint32 p1, uint32 p2, uint32 cmd, const std::string &text);

void InitCommandQueue();
void HandleNextClientFrame();
void SendClientCommand(const CommandPacket *cp);

}  // namespace citymania

#endif
