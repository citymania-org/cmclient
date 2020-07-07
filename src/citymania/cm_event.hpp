#ifndef CM_EVENT_HPP
#define CM_EVENT_HPP

#include "cm_type.hpp"

#include "../console_func.h"
#include "../cargo_type.h"
#include "../company_type.h"
#include "../economy_type.h"
#include "../industry_type.h"
#include "../station_type.h"
#include "../town_type.h"

#include <functional>
#include <map>
#include <typeindex>
#include <typeinfo>

namespace citymania {

namespace event {

struct NewMonth {};

struct TownGrowthSucceeded {
    Town *town;
    TileIndex tile;
    uint32 prev_houses;
};

struct TownGrowthFailed {
    Town *town;
    TileIndex tile;
};

struct TownCachesRebuilt {};

struct HouseRebuilt {
    Town *town;
    TileIndex tile;
    bool was_successful;
};

struct HouseBuilt {
    Town *town;
    TileIndex tile;
    HouseID house_id;
    const HouseSpec *house_spec;
    bool is_rebuilding;
};

struct HouseCleared {
    Town *town;
    TileIndex tile;
    HouseID house_id;
    const HouseSpec *house_spec;
    bool was_completed;  ///< whether house was completed before destruction
};

struct HouseCompleted {
    Town *town;
    TileIndex tile;
    HouseID house_id;
    const HouseSpec *house_spec;
};

struct CompanyEvent {
    Company *company;
};

struct CargoDeliveredToIndustry {
    Industry *industry;
    CargoID cargo_type;
    uint amount;
    const Station *station;
};

struct CargoDeliveredToUnknown {
    CargoID cargo_type;
    uint amount;
    const Station *station;
};

struct CargoAccepted {
    Company *company;
    CargoID cargo_type;
    uint amount;
    const Station *station;
    Money profit;
    SourceType src_type;
    SourceID src;
};

struct CompanyMoneyChanged {
    Company *company;
    Money delta;
};

struct CompanyLoanChanged {
    Company *company;
    Money delta;
};

struct CompanyBalanceChanged {
    Company *company;
    Money delta;
};

enum class Slot : uint8 {
    CONTROLLER = 10,
    GAME = 20,
    CONTROLLER_POST = 30,
    RECORDER = 40,
};

class TypeDispatcherBase {
public:
    virtual ~TypeDispatcherBase() {}
};


template<typename T>
class TypeDispatcher: public TypeDispatcherBase {
public:
    typedef std::function<void(const T &)> Handler;

    TypeDispatcher() { }
    virtual ~TypeDispatcher() {}

    void listen(Slot slot, Handler &handler) {
        auto p = this->handler_map.find(slot);
        if (p != this->handler_map.end())
            IConsolePrintF(CC_ERROR, "ERROR: Ignored duplicate handler for event %s slot %d", typeid(T).name(), (int)slot);
        this->handler_map.insert(p, std::make_pair(slot, handler));
        this->new_handlers = true;
    }

    void emit(const T &event) {
        if (this->new_handlers) {  // only rebuild handlers while not iterating
            this->handlers.clear();
            for (auto &p : this->handler_map) {
                this->handlers.push_back(p.second);
            }
        }
        for (auto &h : this->handlers) {
            h(event);
        }
    }

protected:
    std::vector<Handler> handlers;
    std::map<Slot, Handler> handler_map;
    bool new_handlers = false;
};


class Dispatcher {
protected:
    std::map<std::type_index, up<TypeDispatcherBase>> dispatchers;

    template<typename T>
    TypeDispatcher<T> &get_dispatcher() {
        auto p = this->dispatchers.find(typeid(T));
        if (p == this->dispatchers.end()) {
            auto x = make_up<TypeDispatcher<T>>();
            p = this->dispatchers.emplace_hint(p, typeid(T), std::move(x));
        }
        return *(static_cast<TypeDispatcher<T> *>((*p).second.get()));
    }

public:
    template<typename T>
    void listen(Slot slot, std::function<void(const T &)> handler) {
        this->get_dispatcher<T>().listen(slot, handler);
    }

    template<typename T>
    void emit(const T &event) {
        this->get_dispatcher<T>().emit(event);
    }
};

}  // namespace event

}  // namespace citymania

#endif
