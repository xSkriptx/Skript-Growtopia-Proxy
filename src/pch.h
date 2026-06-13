#pragma once


#include <memory>
#include <new>
#include <type_traits>

namespace std {
    template <class _Ty, class... _Types>
    constexpr _Ty* construct_at(_Ty* const _Ptr, _Types&&... _Args) {
        return ::new (const_cast<void*>(static_cast<const volatile void*>(_Ptr))) _Ty(std::forward<_Types>(_Args)...);
    }
    
    template<typename T>
    constexpr void destroy_at(T* p) {
        if constexpr (std::is_array_v<T>) {
            for (auto& elem : *p) {
                std::destroy_at(std::addressof(elem));
            }
        } else {
            p->~T();
        }
    }
}


#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include <memory>
#include <algorithm>