
#pragma once
#include "command_base.hpp"

namespace command {
class BankCheckCommand : public CommandBase {
public:
    BankCheckCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
};
} 
