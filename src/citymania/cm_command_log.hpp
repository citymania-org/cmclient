#ifndef CM_COMMAND_LOG_HPP
#define CM_COMMAND_LOG_HPP

#include <functional>

namespace citymania {

void load_replay_commands(std::string_view filename, std::function<void(const std::string &)>);

}; // namespace citymania

#endif
