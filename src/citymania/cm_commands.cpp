#include "../stdafx.h"

#include "cm_commands.hpp"

#include "../command_func.h"
#include "../network/network.h"
#include "../network/network_client.h"

#include <queue>

namespace citymania {

const uint32 MAX_CALLBACK_LIFETIME = 30;  // it should be executed within few frames so 30 more than enough

std::map<size_t, std::pair<uint32, std::vector<CommandCallback>>> _command_callbacks;
std::queue<std::pair<size_t, uint32>> _command_sent;

uint GetCurrentQueueDelay();

template <typename T, typename... Rest>
void hash_combine(std::size_t& seed, const T& v, const Rest&... rest)
{
    seed ^= std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    (hash_combine(seed, rest), ...);
}

size_t GetCommandHash(TileIndex tile, uint32 p1, uint32 p2, uint32 cmd, const std::string &text) {
    size_t res = 0;
    hash_combine(res, tile, p1, p2, cmd, text);
    return res;
}
/*
void AddCommandCallback(TileIndex tile, uint32 p1, uint32 p2, uint32 cmd, const std::string &text, CommandCallback callback) {
    if (!_networking) {
        callback(true);
        return;
    }
    auto hash = GetCommandHash(tile, p1, p2, cmd & CMD_ID_MASK, text);
    auto sent_frame = _frame_counter + GetCurrentQueueDelay();
    _command_callbacks[hash].first = sent_frame;
    _command_callbacks[hash].second.push_back(callback);
    _command_sent.push(std::make_pair(hash, sent_frame));
    // fprintf(stderr, "CALLBACK %lu (%u %u %u %u %s)\n", hash, tile, p1, p2, (uint)(cmd & CMD_ID_MASK), text.c_str());
}

bool DoCommandWithCallback(TileIndex tile, uint32 p1, uint32 p2, uint32 cmd, ::CommandCallback *callback, const std::string &text, CommandCallback cm_callback) {
    if (_networking) {
        AddCommandCallback(tile, p1, p2, cmd, text, cm_callback);
        return DoCommandP(tile, p1, p2, cmd, callback, text);
    }
    auto res = DoCommandP(tile, p1, p2, cmd, callback, text);
    cm_callback(res);
    return res;
}

bool DoCommandWithCallback(const CommandContainer &cc, CommandCallback callback) {
    return DoCommandWithCallback(cc.tile, cc.p1, cc.p2, cc.cmd, cc.callback, cc.text, callback);
}
*/
void HandleCommandExecution(bool res, TileIndex tile, uint32 p1, uint32 p2, uint32 cmd, const std::string &text) {
    auto hash = GetCommandHash(tile, p1, p2, cmd & CMD_ID_MASK, text);
    auto p = _command_callbacks.find(hash);
    // fprintf(stderr, "EXECUTED %lu (%u %u %u %u %s) %u\n", hash, tile, p1, p2, (uint)(cmd & CMD_ID_MASK), text.c_str(), (int)(p == _command_callbacks.end()));
    if (p == _command_callbacks.end()) return;
    for (auto &cb : p->second.second)
        cb(res);
    _command_callbacks.erase(p);
}

void ClearOldCallbacks() {
    while(!_command_sent.empty() && _command_sent.front().second + MAX_CALLBACK_LIFETIME < _frame_counter) {
        auto hash = _command_sent.front().first;
        _command_sent.pop();
        auto p = _command_callbacks.find(hash);
        if (p != _command_callbacks.end() && p->first + MAX_CALLBACK_LIFETIME < _frame_counter) {
            _command_callbacks.erase(p);
        }
    }
}

std::queue<CommandPacket> _outgoing_queue;
uint _commands_this_frame;

void InitCommandQueue() {
    _commands_this_frame = 0;
    std::queue<CommandPacket>().swap(_outgoing_queue);  // clear queue
    _command_callbacks.clear();
}

bool CanSendCommand() {
    return _commands_this_frame < 2;
}

uint GetCurrentQueueDelay() {
    return _outgoing_queue.size() / 2;
}

void FlushCommandQueue() {
    while (!_outgoing_queue.empty() && CanSendCommand()) {
        MyClient::SendCommand(&_outgoing_queue.front());
        _outgoing_queue.pop();
        _commands_this_frame++;
    }
}

void HandleNextClientFrame() {
    _commands_this_frame = 0;
    FlushCommandQueue();
    ClearOldCallbacks();
}

/*void SendClientCommand(const CommandPacket *cp) {
    if (_outgoing_queue.empty() && CanSendCommand()) {
        MyClient::SendCommand(cp);
        _commands_this_frame++;
        return;
    }
    _outgoing_queue.push(*cp);
}*/


namespace cmd {

bool Pause::Post(bool automati) {
    return ::Command<CMD_PAUSE>::Post(this->mode, this->pause);
}

bool DoTownAction::Post(bool automatic) {
    return ::Command<CMD_DO_TOWN_ACTION>::Post(this->town_id, this->town_action);
}

bool SkipToOrder::Post(bool automatic) {
    return ::Command<CMD_SKIP_TO_ORDER>::Post(
        this->veh_id, this->sel_ord
        _ctrl_pressed ? STR_ERROR_CAN_T_SKIP_TO_ORDER : STR_ERROR_CAN_T_SKIP_ORDER,
                this->vehicle->tile, this->vehicle->index, citymania::_fn_mod ? this->OrderGetSel() : ((this->vehicle->cur_implicit_order_index + 1) % this->vehicle->GetNumOrders()));
}

}  // namespace cmd

}  // namespace citymania
