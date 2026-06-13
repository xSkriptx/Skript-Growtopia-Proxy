#pragma once
#include "command_base.hpp"
#include "../../core/core.hpp"
#include <vector>
#include <string>

namespace command {

class PullallCommand : public CommandBase {
public:
    PullallCommand();
    std::unique_ptr<CommandBase> clone() const override;
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    
    static void set_core(core::Core* core);
    static void handle_spawn_packet(player::Player* player, const std::string& player_name);

private:
    static core::Core* g_core;
};

} 
