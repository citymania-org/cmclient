#ifndef CM_COMMAND_TYPE_HPP
#define CM_COMMAND_TYPE_HPP

#include "../bridge.h"
#include "../command_func.h"
#include "../misc_cmd.h"
#include "../object_type.h"
#include "../order_type.h"
#include "../road_type.h"
#include "../road_type.h"
#include "../station_type.h"
#include "../track_type.h"

enum StationClassID : byte;

namespace citymania {

class Command {
public:
    TileIndex tile = 0;
    bool automatic = false;
    CompanyID as_company = INVALID_COMPANY;
    StringID error = (StringID)0;

    Command() {}
    Command(TileIndex tile) :tile{tile} {}
    virtual ~Command() {}
    virtual bool DoPost()=0;
    virtual bool DoTest()=0;

    bool Post() {
        CompanyID old = _current_company;
        if (this->as_company != INVALID_COMPANY)
            _current_company = as_company;
        bool res = this->DoPost();
        _current_company = old;
        return res;
    }

    bool Test() {
        CompanyID old = _current_company;
        if (this->as_company != INVALID_COMPANY)
            _current_company = as_company;
        bool res = this->DoTest();
        _current_company = old;
        return res;
    }

    Command &WithTile(TileIndex tile) {
        this->tile = tile;
        return *this;
    }

    Command &WithError(StringID error) {
        this->error = error;
        return *this;
    }

    Command &SetAuto() {
        this->automatic = true;
        return *this;
    }

    Command &AsCompany(CompanyID company) {
        this->as_company = company;
        return *this;
    }
};

}  // namaespace citymania

#endif
