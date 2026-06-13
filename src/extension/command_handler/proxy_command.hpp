#pragma once
#include "command_base.hpp"
#include "../../core/core.hpp"
#include <string>
#include <vector>

namespace command {

class ProxyCommand : public CommandBase {
public:
    ProxyCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    static void set_core(core::Core* core);
    static void show_commands_gui(player::Player* player, core::Core* core);
};

class InfoCommand : public CommandBase {
public:
    InfoCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    static void set_core(core::Core* core);
};

} 
