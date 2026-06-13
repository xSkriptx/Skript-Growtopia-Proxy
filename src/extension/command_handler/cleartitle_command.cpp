#include "cleartitle_command.hpp"
#include "../../client/client.hpp"
#include "../../server/server.hpp"
#include "../../core/core.hpp"
#include "../../player/player.hpp"
#include "../../packet/packet_variant.hpp"
#include "../../packet/packet_types.hpp"
#include "../../utils/packet_utils.hpp"
#include "../../utils/player_tracker.hpp"
#include "../../utils/display_manager.hpp"
#include <spdlog/spdlog.h>

namespace command {

ClearTitleCommand::ClearTitleCommand() : CommandBase(
    {"cleartitle", "ct", "resettitle"},
    {},
    "Remove all titles and reset name",
    0
) {}

std::unique_ptr<CommandBase> ClearTitleCommand::clone() const {
    return std::make_unique<ClearTitleCommand>();
}

void ClearTitleCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    spdlog::warn("ClearTitleCommand::execute called without core - this shouldn't happen");
    if (client && client->get_player()) {
        utils::PacketUtils::send_chat_message(client->get_player(), "`4Error: Command requires core access.");
    }
}

void ClearTitleCommand::execute_with_core(client::Client* client, const std::vector<std::string>& args, core::Core* core) {
    if (!client || !client->get_player()) {
        spdlog::error("ClearTitleCommand: No client or player!");
        return;
    }

    auto* server = core->get_server();
    if (!server || !server->get_player()) {
        spdlog::error("ClearTitleCommand: No server available!");
        utils::PacketUtils::send_chat_message(client->get_player(), "`4Error: Server not available.");
        return;
    }

    auto& player_tracker = utils::PlayerTracker::get_instance();
    auto player_info = player_tracker.get_local_player();
    
    if (player_info.netID == 0) {
        utils::PacketUtils::send_chat_message(client->get_player(), "`4Error: Try again after spawning.");
        return;
    }

    spdlog::info("Clearing all titles for netid: {} (Name: {})", player_info.netID, player_info.name);

    
    core->get_config().set("display.title.g4g", false);
    core->get_config().set("display.title.maxlevel", false);
    core->get_config().set("display.title.dr", false);
    core->get_config().set("display.title.mentor", false);
    core->get_config().set("display.visual_name", "");
    
    
    utils::DisplayManager::apply_display_name(core, player_info.netID, player_info.name);

    utils::PacketUtils::send_chat_message(client->get_player(), "`2All titles cleared!");
    spdlog::info("Cleared all titles successfully via DisplayManager");
}

} 
