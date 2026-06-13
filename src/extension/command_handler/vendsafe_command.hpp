#pragma once
#include "command_base.hpp"
#include "../../core/core.hpp"

namespace command {

class VendSafeCommand : public CommandBase {
public:
    VendSafeCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;

    static void set_core(core::Core* core);
    static bool is_enabled();
    static void set_enabled(bool enabled);

private:
    static core::Core* s_core;
    static bool s_enabled;
};

} 

