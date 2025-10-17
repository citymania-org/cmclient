#ifndef CMEXT_TYPE_HPP
#define CMEXT_TYPE_HPP

#include "../core/geometry_type.hpp"
#include "../core/overflowsafe_type.hpp"

#include <cstddef>
#include <memory>
#include <sstream>
#include <type_traits>
#include <utility>

namespace citymania {

// Make smart pointers easier to type
template<class T> using up=std::unique_ptr<T>;
template<class T> using sp=std::shared_ptr<T>;
template<class T> using wp=std::weak_ptr<T>;

template <typename T, typename... Args>
constexpr auto make_up(Args&&... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}

template <typename T, typename... Args>
constexpr auto make_sp(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

// template<typename T, class... Args> constexpr auto make_sp = std::make_shared<T, Args...>;


enum class GameType: uint8_t {
    GENERIC = 0,
    GOAL = 1,
    MULTIGOAL = 2,
    QUEST = 3,
    BR = 4,
    _NOT_A_GAME_TYPE_CLASSIC_CB = 4,
    _NOT_A_GAME_TYPE_CB = 5,
    _NOT_A_GAME_TYPE_TOWN_DEFENCE = 6,
};

enum class ControllerType: uint8_t {
    GENERIC = 0,
    _NOT_A_CONTROLLER_GOAL = 1,
    _NOT_A_CONTROLLER_MULTIGOAL = 2,
    _NOT_A_CONTROLLER_QUEST = 3,
    CLASSIC_CB = 4,
    CB = 5,
    TOWN_DEFENCE = 6,
};


// For use with std::visit
template<typename ... Ts>
struct Overload : Ts ... {
    using Ts::operator() ...;
};
template<class... Ts> Overload(Ts...) -> Overload<Ts...>;


// Some utility funcitons for strings
namespace string {

template<typename T>
static inline std::string join(T strings, std::string separator) {
    // TODO add map function (can be used in ListGameModeCodes)?
    std::ostringstream res;
    bool first = true;
    for (auto s: strings) {
        if (!first)res << separator;
        res << s;
        first = false;
    }
    return res.str();
}

static inline void iltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int c) {
        return !std::isspace(c);
    }));
}

static inline void irtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int c) {
        return !std::isspace(c);
    }).base(), s.end());
}

static inline void itrim(std::string &s) {
    iltrim(s);
    irtrim(s);
}

static inline std::string ltrim(std::string s) {
    iltrim(s);
    return s;
}

static inline std::string rtrim(std::string s) {
    irtrim(s);
    return s;
}

static inline std::string trim(std::string s) {
    itrim(s);
    return s;
}

}; // namespace string

} // namespace citymania


template <typename T>
struct fmt::formatter<OverflowSafeInt<T>> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const OverflowSafeInt<T>& i, FormatContext& ctx) {
        return fmt::format_to(ctx.out(), "{}", static_cast<T>(i));
    }
};

template <>
struct fmt::formatter<Point> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const Point& pt, FormatContext& ctx) {
        return fmt::format_to(ctx.out(), "({},{})", pt.x, pt.y);
    }
};


#endif
