#pragma once

#include "command_base.hpp"
#include "../../core/core.hpp"

namespace command {

class LockeTest001Command : public CommandBase {
public:
    LockeTest001Command();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;

    static void set_core(core::Core* core);

private:
    static core::Core* g_core;
};

} 

