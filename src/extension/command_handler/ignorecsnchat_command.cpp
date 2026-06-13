#include "ignorecsnchat_command.hpp"

#include "../../utils/packet_utils.hpp"
#include "../../server/server.hpp"

namespace command {

core::Core* IgnoreCSNChatCommand::s_core = nullptr;

IgnoreCSNChatCommand::IgnoreCSNChatCommand() : CommandBase(
    {"ignorecsnchat"},
    {},
    "Toggle auto-ignore for CSN/REME chat spam",
    0
) {}

std::unique_ptr<CommandBase> IgnoreCSNChatCommand::clone() const {
    return std::make_unique<IgnoreCSNChatCommand>(*this);
}

void IgnoreCSNChatCommand::set_core(core::Core* core) {
    s_core = core;
}

void IgnoreCSNChatCommand::execute(client::Client* client, const std::vector<std::string>& ) {
    if (!client || !client->get_player() || !s_core) {
        return;
    }

    player::Player* out_client = client->get_player();
    player::Player* out_server = nullptr;
    if (s_core->get_server() && s_core->get_server()->get_player()) {
        out_server = s_core->get_server()->get_player();
    }

    bool enabled = false;
    try {
        enabled = s_core->get_config().get<bool>("features.ignorecsnchat");
    } catch (...) {
        enabled = false;
    }

    enabled = !enabled;
    s_core->get_config().set<bool>("features.ignorecsnchat", enabled);

    const std::string status_msg = enabled
        ? "`2IgnoreCSNChat enabled`o - auto ignoring CSN/REME chat messages."
        : "`4IgnoreCSNChat disabled`o.";

    if (out_client) {
        utils::PacketUtils::send_chat_message(out_client, status_msg, false);
    }
    if (out_server && out_server != out_client) {
        utils::PacketUtils::send_chat_message(out_server, status_msg, false);
    }
}

} 

