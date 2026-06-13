#include "trashfast_command.hpp"
#include "../../client/client.hpp"
#include "../../player/player.hpp"
#include "../../server/server.hpp"
#include "../../utils/packet_utils.hpp"
#include <spdlog/spdlog.h>

namespace command {

core::Core* TrashFastCommand::s_core = nullptr;
bool TrashFastCommand::s_enabled = false;

TrashFastCommand::TrashFastCommand() : CommandBase(
    {"trashfast", "tf"},
    {},
    "Toggle fast trash mode (auto-confirms trash dialogs)",
    0
) {}

std::unique_ptr<CommandBase> TrashFastCommand::clone() const {
    return std::make_unique<TrashFastCommand>(*this);
}

void TrashFastCommand::set_core(core::Core* core) {
    s_core = core;
}

bool TrashFastCommand::is_enabled() {
    return s_enabled;
}

void TrashFastCommand::toggle() {
    s_enabled = !s_enabled;
    spdlog::info("TrashFastCommand: {}", s_enabled ? "ENABLED" : "DISABLED");
}

void TrashFastCommand::execute(client::Client* client, const std::vector<std::string>& ) {
    if (!client || !client->get_player()) {
        spdlog::error("TrashFastCommand: No client or player!");
        return;
    }

    if (!s_core) {
        spdlog::error("TrashFastCommand: No core set!");
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
                "`2TrashFast enabled:`o will auto-confirm trash dialogs");
        } else {
            utils::PacketUtils::send_chat_message(target_player,
                "`4TrashFast disabled.`o Normal trash behavior restored");
        }
    }
}

} 
