#pragma once
#include "command_base.hpp"

namespace command {

class InventoryCommand : public CommandBase {
public:
    InventoryCommand();
    ~InventoryCommand() override = default;

    std::unique_ptr<CommandBase> clone() const override;
    void execute(client::Client* client, const std::vector<std::string>& args) override;
};

} 
