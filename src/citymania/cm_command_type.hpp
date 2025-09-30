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

extern bool _no_estimate_command;
extern CommandCallback _current_callback;

class Command {
public:
    bool no_estimate_flag = false;
    CompanyID company = INVALID_COMPANY;
    StringID error = (StringID)0;
    CommandCallback callback = nullptr;

    Command() {}
    virtual ~Command() {}

    virtual bool _post(::CommandCallback *callback)=0;
    virtual CommandCost _do(DoCommandFlag flags)=0;
    virtual Commands get_command()=0;

    template <typename Tcallback>
    bool post(Tcallback callback) {
        CompanyID company_backup = _current_company;
        if (this->company != INVALID_COMPANY)
            _current_company = company;
        _no_estimate_command = this->no_estimate_flag;
        _current_callback = this->callback;
        bool res = this->_post(reinterpret_cast<::CommandCallback *>(reinterpret_cast<void(*)()>(callback)));
        _current_company = company_backup;
        _no_estimate_command = false;
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

    CommandCost test() {
        return this->call(DC_NONE);
    }

    Command &with_error(StringID error) {
        this->error = error;
        return *this;
    }

    Command &no_estimate() {
        this->no_estimate_flag = true;
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

class StationBuildCommand : public Command {
public:
    StationID station_to_join;
    bool adjacent;

    StationBuildCommand(StationID station_to_join, bool adjacent)
        :station_to_join{station_to_join}, adjacent{adjacent} {}
};

}  // namaespace citymania

#endif
