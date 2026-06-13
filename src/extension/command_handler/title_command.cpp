#include "title_command.hpp"
#include "../../client/client.hpp"
#include "../../player/player.hpp"
#include "../../server/server.hpp"
#include "../../core/core.hpp"
#include "../../packet/packet_helper.hpp"
#include "../../packet/packet_variant.hpp"
#include "../../packet/packet_types.hpp"
#include "../../utils/packet_utils.hpp"
#include "../../utils/player_tracker.hpp"
#include "../../utils/display_manager.hpp"
#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace command {

TitleCommand::TitleCommand() : CommandBase(
    {"title", "titles", "tag"},
    {},
    "Open title selection GUI",
    0
) {}

std::unique_ptr<CommandBase> TitleCommand::clone() const {
    return std::make_unique<TitleCommand>();
}

void TitleCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    spdlog::warn("TitleCommand::execute called without core - this shouldn't happen");
    if (client && client->get_player()) {
        utils::PacketUtils::send_chat_message(client->get_player(), "`4Error: Command requires core access.");
    }
}

void TitleCommand::execute_with_core(client::Client* client, const std::vector<std::string>& args, core::Core* core) {
    spdlog::info("TitleCommand::execute_with_core called - opening title GUI");
    send_title_gui(client, core);
    spdlog::info("Title command executed - GUI should be displayed");
}

void TitleCommand::send_title_gui(client::Client* client, core::Core* core) {
    if (!client || !client->get_player()) {
        spdlog::error("send_title_gui: client or player is null!");
        return;
    }

    auto* server = core->get_server();
    if (!server || !server->get_player()) {
        spdlog::error("send_title_gui: No server available!");
        return;
    }

    spdlog::info("Building title GUI dialog...");

    try {
        std::string title_dialog = 
            "set_default_color|`o\n"
            "add_label_with_icon|big|`wTitle Manager``|left|5016|\n"
            "add_spacer|small|\n"
            "add_textbox|`oSelect titles below. Titles can be combined!``|left|\n"
            "add_spacer|small|\n"
            
            
            "add_label_with_icon|small|`2G4G Title``|left|11304|\n"
            "add_smalltext|`oShows the green [G4G] badge next to your name``|\n"
            "add_button|g4g|`2Toggle G4G``|\n"
            "add_spacer|small|\n"
            
            "add_label_with_icon|small|`5Max Level Title``|left|11302|\n"
            "add_smalltext|`oShows the purple [125] level badge``|\n"
            "add_button|maxlv|`5Toggle Max Level``|\n"
            "add_spacer|small|\n"
            
            "add_label_with_icon|small|`9Dr. Title``|left|11300|\n"
            "add_smalltext|`oAdds 'Dr.' prefix to your name with doctor badge``|\n"
            "add_button|dr|`9Toggle Dr.``|\n"
            "add_spacer|small|\n"
            
            "add_label_with_icon|small|`6Mentor Title``|left|11298|\n"
            "add_smalltext|`oShows mentor badge next to your name``|\n"
            "add_button|mentor|`6Toggle Mentor``|\n"
            "add_spacer|small|\n"
            
            
            "add_label_with_icon|small|`4Clear All``|left|758|\n"
            "add_smalltext|`oRemove all titles and reset to original name``|\n"
            "add_button|cleartitle|`4Clear Titles``|\n"
            "add_spacer|small|\n"
            
            "add_textbox|`w💡 Tip: Toggle each title to combine them all!``|left|\n"
            "add_quick_exit|\n"
            "end_dialog|title_gui|Close||";

        packet::Variant variant{};
        variant.add("OnDialogRequest");
        variant.add(title_dialog);
        
        std::vector<std::byte> ext_data = variant.serialize();

        packet::GameUpdatePacket game_packet{};
        game_packet.type = packet::PACKET_CALL_FUNCTION;
        game_packet.net_id = -1;
        game_packet.flags.extended = 1;
        game_packet.data_size = static_cast<uint32_t>(ext_data.size());

        ByteStream<std::uint16_t> byte_stream{};
        byte_stream.write(packet::NET_MESSAGE_GAME_PACKET);
        byte_stream.write(game_packet);
        byte_stream.write_data(ext_data.data(), ext_data.size());

        
        server->get_player()->send_packet(byte_stream.get_data(), 0);
        
        spdlog::info("Title GUI sent to client successfully");

    } catch (const std::exception& e) {
        spdlog::error("Failed to send title GUI: {}", e.what());
    }
}


G4GCommand::G4GCommand() : CommandBase(
    {"g4g"},
    {},
    "Apply G4G title",
    0
) {}

std::unique_ptr<CommandBase> G4GCommand::clone() const {
    return std::make_unique<G4GCommand>();
}

void G4GCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    
    spdlog::warn("G4GCommand::execute called without core - this shouldn't happen");
    if (client && client->get_player()) {
        utils::PacketUtils::send_chat_message(client->get_player(), "`4Error: Command requires core access.");
    }
}


MaxLevelCommand::MaxLevelCommand() : CommandBase(
    {"maxlevel", "maxlv", "ml"},
    {},
    "Apply Max Level title",
    0
) {}

