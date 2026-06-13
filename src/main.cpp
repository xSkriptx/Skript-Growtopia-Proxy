#include "core/core.hpp"
#include "core/logger.hpp"
#include "core/hosts_manager.hpp"
#include "core/auth.hpp"
#include "client/client.hpp"
#include "utils/antidebug.hpp"
#include "utils/discord_presence.hpp"
#include "utils/gui_sink.hpp"
#include "utils/lua_manager.hpp"
#include "proxy_imgui_gui.hpp"
#include <functional>
#include <chrono>
#include <atomic>

#include "extension/parser/parser_impl.hpp"
#include "extension/sub_server_switch/sub_server_switch_impl.hpp"
#include "extension/web_server/web_server_impl.hpp"
#include "extension/command_handler/command_handler_impl.hpp"
#include "extension/command_handler/doorbf_tree.hpp"
#include "extension/packet_filter_extension.hpp"
#include "extension/item_finder/item_finder_extension.hpp"
#include "extension/vending_fast/vending_fast_extension.hpp"
#include "extension/vendsafe/vendsafe_extension.hpp"
#include "extension/autosurg/autosurg_extension.hpp"
#include "extension/autocrime/autocrime_extension.hpp"
#include "extension/join_mode/join_mode_extension.hpp"
#include "extension/world_logger_extension.hpp"
#include "extension/packet_interceptor_extension.hpp"
#include "extension/lua_scripting_extension.hpp"
#include "extension/player_state_tracker/player_state_tracker_impl.hpp"
#include <fmt/format.h>

#ifdef _WIN32
#include <tchar.h>
#endif


#ifdef _WIN32
#include <windows.h>
#include <csignal>
#else
#include <csignal>
#include <unistd.h>
#endif
#include <algorithm>
#include <atomic>
#include <cctype>
#include <iostream>
#include <memory>
#include <sstream>
#include <thread>
#include <vector>


static core::HostsManager* global_hosts_manager = nullptr;

static std::atomic<core::Core*> g_core{ nullptr };
static const std::vector<std::string> gt_entries = {
    "127.0.0.1 www.growtopia1.com",
    "127.0.0.1 www.growtopia2.com"
};

