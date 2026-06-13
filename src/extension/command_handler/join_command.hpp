#pragma once
#include "command_base.hpp"
#include "../../player/player.hpp"


namespace core {
    class Core;
}

namespace command {

class JoinCommand : public CommandBase {
public:
    JoinCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    static void set_core(core::Core* core);

    
    static void toggle_pull_mode();
    static void toggle_kick_mode();
    static void toggle_ban_mode();
    
    
    static bool is_pull_mode_enabled();
    static bool is_kick_mode_enabled();
    static bool is_ban_mode_enabled();
    static std::string get_current_mode();

    
    static void handle_dialog_click(player::Player* player, const std::string& button_clicked);
    static void handle_spawn_packet(player::Player* player, const std::string& player_name);

private:
    static void show_join_gui(client::Client* client);
};

} 
