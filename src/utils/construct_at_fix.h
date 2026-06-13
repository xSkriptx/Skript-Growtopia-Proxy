#pragma once





#define _CONSTRUCT_AT_DEFINED 1

#include <new>
#include <type_traits>


#ifdef _STL_CONSTRUCT_AT
#undef _STL_CONSTRUCT_AT
#endif


inline namespace construct_at_override {
    template <class T, class... Args>
    constexpr T* construct_at(T* p, Args&&... args) {
        return ::new (const_cast<void*>(static_cast<const volatile void*>(p))) T(std::forward<Args>(args)...);
    }
}


namespace std {
    using ::construct_at_override::construct_at;
    
    template<typename T>
    constexpr void destroy_at(T* p) {
        p->~T();
    }
}


template <class T, class... Args>
constexpr T* construct_at(T* p, Args&&... args) {
    return ::new (const_cast<void*>(static_cast<const volatile void*>(p))) T(std::forward<Args>(args)...);
}