#pragma once
#include "command_base.hpp"


namespace core {
    class Core;
}

namespace command {

class PingCommand : public CommandBase {
public:
    PingCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    void execute_with_core(client::Client* client, const std::vector<std::string>& args, core::Core* core);
    std::unique_ptr<CommandBase> clone() const override;
};

}
