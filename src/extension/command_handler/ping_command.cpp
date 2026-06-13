#include "ping_command.hpp"
#include "../../client/client.hpp"
#include "../../server/server.hpp"
#include "../../core/core.hpp"
#include "../../player/player.hpp"
#include "../../packet/packet_helper.hpp"
#include "../../packet/packet_variant.hpp"
#include "../../packet/packet_types.hpp"
#include "../../utils/packet_utils.hpp"
#include "../../utils/player_tracker.hpp"
#include "../../utils/display_manager.hpp"
#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace command {

PingCommand::PingCommand() : CommandBase(
    {"ping"},
    {},
    "Toggle ping display next to your name",
    0
) {}

std::unique_ptr<CommandBase> PingCommand::clone() const {
    return std::make_unique<PingCommand>(*this);
}

void PingCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    spdlog::warn("PingCommand::execute called without core - this shouldn't happen");
    if (client && client->get_player()) {
        utils::PacketUtils::send_chat_message(client->get_player(), "`4Error: Command requires core access.");
    }
}

void PingCommand::execute_with_core(client::Client* client, const std::vector<std::string>& args, core::Core* core) {
    if (!client || !client->get_player()) return;

    
    bool current_state = core->get_config().get<bool>("display.show_ping");
    
    
    bool new_state = !current_state;
    core->get_config().set("display.show_ping", new_state);
    
    
    auto& player_tracker = utils::PlayerTracker::get_instance();
    auto player_info = player_tracker.get_local_player();
    
    if (player_info.netID > 0) {
        utils::DisplayManager::apply_display_name(core, player_info.netID, player_info.name);
    }
    
    
    if (new_state) {
        utils::PacketUtils::send_chat_message(client->get_player(), 
            "`2Ping display enabled! Your ping will appear next to your name.");
    } else {
        utils::PacketUtils::send_chat_message(client->get_player(), 
            "`4Ping display disabled.");
    }
}

} 
