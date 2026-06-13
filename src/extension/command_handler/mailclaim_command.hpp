#pragma once
#include "command_base.hpp"

namespace command {

class MailClaimCommand : public CommandBase {
public:
    MailClaimCommand();
    
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    
    std::unique_ptr<CommandBase> clone() const override;
};

} 
