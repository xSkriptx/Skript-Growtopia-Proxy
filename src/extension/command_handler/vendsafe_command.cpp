#include "vendsafe_command.hpp"
#include "../../client/client.hpp"
#include "../../server/server.hpp"
#include "../../player/player.hpp"
#include "../../utils/packet_utils.hpp"
#include <spdlog/spdlog.h>

namespace command {

core::Core* VendSafeCommand::s_core = nullptr;
bool VendSafeCommand::s_enabled = false;

VendSafeCommand::VendSafeCommand() : CommandBase(
    {"vendsafe", "vsafe"},
    {},
    "Toggle safe vending buy helper",
    0
) {}

std::unique_ptr<CommandBase> VendSafeCommand::clone() const {
    return std::make_unique<VendSafeCommand>(*this);
}

void VendSafeCommand::set_core(core::Core* core) {
    s_core = core;
}

bool VendSafeCommand::is_enabled() {
    return s_enabled;
}

void VendSafeCommand::set_enabled(bool enabled) {
    s_enabled = enabled;
}

void VendSafeCommand::execute(client::Client* client, const std::vector<std::string>& ) {
    if (!client || !client->get_player()) {
        spdlog::error("VendSafeCommand: No client/player");
        return;
    }

    s_enabled = !s_enabled;

    player::Player* out = client->get_player();
    if (s_core && s_core->get_server() && s_core->get_server()->get_player()) {
        out = s_core->get_server()->get_player();
    }

    if (out) {
        utils::PacketUtils::send_chat_message(
            out,
            s_enabled
                ? "`2VendSafe enabled:`o safe auto-buy is active.."
                : "`4VendSafe disabled..`o"
        );
    }

    spdlog::info("VendSafeCommand: {}", s_enabled ? "ENABLED" : "DISABLED");
}

} 

