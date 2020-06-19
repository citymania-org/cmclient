#ifndef CMEXT_TYPE_HPP
#define CMEXT_TYPE_HPP

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

} // namespace citymania

#endif
