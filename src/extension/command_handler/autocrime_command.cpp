#include "autocrime_command.hpp"
#include "../../client/client.hpp"
#include "../../player/player.hpp"
#include "../../server/server.hpp"
#include "../../utils/packet_utils.hpp"
#include <spdlog/spdlog.h>

namespace command {

core::Core* AutoCrimeCommand::s_core = nullptr;
bool AutoCrimeCommand::s_enabled = false;

AutoCrimeCommand::AutoCrimeCommand() : CommandBase(
    {"autocrime"},
    {},
    "Toggle auto-crime mode",
    0
) {}

std::unique_ptr<CommandBase> AutoCrimeCommand::clone() const {
    return std::make_unique<AutoCrimeCommand>(*this);
}

void AutoCrimeCommand::set_core(core::Core* core) {
    s_core = core;
}

bool AutoCrimeCommand::is_enabled() {
    return s_enabled;
}

void AutoCrimeCommand::toggle() {
    s_enabled = !s_enabled;
    spdlog::info("AutoCrimeCommand: {}", s_enabled ? "ENABLED" : "DISABLED");
}

void AutoCrimeCommand::execute(client::Client* client, const std::vector<std::string>& ) {
    if (!client || !client->get_player()) {
        spdlog::error("AutoCrimeCommand: No client or player!");
        return;
    }

    if (!s_core) {
        spdlog::error("AutoCrimeCommand: No core set!");
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
                "`2Auto-crime enabled:`o will auto-play crime battles");
        } else {
            utils::PacketUtils::send_chat_message(target_player,
                "`4Auto-crime disabled.`o Normal behavior restored");
        }
    }
}

} 
