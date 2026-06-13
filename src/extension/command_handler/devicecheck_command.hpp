#pragma once
#include "command_base.hpp"
#include "../../core/core.hpp"

namespace command {

class DeviceCheckCommand : public CommandBase {
public:
    DeviceCheckCommand();
    ~DeviceCheckCommand() override = default;

    std::unique_ptr<CommandBase> clone() const override;
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    static void set_core(core::Core* core);

private:
    static core::Core* g_core;
};

} 
