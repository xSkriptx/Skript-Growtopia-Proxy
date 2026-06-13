#pragma once

#include "command_base.hpp"
#include "../../core/core.hpp"
#include <string>

namespace command {

class ModDetectCommand : public CommandBase {
public:
    ModDetectCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;

    static void set_core(core::Core* core);
    static void handle_spawn_packet(const std::string& spawn_data);

    
    static bool is_enabled();
    static void toggle();

private:
    static core::Core* s_core;
};

} 

