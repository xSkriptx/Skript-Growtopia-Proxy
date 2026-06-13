#pragma once
#include "command_base.hpp"
#include "../../player/player.hpp"
#include "../../core/core.hpp"
#include <string>
#include <atomic>
#include <thread>

namespace command {

class DoorIDCommand : public CommandBase {
public:
    DoorIDCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    
    static void set_core(core::Core* core);
    static void toggle_door_id_reveal();
    static bool is_door_id_reveal_enabled();
    static void handle_send_to_server(const std::string& server_info);

private:
    static void start_monitor();
    static void stop_monitor();
    static void monitor_loop();
    static void send_console_message(const std::string& message);
    static int find_door_id_near(int tx, int ty, std::string& out_id);

    static bool s_reveal_enabled;
    static core::Core* s_core;
    static std::atomic<bool> s_monitor_running;
    static std::thread s_monitor_thread;
    static int s_last_tile_x;
    static int s_last_tile_y;
    static bool s_have_last_tile;
    static std::string s_last_reported_door_id;
};

} 
