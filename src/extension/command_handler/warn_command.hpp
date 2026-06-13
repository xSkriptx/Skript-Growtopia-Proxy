#pragma once
#include "command_base.hpp"
#include "../../utils/text_parse.hpp"
#include "../../packet/message/chat.hpp"


namespace core {
    class Core;
}

namespace command {

class WarnCommand : public CommandBase {
public:
    WarnCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    void execute_with_core(client::Client* client, const std::vector<std::string>& args, core::Core* core);
    std::unique_ptr<CommandBase> clone() const override;
};

} 
