#pragma once
#include "command_base.hpp"

namespace command {

class EventMenuCommand : public CommandBase {
public:
    EventMenuCommand();
    
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    
    std::unique_ptr<CommandBase> clone() const override;
};

} 
