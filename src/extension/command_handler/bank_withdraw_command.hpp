#pragma once
#include "command_base.hpp"

namespace command {
class BankWithdrawCommand : public CommandBase {
public:
    BankWithdrawCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    static void set_core(core::Core* core);
private:
    static core::Core* s_core;
};
} 
