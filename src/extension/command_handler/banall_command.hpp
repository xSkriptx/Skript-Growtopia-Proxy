#pragma once
#include "command_base.hpp"
#include "../../core/core.hpp"
#include <vector>
#include <string>

namespace command {

class BanallCommand : public CommandBase {
public:
    BanallCommand();
    std::unique_ptr<CommandBase> clone() const override;
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    
    static void set_core(core::Core* core);
    static void handle_spawn_packet(player::Player* player, const std::string& player_name);
    
    
    static void add_spawned_player(const std::string& name);
    static std::vector<std::string> get_spawned_players();
    static void clear_spawned_players();

private:
    static core::Core* g_core;
    static std::vector<std::string> spawned_players_;
};

} 
