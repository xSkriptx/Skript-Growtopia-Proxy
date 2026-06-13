#include "warn_command.hpp"
#include "../../client/client.hpp"
#include "../../server/server.hpp"
#include "../../core/core.hpp"
#include "../../player/player.hpp"
#include "../../packet/packet_variant.hpp"
#include "../../packet/packet_types.hpp"
#include "../../utils/packet_utils.hpp"
#include "../../utils/player_tracker.hpp"
#include "../../utils/byte_stream.hpp"
#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace command {

WarnCommand::WarnCommand() : CommandBase(
    {"warn", "warning", "fakeban"},
    {},
    "Send a fake ban warning notification to yourself",
    0
) {}

std::unique_ptr<CommandBase> WarnCommand::clone() const {
    return std::make_unique<WarnCommand>();
}

void WarnCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    if (!client || !client->get_player()) {
        spdlog::error("WarnCommand: No client or player!");
        return;
    }

    spdlog::warn("WarnCommand::execute called without core - this shouldn't happen");
    utils::PacketUtils::send_chat_message(client->get_player(), "`4Error: Command requires core access.");
}

void WarnCommand::execute_with_core(client::Client* client, const std::vector<std::string>& args, core::Core* core) {
    if (!client || !client->get_player()) {
        spdlog::error("WarnCommand: No client or player!");
        return;
    }

    auto* server = core->get_server();
    if (!server || !server->get_player()) {
        spdlog::error("WarnCommand: No server available!");
        utils::PacketUtils::send_chat_message(client->get_player(), "`4Error: Server not available.");
        return;
    }

    spdlog::info("WarnCommand::execute_with_core called - sending fake ban warning via server");
    
    try {
        auto& player_tracker = utils::PlayerTracker::get_instance();
        auto player_info = player_tracker.get_local_player();
        
        if (player_info.netID == 0) {
            utils::PacketUtils::send_chat_message(client->get_player(), "`4Error: Try again after spawning.");
            spdlog::warn("Warning command failed: No player information available");
            return;
        }

        spdlog::info("Sending warning messages to netid: {} (Name: {})", player_info.netID, player_info.name);

        
        {
            packet::Variant var{};
            var.add("OnConsoleMessage");
            var.add("Reality flickers as you begin to wake up. (`$Ban mod added, `$730 days left)");
            
            std::vector<std::byte> ext_data = var.serialize();
            packet::GameUpdatePacket pkt{};
            pkt.type = packet::PACKET_CALL_FUNCTION;
            pkt.net_id = -1;  
            pkt.flags.extended = 1;
            pkt.data_size = static_cast<uint32_t>(ext_data.size());

            ByteStream<std::uint16_t> bs{};
            bs.write(packet::NET_MESSAGE_GAME_PACKET);
            bs.write(pkt);
            bs.write_data(ext_data.data(), ext_data.size());
            
            server->get_player()->send_packet(bs.get_data(), 0);
            spdlog::debug("Sent console message 1");
        }

        
        {
            packet::Variant var{};
            var.add("OnAddNotification");
            var.add("interface/atomic_button.rttex");
            var.add("Warning from `4System: You've been `4BANNED from `wGrowtopia for 730 days");
            var.add("audio/hub_open.wav");
            var.add(0);  
            
            std::vector<std::byte> ext_data = var.serialize();
            packet::GameUpdatePacket pkt{};
            pkt.type = packet::PACKET_CALL_FUNCTION;
            pkt.net_id = -1;
            pkt.flags.extended = 1;
            pkt.data_size = static_cast<uint32_t>(ext_data.size());

            ByteStream<std::uint16_t> bs{};
            bs.write(packet::NET_MESSAGE_GAME_PACKET);
            bs.write(pkt);
            bs.write_data(ext_data.data(), ext_data.size());
            
            server->get_player()->send_packet(bs.get_data(), 0);
            spdlog::debug("Sent notification popup");
        }

        
        {
            packet::Variant var{};
            var.add("OnConsoleMessage");
            var.add("Warning from `4System: You've been `4BANNED from `wGrowtopia for 730 days");
            
            std::vector<std::byte> ext_data = var.serialize();
            packet::GameUpdatePacket pkt{};
            pkt.type = packet::PACKET_CALL_FUNCTION;
            pkt.net_id = -1;
            pkt.flags.extended = 1;
            pkt.data_size = static_cast<uint32_t>(ext_data.size());

            ByteStream<std::uint16_t> bs{};
            bs.write(packet::NET_MESSAGE_GAME_PACKET);
            bs.write(pkt);
            bs.write_data(ext_data.data(), ext_data.size());
            
            server->get_player()->send_packet(bs.get_data(), 0);
            spdlog::debug("Sent console message 2");
        }

        
        {
            std::string player_name = player_info.name.empty() ? "Unknown" : player_info.name;
            std::string final_msg = fmt::format("`4** {} AUTO-BANNED BY SYSTEM ** `$(/rules to view rules)", player_name);
            
            packet::Variant var{};
            var.add("OnConsoleMessage");
            var.add(final_msg);
            
            std::vector<std::byte> ext_data = var.serialize();
            packet::GameUpdatePacket pkt{};
            pkt.type = packet::PACKET_CALL_FUNCTION;
            pkt.net_id = -1;
            pkt.flags.extended = 1;
            pkt.data_size = static_cast<uint32_t>(ext_data.size());

            ByteStream<std::uint16_t> bs{};
            bs.write(packet::NET_MESSAGE_GAME_PACKET);
            bs.write(pkt);
            bs.write_data(ext_data.data(), ext_data.size());
            
            server->get_player()->send_packet(bs.get_data(), 0);
            spdlog::debug("Sent console message 3");
        }

        utils::PacketUtils::send_chat_message(client->get_player(), "`2Warning notification sent!");
        spdlog::info("Warning command executed successfully");
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to send warning: {}", e.what());
        utils::PacketUtils::send_chat_message(client->get_player(), "`4Error: Failed to send warning");
    }
}

} 
