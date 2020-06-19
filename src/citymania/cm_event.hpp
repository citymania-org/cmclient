#ifndef CM_EVENT_HPP
#define CM_EVENT_HPP

#include "cm_type.hpp"
#include "../cargo_type.h"
#include "../company_type.h"
#include "../economy_type.h"
#include "../station_type.h"
#include "../town_type.h"

#include <functional>
#include <map>
#include <typeindex>
#include <typeinfo>

namespace citymania {

namespace event {

struct NewMonth {
};

struct TownGrowthSucceeded {
    Town *town;
    TileIndex tile;
    uint32 prev_houses;
};

struct TownGrowthFailed {
    Town *town;
    TileIndex tile;
};

struct HouseRebuilt {
    Town *town;
    TileIndex tile;
    bool was_successful;
};

struct HouseBuilt {
    Town *town;
    TileIndex tile;
    const HouseSpec *house_spec;
};

struct HouseCompleted {
    Town *town;
    TileIndex tile;
    const HouseSpec *house_spec;
};

struct CompanyEvent {
    Company *company;
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

    void listen(Handler &handler) {
        this->new_handlers.push_back(handler);
    }

    void emit(const T &event) {
        if (!this->new_handlers.empty()) {
            this->handlers.insert(this->handlers.end(), this->new_handlers.begin(), this->new_handlers.end());
            this->new_handlers.clear();
        }
        for (auto &h : this->handlers) {
            h(event);
        }
    }

protected:
    std::vector<Handler> handlers;
    std::vector<Handler> new_handlers;
};


class Dispatcher {
protected:
    std::map<std::type_index, up<TypeDispatcherBase>> dispacthers;

    template<typename T>
    TypeDispatcher<T> &get_dispatcher() {
        auto p = this->dispacthers.find(typeid(T));
        if (p == this->dispacthers.end()) {
            auto x = make_up<TypeDispatcher<T>>();
            p = this->dispacthers.emplace_hint(p, typeid(T), std::move(x));
        }
        return *(static_cast<TypeDispatcher<T> *>((*p).second.get()));
    }

public:
    template<typename T>
    void listen(std::function<void(const T &)> handler) {
        this->get_dispatcher<T>().listen(handler);
    }

    template<typename T>
    void emit(const T &event) {
        this->get_dispatcher<T>().emit(event);
    }
};

}  // namespace event

}  // namespace citymania

#endif
