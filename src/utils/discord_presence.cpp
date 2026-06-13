#include "discord_presence.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <thread>

#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <windows.h>
#include <shobjidl_core.h>
#endif

namespace utils {

namespace {

#ifdef _WIN32
struct DiscordRichPresence {
    const char* state;
    const char* details;
    long long startTimestamp;
    long long endTimestamp;
    const char* largeImageKey;
    const char* largeImageText;
    const char* smallImageKey;
    const char* smallImageText;
    const char* partyId;
    int partySize;
    int partyMax;
    const char* matchSecret;
    const char* joinSecret;
    const char* spectateSecret;
    int8_t instance;
};

using DiscordInitializeFn = void(__cdecl*)(const char*, void*, int, const char*);
using DiscordShutdownFn = void(__cdecl*)();
using DiscordUpdatePresenceFn = void(__cdecl*)(const DiscordRichPresence*);
using DiscordRunCallbacksFn = void(__cdecl*)();
using DiscordClearPresenceFn = void(__cdecl*)();

HMODULE g_rpc_module = nullptr;
DiscordInitializeFn g_initialize = nullptr;
DiscordShutdownFn g_shutdown = nullptr;
DiscordUpdatePresenceFn g_update_presence = nullptr;
DiscordRunCallbacksFn g_run_callbacks = nullptr;
DiscordClearPresenceFn g_clear_presence = nullptr;
bool g_initialized = false;
long long g_started_at = 0;
std::atomic<bool> g_callbacks_running{ false };
std::thread g_callbacks_thread;
HANDLE g_ipc_pipe = INVALID_HANDLE_VALUE;
bool g_ipc_connected = false;
std::uint64_t g_nonce_counter = 0;

bool ipc_write_frame(std::int32_t op, const std::string& payload) {
    if (g_ipc_pipe == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD written = 0;
    const std::int32_t len = static_cast<std::int32_t>(payload.size());
    if (!WriteFile(g_ipc_pipe, &op, sizeof(op), &written, nullptr) || written != sizeof(op)) {
        return false;
    }
    if (!WriteFile(g_ipc_pipe, &len, sizeof(len), &written, nullptr) || written != sizeof(len)) {
        return false;
    }
    if (len > 0) {
        if (!WriteFile(g_ipc_pipe, payload.data(), len, &written, nullptr) || written != static_cast<DWORD>(len)) {
            return false;
        }
    }
    return true;
}

bool ipc_read_frame(std::int32_t& op, std::string& payload) {
    if (g_ipc_pipe == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD read = 0;
    std::int32_t len = 0;
    if (!ReadFile(g_ipc_pipe, &op, sizeof(op), &read, nullptr) || read != sizeof(op)) {
        return false;
    }
    if (!ReadFile(g_ipc_pipe, &len, sizeof(len), &read, nullptr) || read != sizeof(len) || len < 0) {
        return false;
    }
    payload.clear();
    payload.resize(static_cast<size_t>(len));
    if (len > 0) {
        if (!ReadFile(g_ipc_pipe, payload.data(), static_cast<DWORD>(len), &read, nullptr) ||
            read != static_cast<DWORD>(len)) {
            return false;
        }
    }
    return true;
}

bool ipc_connect() {
    if (g_ipc_connected && g_ipc_pipe != INVALID_HANDLE_VALUE) {
        return true;
    }

    for (int i = 0; i < 10; ++i) {
        const std::string pipe_name = "\\\\.\\pipe\\discord-ipc-" + std::to_string(i);
        HANDLE h = CreateFileA(
            pipe_name.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr
        );
        if (h != INVALID_HANDLE_VALUE) {
            g_ipc_pipe = h;
            nlohmann::json hs = {
                {"v", 1},
                {"client_id", "1463966283585425504"}
            };
            if (!ipc_write_frame(0, hs.dump())) {
                CloseHandle(g_ipc_pipe);
                g_ipc_pipe = INVALID_HANDLE_VALUE;
                continue;
            }
            std::int32_t op = -1;
            std::string payload;
            if (!ipc_read_frame(op, payload)) {
                CloseHandle(g_ipc_pipe);
                g_ipc_pipe = INVALID_HANDLE_VALUE;
                continue;
            }
            g_ipc_connected = true;
            return true;
        }
    }
    return false;
}

void ipc_disconnect() {
    g_ipc_connected = false;
    if (g_ipc_pipe != INVALID_HANDLE_VALUE) {
        CloseHandle(g_ipc_pipe);
        g_ipc_pipe = INVALID_HANDLE_VALUE;
    }
}

void push_presence_rpc() {
    if (!g_update_presence) {
        return;
    }

    DiscordRichPresence p{};
    p.details = "License: Free Skript Proxy";
    p.state = "Currently In-game";
    p.startTimestamp = g_started_at;
    p.endTimestamp = 0;
    p.largeImageKey = nullptr;
    p.largeImageText = "Visit https://skriptproxy.com/";
    p.smallImageKey = nullptr;
    p.smallImageText = "https://skriptproxy.com";
    p.instance = 1;
    g_update_presence(&p);
}

bool push_presence_ipc() {
    if (!ipc_connect()) {
        return false;
    }

    nlohmann::json activity = {
        {"details", "License: Free Skript Proxy"},
        {"state", "Currently In-game"},
        {"timestamps", {{"start", g_started_at}}},
        {"buttons", nlohmann::json::array({
            nlohmann::json{{"label", "Skript Proxy Discord"}, {"url", "https://discord.gg/Cke6JZWhbN"}},
            nlohmann::json{{"label", "Skript Proxy Website"}, {"url", "https://skriptproxy.com"}}
        })}
    };

    nlohmann::json payload = {
        {"cmd", "SET_ACTIVITY"},
        {"args", {
            {"pid", static_cast<int>(GetCurrentProcessId())},
            {"activity", activity}
        }},
        {"nonce", std::to_string(++g_nonce_counter)}
    };

    if (!ipc_write_frame(1, payload.dump())) {
        ipc_disconnect();
        return false;
    }
    std::int32_t op = -1;
    std::string ack_payload;
    if (!ipc_read_frame(op, ack_payload)) {
        ipc_disconnect();
        return false;
    }
    return true;
}

void callbacks_loop() {
    int refresh_ticks = 0;
    while (g_callbacks_running.load()) {
        if (g_run_callbacks) {
            g_run_callbacks();
        }
        if (++refresh_ticks >= 30) {
            const bool ipc_ok = push_presence_ipc();
            if (!ipc_ok) {
                push_presence_rpc();
            }
            refresh_ticks = 0;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

#endif

} 

void DiscordPresence::start() {
#ifdef _WIN32
    if (g_initialized) {
        return;
    }

    
    SetConsoleTitleA("Skript Proxy");
    SetCurrentProcessExplicitAppUserModelID(L"com.skript.proxy.desktop");

    g_started_at = std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();

    
    g_rpc_module = LoadLibraryA("discord-rpc.dll");
    if (g_rpc_module) {
        g_initialize = reinterpret_cast<DiscordInitializeFn>(GetProcAddress(g_rpc_module, "Discord_Initialize"));
        g_shutdown = reinterpret_cast<DiscordShutdownFn>(GetProcAddress(g_rpc_module, "Discord_Shutdown"));
        g_update_presence = reinterpret_cast<DiscordUpdatePresenceFn>(GetProcAddress(g_rpc_module, "Discord_UpdatePresence"));
        g_run_callbacks = reinterpret_cast<DiscordRunCallbacksFn>(GetProcAddress(g_rpc_module, "Discord_RunCallbacks"));
        g_clear_presence = reinterpret_cast<DiscordClearPresenceFn>(GetProcAddress(g_rpc_module, "Discord_ClearPresence"));

        if (g_initialize && g_shutdown && g_update_presence && g_run_callbacks) {
            g_initialize("1463966283585425504", nullptr, 1, nullptr);
        } else {
            FreeLibrary(g_rpc_module);
            g_rpc_module = nullptr;
            g_initialize = nullptr;
            g_shutdown = nullptr;
            g_update_presence = nullptr;
            g_run_callbacks = nullptr;
            g_clear_presence = nullptr;
        }
    }

    const bool ipc_ok = push_presence_ipc();
    if (!ipc_ok) {
        push_presence_rpc();
    }

    g_callbacks_running.store(true);
    g_callbacks_thread = std::thread(callbacks_loop);
    g_initialized = true;
#endif
}

void DiscordPresence::stop() {
#ifdef _WIN32
    if (!g_initialized) {
        return;
    }

    g_callbacks_running.store(false);
    if (g_callbacks_thread.joinable()) {
        g_callbacks_thread.join();
    }

    if (g_clear_presence) {
        g_clear_presence();
    }
    if (g_shutdown) {
        g_shutdown();
    }

    if (g_rpc_module) {
        FreeLibrary(g_rpc_module);
    }

    g_rpc_module = nullptr;
    g_initialize = nullptr;
    g_shutdown = nullptr;
    g_update_presence = nullptr;
    g_run_callbacks = nullptr;
    g_clear_presence = nullptr;
    g_initialized = false;
    ipc_disconnect();
#endif
}

} 
