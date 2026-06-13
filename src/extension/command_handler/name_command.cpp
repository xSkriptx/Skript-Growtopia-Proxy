#include "name_command.hpp"
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

NameCommand::NameCommand() : CommandBase(
    {"name", "nick", "nickname"},
    {"new_name"},
    "Change your display name",
    1
) {}

std::unique_ptr<CommandBase> NameCommand::clone() const {
    return std::make_unique<NameCommand>(*this);
}

void NameCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    
    spdlog::warn("NameCommand::execute called without core - this shouldn't happen");
    if (client && client->get_player()) {
        utils::PacketUtils::send_chat_message(client->get_player(), "`4Error: Command requires core access.");
    }
}

void NameCommand::execute_with_core(client::Client* client, const std::vector<std::string>& args, core::Core* core) {
    if (args.empty()) {
        if (client && client->get_player()) {
            utils::PacketUtils::send_chat_message(client->get_player(), "`4Usage: /name <new_name>");
        }
        return;
    }

    
    std::string new_name;
    for (size_t i = 1; i < args.size(); ++i) {
        if (i > 1) new_name += " ";
        new_name += args[i];
    }

    
    if (new_name.empty() || new_name.length() > 20) {
        if (client && client->get_player()) {
            utils::PacketUtils::send_chat_message(client->get_player(), "`4Name must be between 1 and 20 characters");
        }
        return;
    }

    
    core->get_config().set("display.visual_name", new_name);
    
    
    auto& player_tracker = utils::PlayerTracker::get_instance();
    auto player_info = player_tracker.get_local_player();
    
    if (player_info.netID > 0) {
        utils::DisplayManager::apply_display_name(core, player_info.netID, player_info.name);
    }
    
    
    if (client && client->get_player()) {
        utils::PacketUtils::send_chat_message(client->get_player(), 
            fmt::format("`2Your name has been changed to: `5{} `o(saved for next login)", new_name));
    }
}

} 
