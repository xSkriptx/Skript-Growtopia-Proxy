#pragma once
#include "command_base.hpp"
#include "../../utils/text_parse.hpp"
#include "../../packet/message/chat.hpp"

namespace command {

class GuiCommand : public CommandBase {
public:
    GuiCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;

private:
    static void send_gui_interface(player::Player* player);
};

}