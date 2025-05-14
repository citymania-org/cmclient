#include "../stdafx.h"

#include "cm_command_log.hpp"

#include "cm_bitstream.hpp"

#include "../command_type.h"
#include "../network/network_internal.h"
#include "../network/network_server.h"
#include "../console_func.h"
#include "../rev.h"
#include "../strings_func.h"

#include <lzma.h>

#include <fstream>
#include <queue>

namespace citymania {


struct FakeCommand {
    TimerGameTick::TickCounter counter;
    uint res;
    uint8 seed;
    uint16 client_id;
    CommandPacket cp;
};

static std::queue<FakeCommand> _fake_commands;
bool _replay_started = false;

void SkipFakeCommands(TimerGameTick::TickCounter counter) {
    uint commands_skipped = 0;

    while (!_fake_commands.empty() && _fake_commands.front().counter < counter) {
        _fake_commands.pop();
        commands_skipped++;
    }

    if (commands_skipped) {
        fprintf(stderr, "Skipped %u commands that predate the current counter (%lu)\n", commands_skipped, counter);
    }
}

extern CommandCost ExecuteCommand(CommandPacket *cp);

void ExecuteFakeCommands(TimerGameTick::TickCounter counter) {
    if (!_replay_started) {
        SkipFakeCommands(counter);
        _replay_started = true;
    }

    auto backup_company = _current_company;
    while (!_fake_commands.empty() && _fake_commands.front().counter <= counter) {
        auto &x = _fake_commands.front();

        fprintf(stderr, "Executing command: %s(%u) company=%u ... ", GetCommandName(x.cp.cmd), x.cp.cmd, x.cp.company);
        if (x.res == 0) {
            fprintf(stderr, "REJECTED\n");
            _fake_commands.pop();
            continue;
        }

        if (_networking) {
            x.cp.frame = _frame_counter_max + 1;
            x.cp.callback = nullptr;
            x.cp.my_cmd = false;

            for (NetworkClientSocket *cs : NetworkClientSocket::Iterate()) {
                if (cs->status >= NetworkClientSocket::STATUS_MAP) {
                    cs->outgoing_queue.push_back(x.cp);
                }
            }
        }

        _current_company = (CompanyID)x.cp.company;
        auto res = ExecuteCommand(&x.cp);
        if (res.Failed() != (x.res != 1)) {
            if (!res.Failed()) {
                fprintf(stderr, "FAIL (Failing command succeeded)\n");
            } else if (res.GetErrorMessage() != INVALID_STRING_ID) {
                auto buf = GetString(res.GetErrorMessage());
                fprintf(stderr, "FAIL (Successful command failed: %s)\n", buf.c_str());
            } else {
                fprintf(stderr, "FAIL (Successful command failed)\n");
            }
        } else {
            fprintf(stderr, "OK\n");
        }
        if (x.seed != (_random.state[0] & 255)) {
            fprintf(stderr, "*** DESYNC expected seed %u vs current %u ***\n", x.seed, _random.state[0] & 255);
        }
        _fake_commands.pop();
    }
    _current_company = backup_company;
}


void load_replay_commands(const std::string &filename, std::function<void(const std::string &)> error_func) {
    _fake_commands = {};

    FILE *f = fopen(filename.c_str(), "rb");

    if (f == nullptr) {
        error_func(fmt::format("Cannot open file `{}`: {}", filename, std::strerror(errno)));
        return;
    }

    lzma_stream lzma = LZMA_STREAM_INIT;
    lzma_ret ret = lzma_auto_decoder(&lzma, 1 << 28, 0);
    if (ret != LZMA_OK) {
        error_func(fmt::format("Cannot initialize LZMA decompressor (code {})", ret));
        return;
    }

    static const size_t CHUNK_SIZE = 128 * 1024;
    static uint8_t inbuf[CHUNK_SIZE];

    lzma.next_in = NULL;
    lzma.avail_in = 0;

    lzma_action action = LZMA_RUN;

    u8vector data(CHUNK_SIZE);
    size_t bytes_read = 0;
    lzma.next_out = &data[0];
    lzma.avail_out = data.size();
    do {
        if (lzma.avail_in == 0 && !std::feof(f)) {
            lzma.next_in = inbuf;
            lzma.avail_in = std::fread((char *)inbuf, 1, sizeof(inbuf), f);

            if (std::ferror(f)) {
                error_func(fmt::format("Error reading {}: {}", filename, std::strerror(errno)));
                return;
            }

            if (std::feof(f)) action = LZMA_FINISH;
        }

        ret = lzma_code(&lzma, action);

        if (lzma.avail_out == 0 || ret == LZMA_STREAM_END) {
            bytes_read += CHUNK_SIZE - lzma.avail_out;
            data.resize(bytes_read + CHUNK_SIZE);
            lzma.next_out = &data[bytes_read];
            lzma.avail_out = CHUNK_SIZE;
        }
    } while (ret == LZMA_OK);

    std::fclose(f);

    data.resize(bytes_read);

    if (ret != LZMA_STREAM_END) {
        error_func(fmt::format("LZMA decompressor returned error code {}", ret));
        // return;
    }

    auto bs = BitIStream(data);
    auto version = bs.ReadBytes(2);
    if (version != 2) {
        error_func(fmt::format("Unsupported log file version {}", version));
        return;
    }

    auto openttd_version = bs.ReadBytes(4);

    if (_openttd_newgrf_version != openttd_version) {
        error_func(fmt::format("OpenTTD version doesn't match: current {}, log file {}",
                               _openttd_newgrf_version, openttd_version));
        return;
    }
    try {
        while (!bs.eof()) {
            FakeCommand fk;
            fk.counter = bs.ReadBytes(4);
            fk.res = bs.ReadBytes(1);
            fk.seed = bs.ReadBytes(1);
            fk.cp.company = (Owner)bs.ReadBytes(1);
            fk.client_id = bs.ReadBytes(2);
            fk.cp.cmd = (Commands)bs.ReadBytes(2);
            fk.cp.data = bs.ReadData();
            fk.cp.callback = nullptr;
            _fake_commands.push(fk);
            error_func(fmt::format("Command {}({}) company={} client={}", GetCommandName(fk.cp.cmd), fk.cp.cmd, fk.cp.company, fk.client_id));
        }
    }
    catch (BitIStreamUnexpectedEnd &) {
        error_func("Unexpected end of command data");
    }

    _replay_started = false;
}

bool IsReplayingCommands() {
    return !_fake_commands.empty();
}


};  // namespace citymania
