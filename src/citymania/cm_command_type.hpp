#ifndef CM_COMMAND_TYPE_HPP
#define CM_COMMAND_TYPE_HPP

#include "../bridge.h"
#include "../command_func.h"
#include "../depot_type.h"
#include "../goal_type.h"
#include "../group_cmd.h"
#include "../engine_type.h"
#include "../livery.h"
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

class Command {
public:
    TileIndex tile = 0;
    bool automatic = false;
    CompanyID company = INVALID_COMPANY;
    StringID error = (StringID)0;

    Command() {}
    Command(TileIndex tile) :tile{tile} {}
    virtual ~Command() {}

    virtual bool do_post(CommandCallback *callback)=0;
    virtual bool do_test()=0;

    template <typename Tcallback>
    bool post(Tcallback callback) {
        CompanyID old = _current_company;
        if (this->company != INVALID_COMPANY)
            _current_company = company;
        bool res = this->do_post(reinterpret_cast<CommandCallback *>(reinterpret_cast<void(*)()>(callback)));
        _current_company = old;
        return res;
    }

    bool post() {
        return this->post<CommandCallback *>(nullptr);
    }

    bool test() {
        CompanyID old = _current_company;
        if (this->company != INVALID_COMPANY)
            _current_company = company;
        bool res = this->do_test();
        _current_company = old;
        return res;
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
};

}  // namaespace citymania

#endif
