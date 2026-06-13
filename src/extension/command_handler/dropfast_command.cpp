#include "dropfast_command.hpp"
#include "../../client/client.hpp"
#include "../../player/player.hpp"
#include "../../server/server.hpp"
#include "../../utils/packet_utils.hpp"
#include <spdlog/spdlog.h>

namespace command {

core::Core* DropFastCommand::s_core = nullptr;
bool DropFastCommand::s_enabled = false;

DropFastCommand::DropFastCommand() : CommandBase(
    {"dropfast", "df"},
    {},
    "Toggle fast drop mode (auto-confirms drop dialogs)",
    0
) {}

std::unique_ptr<CommandBase> DropFastCommand::clone() const {
    return std::make_unique<DropFastCommand>(*this);
}

void DropFastCommand::set_core(core::Core* core) {
    s_core = core;
}

bool DropFastCommand::is_enabled() {
    return s_enabled;
}

void DropFastCommand::toggle() {
    s_enabled = !s_enabled;
    spdlog::info("DropFastCommand: {}", s_enabled ? "ENABLED" : "DISABLED");
}

void DropFastCommand::execute(client::Client* client, const std::vector<std::string>& ) {
    if (!client || !client->get_player()) {
        spdlog::error("DropFastCommand: No client or player!");
        return;
    }

    if (!s_core) {
        spdlog::error("DropFastCommand: No core set!");
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
                "`2DropFast enabled:`o will auto-confirm drop dialogs");
        } else {
            utils::PacketUtils::send_chat_message(target_player,
                "`4DropFast disabled.`o Normal drop behavior restored");
        }
    }
}

} 

