#pragma once
#include <lua.hpp>
#include <string>
#include <memory>
#include <functional>
#include <utility>
#include <thread>
#include <chrono>
#include <spdlog/spdlog.h>

namespace lua {


using SendPacketCallback = std::function<void(int type, const std::string& data)>;
using GetPlayerPosCallback = std::function<std::pair<float, float>()>;
using ConsoleMessageCallback = std::function<void(const std::string& msg)>;

class LuaBridge {
    lua_State* L;
    
    
    static SendPacketCallback s_send_packet_callback;
    static GetPlayerPosCallback s_get_player_pos_callback;
    static ConsoleMessageCallback s_console_message_callback;
    
    
    static int lua_send_packet(lua_State* L) {
        int type = luaL_checkinteger(L, 1);
        const char* data = luaL_checkstring(L, 2);
        if (!s_send_packet_callback) {
            spdlog::warn("[LUA] SendPacket called but callback not set yet");
            return 0;
        }
        s_send_packet_callback(type, std::string(data));
        return 0;
    }
    
    static int lua_get_player_pos(lua_State* L) {
        if (s_get_player_pos_callback) {
            auto [x, y] = s_get_player_pos_callback();
            lua_pushnumber(L, x);
            lua_pushnumber(L, y);
            return 2; 
        }
        lua_pushnumber(L, 0);
        lua_pushnumber(L, 0);
        return 2;
    }
    
    static int lua_console_message(lua_State* L) {
        const char* msg = luaL_checkstring(L, 1);
        if (s_console_message_callback) {
            s_console_message_callback(std::string(msg));
        }
        return 0;
    }
    
    static int lua_sleep(lua_State* L) {
        int ms = luaL_checkinteger(L, 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        return 0;
    }
    
public:
    LuaBridge() {
        L = luaL_newstate();
        luaL_openlibs(L);
        register_functions();
        spdlog::trace("Lua bridge initialized with C++ bindings");
    }
    
    ~LuaBridge() {
        if (L) {
            lua_close(L);
        }
    }
    
    
    bool execute(const std::string& code) {
        if (luaL_dostring(L, code.c_str()) != LUA_OK) {
            spdlog::error("Lua error: {}", lua_tostring(L, -1));
            lua_pop(L, 1);
            return false;
        }
        return true;
    }

    
    std::string execute_with_error(const std::string& code) {
        if (luaL_dostring(L, code.c_str()) != LUA_OK) {
            std::string err = lua_tostring(L, -1);
            lua_pop(L, 1);
            spdlog::error("Lua error: {}", err);
            return err;
        }
        return {};
    }
    
    
    bool execute_file(const std::string& filepath) {
        if (luaL_dofile(L, filepath.c_str()) != LUA_OK) {
            spdlog::error("Lua file error: {}", lua_tostring(L, -1));
            lua_pop(L, 1);
            return false;
        }
        return true;
    }
    
    
    bool call_function(const std::string& func_name) {
        lua_getglobal(L, func_name.c_str());
        if (!lua_isfunction(L, -1)) {
            spdlog::error("Lua: '{}' is not a function", func_name);
            lua_pop(L, 1);
            return false;
        }
        
        if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            spdlog::error("Lua call error: {}", lua_tostring(L, -1));
            lua_pop(L, 1);
            return false;
        }
        return true;
    }
    
    
    std::string call_function_get_string(const std::string& func_name) {
        lua_getglobal(L, func_name.c_str());
        if (!lua_isfunction(L, -1)) {
            spdlog::error("Lua: '{}' is not a function", func_name);
            lua_pop(L, 1);
            return "";
        }
        
        if (lua_pcall(L, 0, 1, 0) != LUA_OK) {
            spdlog::error("Lua call error: {}", lua_tostring(L, -1));
            lua_pop(L, 1);
            return "";
        }
        
        std::string result = "";
        if (lua_isstring(L, -1)) {
            result = lua_tostring(L, -1);
        }
        lua_pop(L, 1);
        return result;
    }
    
    
    lua_State* get_state() { return L; }
    
    
    
    
    void register_functions() {
        lua_register(L, "OnConsoleMessage", lua_console_message);
        spdlog::trace("Lua: Registered C++ functions");
    }
    
    
    void set_send_packet_callback(SendPacketCallback cb) {
        s_send_packet_callback = cb;
    }
    
    void set_get_player_pos_callback(GetPlayerPosCallback cb) {
        s_get_player_pos_callback = cb;
    }
    
    void set_console_message_callback(ConsoleMessageCallback cb) {
        s_console_message_callback = cb;
    }
};


inline SendPacketCallback LuaBridge::s_send_packet_callback = nullptr;
inline GetPlayerPosCallback LuaBridge::s_get_player_pos_callback = nullptr;
inline ConsoleMessageCallback LuaBridge::s_console_message_callback = nullptr;

} 