std::unique_ptr<CommandBase> MaxLevelCommand::clone() const {
    return std::make_unique<MaxLevelCommand>();
}

void MaxLevelCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    
    spdlog::warn("MaxLevelCommand::execute called without core - this shouldn't happen");
    if (client && client->get_player()) {
        utils::PacketUtils::send_chat_message(client->get_player(), "`4Error: Command requires core access.");
    }
}


DrCommand::DrCommand() : CommandBase(
    {"dr", "doctor"},
    {},
    "Apply Dr. title",
    0
) {}

std::unique_ptr<CommandBase> DrCommand::clone() const {
    return std::make_unique<DrCommand>();
}

void DrCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    
    spdlog::warn("DrCommand::execute called without core - this shouldn't happen");
    if (client && client->get_player()) {
        utils::PacketUtils::send_chat_message(client->get_player(), "`4Error: Command requires core access.");
    }
}





void G4GCommand::execute_with_core(client::Client* client, const std::vector<std::string>& args, core::Core* core) {
    if (!client || !client->get_player()) {
        spdlog::error("G4GCommand: No client or player!");
        return;
    }

    auto* server = core->get_server();
    if (!server || !server->get_player()) {
        spdlog::error("G4GCommand: No server available!");
        utils::PacketUtils::send_chat_message(client->get_player(), "`4Error: Server not available.");
        return;
    }

    auto& player_tracker = utils::PlayerTracker::get_instance();
    auto player_info = player_tracker.get_local_player();
    
    if (player_info.netID == 0) {
        utils::PacketUtils::send_chat_message(client->get_player(), "`4Error: Try again after spawning.");
        return;
    }

    spdlog::info("Applying G4G title to netid: {} (Name: {})", player_info.netID, player_info.name);
    
    
    bool current = core->get_config().get<bool>("display.title.g4g");
    core->get_config().set("display.title.g4g", !current);

    
    utils::DisplayManager::apply_display_name(core, player_info.netID, player_info.name);

    if (!current) {
        utils::PacketUtils::send_chat_message(client->get_player(), "`2G4G title enabled!");
    } else {
        utils::PacketUtils::send_chat_message(client->get_player(), "`4G4G title disabled.");
    }
    spdlog::info("Toggled G4G title via DisplayManager");
}

void MaxLevelCommand::execute_with_core(client::Client* client, const std::vector<std::string>& args, core::Core* core) {
    if (!client || !client->get_player()) {
        spdlog::error("MaxLevelCommand: No client or player!");
        return;
    }

    auto* server = core->get_server();
    if (!server || !server->get_player()) {
        spdlog::error("MaxLevelCommand: No server available!");
        utils::PacketUtils::send_chat_message(client->get_player(), "`4Error: Server not available.");
        return;
    }

    auto& player_tracker = utils::PlayerTracker::get_instance();
    auto player_info = player_tracker.get_local_player();
    
    if (player_info.netID == 0) {
        utils::PacketUtils::send_chat_message(client->get_player(), "`4Error: Try again after spawning.");
        return;
    }

    spdlog::info("Applying Max Level title to netid: {} (Name: {})", player_info.netID, player_info.name);
    
    
    bool current = core->get_config().get<bool>("display.title.maxlevel");
    core->get_config().set("display.title.maxlevel", !current);
    
    
    utils::DisplayManager::apply_display_name(core, player_info.netID, player_info.name);

    if (!current) {
        utils::PacketUtils::send_chat_message(client->get_player(), "`5Max Level title enabled!");
    } else {
        utils::PacketUtils::send_chat_message(client->get_player(), "`4Max Level title disabled.");
    }
    spdlog::info("Toggled Max Level title via DisplayManager");
}

void DrCommand::execute_with_core(client::Client* client, const std::vector<std::string>& args, core::Core* core) {
    if (!client || !client->get_player()) {
        spdlog::error("DrCommand: No client or player!");
        return;
    }

    auto* server = core->get_server();
    if (!server || !server->get_player()) {
        spdlog::error("DrCommand: No server available!");
        utils::PacketUtils::send_chat_message(client->get_player(), "`4Error: Server not available.");
        return;
    }

    auto& player_tracker = utils::PlayerTracker::get_instance();
    auto player_info = player_tracker.get_local_player();
    
    if (player_info.netID == 0) {
        utils::PacketUtils::send_chat_message(client->get_player(), "`4Error: Try again after spawning.");
        return;
    }

    spdlog::info("Applying Dr. title to netid: {} (Name: {})", player_info.netID, player_info.name);
    
    
    bool current = core->get_config().get<bool>("display.title.dr");
    core->get_config().set("display.title.dr", !current);
    
    
    utils::DisplayManager::apply_display_name(core, player_info.netID, player_info.name);

    if (!current) {
        utils::PacketUtils::send_chat_message(client->get_player(), "`9Dr. title enabled!");
    } else {
        utils::PacketUtils::send_chat_message(client->get_player(), "`4Dr. title disabled.");
    }
    spdlog::info("Toggled Dr. title via DisplayManager");
}

} 