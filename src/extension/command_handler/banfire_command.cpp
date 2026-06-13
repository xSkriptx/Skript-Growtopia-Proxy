#include "banfire_command.hpp"
#include "../../client/client.hpp"
#include "../../player/player.hpp"
#include "../../server/server.hpp"
#include "../../utils/packet_utils.hpp"
#include <spdlog/spdlog.h>

namespace command {

core::Core* BanFireCommand::s_core = nullptr;
bool BanFireCommand::s_enabled = false;

BanFireCommand::BanFireCommand() : CommandBase(
    {"banfire", "bf"},
    {},
    "Toggle auto-ban fire mode (bans players who use pocket lighter)",
    0
) {}

std::unique_ptr<CommandBase> BanFireCommand::clone() const {
    return std::make_unique<BanFireCommand>(*this);
}

void BanFireCommand::set_core(core::Core* core) {
    s_core = core;
}

bool BanFireCommand::is_enabled() {
    return s_enabled;
}

void BanFireCommand::toggle() {
    s_enabled = !s_enabled;
    spdlog::info("BanFireCommand: {}", s_enabled ? "ENABLED" : "DISABLED");
}

void BanFireCommand::execute(client::Client* client, const std::vector<std::string>& ) {
    if (!client || !client->get_player()) {
        spdlog::error("BanFireCommand: No client or player!");
        return;
    }

    if (!s_core) {
        spdlog::error("BanFireCommand: No core set!");
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
                "`2BanFire enabled:`o will auto-ban players who use pocket lighter");
        } else {
            utils::PacketUtils::send_chat_message(target_player,
                "`4BanFire disabled.`o Normal behavior restored");
        }
    }
}

} 
