#include "autosurg_command.hpp"
#include "../../client/client.hpp"
#include "../../player/player.hpp"
#include "../../server/server.hpp"
#include "../../utils/packet_utils.hpp"
#include <spdlog/spdlog.h>

namespace command {

core::Core* AutoSurgCommand::s_core = nullptr;
bool AutoSurgCommand::s_enabled = false;

AutoSurgCommand::AutoSurgCommand() : CommandBase(
    {"autosurg", "autos"},
    {},
    "Toggle auto-surgery mode (auto-selects correct tool)",
    0
) {}

std::unique_ptr<CommandBase> AutoSurgCommand::clone() const {
    return std::make_unique<AutoSurgCommand>(*this);
}

void AutoSurgCommand::set_core(core::Core* core) {
    s_core = core;
}

bool AutoSurgCommand::is_enabled() {
    return s_enabled;
}

void AutoSurgCommand::toggle() {
    s_enabled = !s_enabled;
    spdlog::info("AutoSurgCommand: {}", s_enabled ? "ENABLED" : "DISABLED");
}

void AutoSurgCommand::execute(client::Client* client, const std::vector<std::string>& ) {
    if (!client || !client->get_player()) {
        spdlog::error("AutoSurgCommand: No client or player!");
        return;
    }

    if (!s_core) {
        spdlog::error("AutoSurgCommand: No core set!");
        return;
    }

    toggle();

    
    player::Player* target_player = nullptr;
    if (auto* server = s_core->get_server(); server && server->get_player()) {
        target_player = server->get_player();
    } else {
        target_player = client->get_player();
    }

    if (target_player) {
        if (s_enabled) {
            utils::PacketUtils::send_chat_message(target_player,
                "`2Auto-surg enabled:`o will auto-select correct tool");
        } else {
            utils::PacketUtils::send_chat_message(target_player,
                "`4Auto-surg disabled.`o Normal behavior restored");
        }
    }
}

} 
