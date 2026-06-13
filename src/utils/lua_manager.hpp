#pragma once
#include "lua_bridge.hpp"
#include <memory>

namespace lua {

class LuaManager {
    static inline std::unique_ptr<LuaBridge> instance_ = nullptr;
    
public:
    static void initialize() {
        if (!instance_) {
            instance_ = std::make_unique<LuaBridge>();
        }
    }
    
    static LuaBridge* get() {
        return instance_.get();
    }
    
    static void shutdown() {
        instance_.reset();
    }
};

} 
