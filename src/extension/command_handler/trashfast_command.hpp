#pragma once
#include "command_base.hpp"
#include "../../core/core.hpp"

namespace command {

class TrashFastCommand : public CommandBase {
public:
    TrashFastCommand();
    std::unique_ptr<CommandBase> clone() const override;
    void execute(client::Client* client, const std::vector<std::string>& args) override;

    static void set_core(core::Core* core);
    static bool is_enabled();
    static void toggle();

private:
    static core::Core* s_core;
    static bool s_enabled;
};

} 
