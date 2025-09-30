#ifndef CM_EVENT_HPP
#define CM_EVENT_HPP

#include "cm_type.hpp"

#include "../cargo_type.h"
#include "../economy_type.h"

#include <functional>
#include <map>
#include <typeindex>
#include <typeinfo>

struct Company;
struct Industry;
struct Station;
struct Source;
struct Town;

namespace citymania {

namespace event {

struct NewMonth {};

struct TownBuilt {
    Town *town;
};

struct TownGrowthSucceeded {
    Town *town;
    TileIndex tile;
    uint32_t prev_houses;
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

struct HouseDestroyed {  // by dynamite, called after HouseCleared
    CompanyID company_id;
    Town *town;
    TileIndex tile;
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
    CargoType cargo_type;
    uint amount;
    const Station *station;
};

struct CargoDeliveredToUnknown {
    CargoType cargo_type;
    uint amount;
    const Station *station;
};

struct CargoAccepted {
    Company *company;
    CargoType cargo_type;
    uint amount;
    const Station *station;
    Money profit;
    Source src;
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

enum class Slot : uint8_t {
    GOAL = 10,
    CONTROLLER = 20,
    GAME = 30,
    CONTROLLER_POST = 40,
    RECORDER = 50,
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
        this->handler_map.insert(std::make_pair(slot, handler));
        this->new_handlers = true;
    }

    void emit(const T &event) {
        if (this->new_handlers) {  // only rebuild handlers while not iterating
            this->handlers.clear();
            for (auto &p : this->handler_map) {
                this->handlers.push_back(p.second);
            }
            this->new_handlers = false;
        }
        for (auto &h : this->handlers) {
            h(event);
        }
    }

protected:
    std::vector<Handler> handlers;
    std::multimap<Slot, Handler> handler_map;
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