namespace {

void print_purple_console(const std::string& msg) {
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info{};
    WORD original = 0;
    if (GetConsoleScreenBufferInfo(h, &info)) {
        original = info.wAttributes;
    }
    SetConsoleTextAttribute(h, FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
    std::cout << msg << std::endl;
    if (original != 0) {
        SetConsoleTextAttribute(h, original);
    }
#else
    std::cout << "\033[95m" << msg << "\033[0m" << std::endl;
#endif
}

std::string trim_copy(std::string s) {
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    return s;
}

std::vector<std::string> split_whitespace(const std::string& line) {
    std::istringstream iss(line);
    std::vector<std::string> out;
    std::string token;
    while (iss >> token) out.push_back(token);
    return out;
}

void print_doorbf_help_console() {
    print_purple_console("[DevDoorBF] Usage:");
    print_purple_console("[DevDoorBF]  doorbf            -> enter paste mode for list");
    print_purple_console("[DevDoorBF]  doorbf 1-100 lol -> parse/save list immediately");
    print_purple_console("[DevDoorBF]  doorbf ?list      -> show all saved ids");
    print_purple_console("[DevDoorBF]  doorbf ?clearlist -> clear saved list");
    print_purple_console("[DevDoorBF]  doorbf ?help      -> show this help");
}

void start_dev_console_thread() {
#ifdef _WIN32
    auto dev_console_open = std::make_shared<std::atomic<bool>>(false);

    
    std::thread([dev_console_open] {
        int tap_count = 0;
        auto first_tap = std::chrono::steady_clock::time_point{};
        auto last_trigger = std::chrono::steady_clock::time_point{};
        bool prev_left_down = false;
        bool prev_right_down = false;

        while (true) {
            const bool left_down = (GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0;
            const bool right_down = (GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0;

            const bool left_tap = left_down && !prev_left_down;
            const bool right_tap = right_down && !prev_right_down;
            if (left_tap || right_tap) {
                const auto now = std::chrono::steady_clock::now();
                const auto since_trigger = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_trigger).count();
                if (since_trigger < 1200) {
                    prev_left_down = left_down;
                    prev_right_down = right_down;
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                    continue;
                }

                if (tap_count == 0 ||
                    std::chrono::duration_cast<std::chrono::milliseconds>(now - first_tap).count() > 1200) {
                    tap_count = 1;
                    first_tap = now;
                } else {
                    tap_count++;
                }

                if (tap_count >= 2 && !dev_console_open->load()) {
                    dev_console_open->store(true);
                    last_trigger = now;
                    print_purple_console("You've entered the proxy dev commands. Enter help to see all the instructions.");
                }
            }
            prev_left_down = left_down;
            prev_right_down = right_down;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }).detach();

    std::thread([dev_console_open] {
#else
    auto dev_console_open = std::make_shared<std::atomic<bool>>(true);
    std::thread([dev_console_open] {
#endif
        bool waiting_for_doorbf_list = false;

        for (std::string line; std::getline(std::cin, line);) {
            line = trim_copy(line);
            if (line.empty()) continue;
            if (!dev_console_open->load()) continue;

            if (line == "help") {
                print_doorbf_help_console();
                continue;
            }

            if (waiting_for_doorbf_list) {
                waiting_for_doorbf_list = false;
                if (line == "?help") {
                    print_doorbf_help_console();
                    continue;
                }
                if (line == "?list") {
                    const auto ids = command::doorbf_tree::load_custom_ids(0);
                    if (ids.empty()) {
                        print_purple_console("[DevDoorBF] No saved IDs.");
                    } else {
                        print_purple_console(fmt::format("[DevDoorBF] Saved IDs ({}):", ids.size()));
                        for (const auto& id : ids) {
                            print_purple_console("  " + id);
                        }
                    }
                    continue;
                }
                if (line == "?clearlist") {
                    command::doorbf_tree::clear_custom_ids();
                    print_purple_console("[DevDoorBF] Saved custom list cleared.");
                    continue;
                }

                std::vector<std::string> args{"doorbf"};
                auto pasted = split_whitespace(line);
                args.insert(args.end(), pasted.begin(), pasted.end());

                const auto parsed = command::doorbf_tree::parse_input(args);
                if (!parsed.error.empty()) {
                    spdlog::warn("[DevDoorBF] {}", parsed.error);
                    continue;
                }
                if (parsed.has_custom_ids) {
                    command::doorbf_tree::save_custom_ids(parsed.custom_ids);
                    print_purple_console(fmt::format("[DevDoorBF] Saved {} IDs from pasted list.", parsed.custom_ids.size()));
                } else {
                    print_purple_console("[DevDoorBF] No IDs parsed from pasted list.");
                }
                continue;
            }

            auto tokens = split_whitespace(line);
            if (tokens.empty()) continue;

            std::string cmd = tokens[0];
            std::transform(cmd.begin(), cmd.end(), cmd.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            if (cmd != "doorbf") {
                continue;
            }

            if (tokens.size() == 1) {
                waiting_for_doorbf_list = true;
                if (command::doorbf_tree::saved_custom_count() == 0) {
                    print_purple_console("[DevDoorBF] No ID has been registered yet in dev console, please add the IDs.");
                }
                print_purple_console("[DevDoorBF] Paste the brute-force IDs list now. Example: 1-100 good lol pep");
                print_purple_console("[DevDoorBF] Type '?help' for usage or '?clearlist' to clear saved list.");
                continue;
            }

            std::vector<std::string> args(tokens.begin(), tokens.end());
            const auto parsed = command::doorbf_tree::parse_input(args);
            if (!parsed.error.empty()) {
                spdlog::warn("[DevDoorBF] {}", parsed.error);
                continue;
            }
            if (parsed.show_help) {
                print_doorbf_help_console();
                continue;
            }
            if (parsed.show_list) {
                const auto ids = command::doorbf_tree::load_custom_ids(0);
                if (ids.empty()) {
                    print_purple_console("[DevDoorBF] No saved IDs.");
                } else {
                    print_purple_console(fmt::format("[DevDoorBF] Saved IDs ({}):", ids.size()));
                    for (const auto& id : ids) {
                        print_purple_console("  " + id);
                    }
                }
                continue;
            }
            if (parsed.clear_list) {
                command::doorbf_tree::clear_custom_ids();
                print_purple_console("[DevDoorBF] Saved custom list cleared.");
                continue;
            }
            if (parsed.has_custom_ids) {
                command::doorbf_tree::save_custom_ids(parsed.custom_ids);
                print_purple_console(fmt::format("[DevDoorBF] Saved {} IDs.", parsed.custom_ids.size()));
            }
        }
    }).detach();
}

} 

#ifdef _WIN32
BOOL WINAPI console_handler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT) {
        spdlog::info("Shutting down gracefully...");
        utils::DiscordPresence::stop();
        if (global_hosts_manager) {
            global_hosts_manager->remove_entries(gt_entries);
        }
        ExitProcess(0);
    }
    return TRUE;
}
#else
void signal_handler(int signum) {
    spdlog::info("Shutting down gracefully...");
    utils::DiscordPresence::stop();
    if (global_hosts_manager) {
        global_hosts_manager->remove_entries(gt_entries);
    }
    exit(signum);
}
#endif

int main() {
    
#ifdef _WIN32
    if (!SetConsoleCtrlHandler(console_handler, TRUE)) {
        std::cerr << "Could not set console control handler" << std::endl;
        return 1;
    }
#else
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGABRT, signal_handler);
#endif

    
    runAntiDebugChecks();

    
    if (!core::Authenticator::authenticate()) {
        return 1;
    }

    
    startAntiDebugMonitor();

    try {
        core::Logger logger{};

        std::vector<spdlog::sink_ptr> sinks{
            core::Logger::create_console_sink(),
            core::Logger::create_file_sink(),
            std::make_shared<utils::GuiSink_mt>()
        };

        auto main_logger = std::make_shared<spdlog::logger>("Skript", sinks.begin(), sinks.end());
        main_logger->set_level(spdlog::level::debug);
        
        logger.set_logger(main_logger);
        spdlog::register_logger(logger.get_logger());
        spdlog::set_default_logger(logger.get_logger());
        
        
        spdlog::set_pattern("%^%v%$");

        
        
        SetLuaRunner([](const std::string& code, std::string& err) -> bool {
            auto* bridge = lua::LuaManager::get();
            if (!bridge) { err = "Lua not initialized"; return false; }
            err = bridge->execute_with_error(code);
            return err.empty();
        });
        SetCommandRunner([](const std::string& cmd_line) {
            
            core::Core* core = g_core.load();
            if (!core) { AppendLog("[GUI] Core not ready yet"); return; }
            auto* client = core->get_client();
            if (!client || !client->get_player()) {
                AppendLog("[GUI] Not connected – command ignored: " + cmd_line);
                return;
            }
            
            std::string line = cmd_line;
            if (!line.empty() && line[0] == '/') line = line.substr(1);
            auto* handler = core->query_extension<extension::command_handler::CommandHandlerExtension>();
            if (!handler) { AppendLog("[GUI] CommandHandler not found"); return; }
            handler->execute_command(client->get_player(), line);
        });
        SetProxyConfigCallbacks(
            
            [](const std::string& key) -> std::string {
                core::Core* core = g_core.load();
                if (!core) return "";
                if (key == "proxy.enabled")
                    return core->get_config().get<bool>(key) ? "true" : "false";
                if (key == "proxy.port")
                    return std::to_string(core->get_config().get<unsigned int>(key));
                return core->get_config().get<std::string>(key);
            },
            
            [](const std::string& key, const std::string& value) {
                core::Core* core = g_core.load();
                if (!core) return;
                if (key == "proxy.enabled") {
                    core->get_config().set<bool>(key, value == "true" || value == "1");
                } else if (key == "proxy.port") {
                    try { core->get_config().set<unsigned int>(key, (unsigned int)std::stoul(value)); }
                    catch (...) {}
                } else {
                    core->get_config().set<std::string>(key, value);
                }
            }
        );

        
        SetConnectionCallbacks(
            
            
            []() {
                core::Core* core = g_core.load();
                if (!core) { AppendLog("[GUI] Core not ready"); return; }

                
                auto* client = core->get_client();
                if (client && client->get_player()) {
                    AppendLog("[GUI] Already connected to GT server");
                    return;
                }

                
                auto* server = core->get_server();
                if (server && server->get_player()) {
                    server->get_player()->disconnect_now();
                    AppendLog("[GUI] Dropped GT client — GT will auto-reconnect and login");
                } else {
                    AppendLog("[GUI] GT client not connected to proxy yet — open Growtopia first");
                }
            },
            
            []() {
                core::Core* core = g_core.load();
                if (!core) { AppendLog("[GUI] Core not ready"); return; }
                auto* client = core->get_client();
                if (!client) { AppendLog("[GUI] Client not ready"); return; }
                auto* player = client->get_player();
                if (!player) { AppendLog("[GUI] Not connected to GT server"); return; }
                player->disconnect_now();
                AppendLog("[GUI] Disconnected from GT server");
            },
            
            []() -> bool {
                core::Core* core = g_core.load();
                if (!core) return false;
                auto* client = core->get_client();
                if (!client) return false;
                return client->get_player() != nullptr;
            }
        );

        
        std::thread(RunImGuiApp).detach();
    }
    catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Log initialization failed: " << ex.what() << std::endl;
        return 1;
    }

    try {
        spdlog::info("Skript Proxy");
        spdlog::info("Version 2.43 - Relog if you encounter issues");
        spdlog::info("");
        utils::DiscordPresence::start();

        
        core::HostsManager hosts_manager;
        global_hosts_manager = &hosts_manager;
        
        
        if (!hosts_manager.add_entries_if_needed(gt_entries)) {
            spdlog::info("Hosts file modification failed - Administrator privileges required");
            return 1;
        }

        spdlog::info("Network configuration completed");

        core::Core core{};
        g_core.store(&core);  

        spdlog::info("Loading extensions...");
        start_dev_console_thread();
        
        
        core.add_extension(new extension::web_server::WebServerExtension{ &core });
        core.add_extension(new extension::command_handler::CommandHandlerExtension{ &core });
        core.add_extension(new extension::player_state_tracker::PlayerStateTrackerExtension{ &core });
        core.add_extension(new extension::parser::ParserExtension{ &core });
        core.add_extension(new extension::sub_server_switch::SubServerSwitchExtension{ &core });
        core.add_extension(new extension::packet_filter::PacketFilterExtension{ &core });
        core.add_extension(new extension::item_finder::ItemFinderExtension{ &core });
        core.add_extension(new extension::vending_fast::VendingFastExtension{ &core });
        core.add_extension(new extension::vendsafe::VendSafeExtension{ &core });
        core.add_extension(new extension::autosurg::AutoSurgExtension{ &core });
        core.add_extension(new extension::autocrime::AutoCrimeExtension{ &core });
        core.add_extension(new extension::join_mode::JoinModeExtension{ &core });
        core.add_extension(new extension::packet_interceptor::PacketInterceptor{ &core });
        core.add_extension(new extension::world_logger::WorldLoggerExtension{ &core });
        core.add_extension(new extension::lua_scripting::LuaScriptingExtension{ &core });

        spdlog::info("Extension loaded");

        
        spdlog::info("Starting proxy service...");
        spdlog::info("Service status: ACTIVE");
        spdlog::info("");
        core.run();

        
        g_core.store(nullptr);
        hosts_manager.remove_entries(gt_entries);
        spdlog::info("Network configuration restored");
        utils::DiscordPresence::stop();
        stopAntiDebugMonitor();
    }
    catch (const std::runtime_error& e) {
        spdlog::info("System error: {}", e.what());
        if (global_hosts_manager) {
            global_hosts_manager->remove_entries(gt_entries);
        }
        utils::DiscordPresence::stop();
        return 1;
    }
    catch (const std::exception& ex) {
        spdlog::info("Critical exception: {}", ex.what());
        if (global_hosts_manager) {
            global_hosts_manager->remove_entries(gt_entries);
        }
        utils::DiscordPresence::stop();
        return 1;
    }

    return 0;
}
