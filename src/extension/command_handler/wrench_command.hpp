#pragma once

#include "command_base.hpp"
#include "../../core/core.hpp"
#include "../../utils/text_parse.hpp"

namespace command {

class WrenchCommand : public CommandBase {
public:
    WrenchCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;

    static void set_core(core::Core* core);
    static void apply_dialog_settings(const TextParse& tp);

private:
    static core::Core* s_core;
    static void send_settings_dialog(player::Player* out);
};

} 

