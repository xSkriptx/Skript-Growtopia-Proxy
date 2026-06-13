#pragma once

#include "command_base.hpp"
#include "../../core/core.hpp"

namespace command {

class IgnoreCSNChatCommand : public CommandBase {
public:
    IgnoreCSNChatCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;

    static void set_core(core::Core* core);

private:
    static core::Core* s_core;
};

} 

