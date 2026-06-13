#pragma once
#include "command_base.hpp"
#include "../../utils/text_parse.hpp"
#include "../../packet/message/chat.hpp"

namespace command {

class DoorCommand : public CommandBase {
public:
    DoorCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;

private:
    static void send_door_packet(client::Client* client, const std::string& world_name, const std::string& door_id);
};

} 
