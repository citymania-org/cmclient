#ifndef CM_COMMAND_TYPE_HPP
#define CM_COMMAND_TYPE_HPP

#include "../bridge.h"
#include "../command_func.h"
#include "../depot_type.h"
#include "../goal_type.h"
#include "../group_cmd.h"
#include "../engine_type.h"
#include "../livery.h"
#include "../network/network_type.h"
#include "../misc_cmd.h"
#include "../news_type.h"
#include "../object_type.h"
#include "../order_type.h"
#include "../road_type.h"
#include "../road_type.h"
#include "../station_type.h"
#include "../story_type.h"
#include "../track_type.h"
#include "../vehiclelist.h"

enum StationClassID : byte;

namespace citymania {

typedef std::function<bool(bool)> CommandCallback;

extern bool _auto_command;
extern CommandCallback _current_callback;

class Command {
public:
    TileIndex tile = 0;
    bool automatic = false;
    CompanyID company = INVALID_COMPANY;
    StringID error = (StringID)0;
    CommandCallback callback = nullptr;

    Command() {}
    Command(TileIndex tile) :tile{tile} {}
    virtual ~Command() {}

    virtual bool _post(::CommandCallback *callback)=0;
    virtual CommandCost _do(DoCommandFlag flags)=0;
    virtual Commands get_command()=0;

    template <typename Tcallback>
    bool post(Tcallback callback) {
        CompanyID old = _current_company;
        if (this->company != INVALID_COMPANY)
            _current_company = company;
        _auto_command = this->automatic;
        _current_callback = this->callback;
        bool res = this->_post(reinterpret_cast<::CommandCallback *>(reinterpret_cast<void(*)()>(callback)));
        assert(_current_callback == nullptr);
        _current_callback = nullptr;
        _auto_command = false;
        _current_company = old;
        return res;
    }

    bool post() {
        return this->post<::CommandCallback *>(nullptr);
    }

    CommandCost call(DoCommandFlag flags) {
        CompanyID old = _current_company;
        if (this->company != INVALID_COMPANY)
            _current_company = company;
        auto res = this->_do(flags);
        _current_company = old;
        return res;
    }

    bool test() {
        return this->call(DC_NONE).Succeeded();
    }

    Command &with_tile(TileIndex tile) {
        this->tile = tile;
        return *this;
    }

    Command &with_error(StringID error) {
        this->error = error;
        return *this;
    }

    Command &set_auto() {
        this->automatic = true;
        return *this;
    }

    Command &as_company(CompanyID company) {
        this->company = company;
        return *this;
    }

    Command &with_callback(CommandCallback callback) {
        this->callback = callback;
        return *this;
    }
};

}  // namaespace citymania

#endif
