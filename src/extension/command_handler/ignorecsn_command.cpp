#include "ignorecsn_command.hpp"

#include "../../utils/packet_utils.hpp"
#include "../../server/server.hpp"

namespace command {

core::Core* IgnoreCSNCommand::s_core = nullptr;

IgnoreCSNCommand::IgnoreCSNCommand() : CommandBase(
    {"ignorecsn"},
    {},
    "Toggle auto-ignore for CSN/REME seller spam",
    0
) {}

std::unique_ptr<CommandBase> IgnoreCSNCommand::clone() const {
    return std::make_unique<IgnoreCSNCommand>(*this);
}

void IgnoreCSNCommand::set_core(core::Core* core) {
    s_core = core;
}

void IgnoreCSNCommand::execute(client::Client* client, const std::vector<std::string>& ) {
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
        enabled = s_core->get_config().get<bool>("features.ignorecsn");
    } catch (...) {
        enabled = false;
    }

    enabled = !enabled;
    s_core->get_config().set<bool>("features.ignorecsn", enabled);

    const std::string status_msg = enabled
        ? "`2IgnoreCSN enabled`o - auto ignoring CSN/REME messages."
        : "`4IgnoreCSN disabled`o.";

    if (out_client) {
        utils::PacketUtils::send_chat_message(out_client, status_msg, false);
    }
    if (out_server && out_server != out_client) {
        utils::PacketUtils::send_chat_message(out_server, status_msg, false);
    }
}

} 

