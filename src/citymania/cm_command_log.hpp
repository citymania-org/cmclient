#ifndef CM_COMMAND_LOG_HPP
#define CM_COMMAND_LOG_HPP

#include <functional>

namespace citymania {

void load_replay_commands(const std::string &filename, std::function<void(const std::string &)>);

}; // namespace citymania

#endif
