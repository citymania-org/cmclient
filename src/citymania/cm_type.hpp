#ifndef CMEXT_TYPE_HPP
#define CMEXT_TYPE_HPP

#include "../core/geometry_type.hpp"
#include "../core/overflowsafe_type.hpp"

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

namespace citymania {

// Make smart pointers easier to type
template<class T> using up=std::unique_ptr<T>;
template<class T> using sp=std::shared_ptr<T>;
template<class T> using wp=std::weak_ptr<T>;

/* C++14 implementation of make_unique */
template<class T> struct _Unique_if {
    typedef std::unique_ptr<T> _Single_object;
};

template<class T> struct _Unique_if<T[]> {
    typedef std::unique_ptr<T[]> _Unknown_bound;
};

template<class T, size_t N> struct _Unique_if<T[N]> {
    typedef void _Known_bound;
};

template<class T, class... Args>
    typename _Unique_if<T>::_Single_object
    make_up(Args&&... args) {
        return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
    }

template<class T>
    typename _Unique_if<T>::_Unknown_bound
    make_up(size_t n) {
        typedef typename std::remove_extent<T>::type U;
        return std::unique_ptr<T>(new U[n]());
    }

template<class T, class... Args>
    typename _Unique_if<T>::_Known_bound
    make_up(Args&&...) = delete;

// template<typename T> const auto make_up = std::make_unique<T>;
// template<typename T> const auto make_sp = std::make_shared<T>;
template<typename T, class... Args> const auto make_sp = std::make_shared<T, Args...>;


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
