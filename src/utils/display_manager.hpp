#pragma once
#include "../core/core.hpp"
#include "../server/server.hpp"
#include "../player/player.hpp"
#include "../packet/packet_helper.hpp"
#include "../packet/packet_variant.hpp"
#include "../packet/packet_types.hpp"
#include "player_tracker.hpp"
#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace utils {

class DisplayManager {
public:
    
    static std::string build_display_name(core::Core* core, const std::string& base_name) {
        if (!core) return base_name;
        
        std::string display_name = base_name;
        
        
        std::string saved_visual_name = core->get_config().get<std::string>("display.visual_name");
        bool show_ping = core->get_config().get<bool>("display.show_ping");
        
        
        if (!saved_visual_name.empty()) {
            display_name = saved_visual_name;
        }
        
        
        bool has_dr = core->get_config().get<bool>("display.title.dr");
        if (has_dr) {
            display_name = "`9Dr.`` " + display_name;
        }
        
        
        
        if (show_ping) {
            auto* server = core->get_server();
            if (server && server->get_player() && server->get_player()->get_peer()) {
                int ping_ms = static_cast<int>(server->get_player()->get_peer()->roundTripTime);
                
                
                std::string ping_color = "`2"; 
                if (ping_ms > 150) {
                    ping_color = "`6"; 
                }
                if (ping_ms > 300) {
                    ping_color = "`4"; 
                }
                display_name = fmt::format("{} `0[{}{}``]", display_name, ping_color, ping_ms);
            }
        }
        
        return display_name;
    }
    
    
    static void apply_display_name(core::Core* core, uint32_t netID, const std::string& base_name) {
        if (!core) return;
        
        auto* server = core->get_server();
        if (!server || !server->get_player() || !server->get_player()->is_connected()) {
            spdlog::warn("DisplayManager: Server not available");
            return;
        }
        
        std::string display_name = build_display_name(core, base_name);
        
        try {
            
            packet::Variant variant{};
            variant.add("OnNameChanged");
            variant.add(display_name);
            
            std::vector<std::byte> ext_data = variant.serialize();
            
            packet::GameUpdatePacket game_packet{};
            game_packet.type = packet::PACKET_CALL_FUNCTION;
            game_packet.net_id = netID;
            game_packet.flags.extended = 1;
            game_packet.data_size = static_cast<uint32_t>(ext_data.size());
            
            ByteStream<std::uint16_t> byte_stream{};
            byte_stream.write(packet::NET_MESSAGE_GAME_PACKET);
            byte_stream.write(game_packet);
            byte_stream.write_data(ext_data.data(), ext_data.size());
            
            server->get_player()->send_packet(byte_stream.get_data(), 0);
            
            spdlog::debug("DisplayManager: Applied display name '{}' for netID {}", display_name, netID);
            
            
            apply_title_effects(core, netID);
            
        } catch (const std::exception& e) {
            spdlog::error("DisplayManager: Failed to apply display name: {}", e.what());
        }
    }
    
    
    static void apply_title_effects(core::Core* core, uint32_t netID) {
        if (!core) return;
        
        auto* server = core->get_server();
        if (!server || !server->get_player() || !server->get_player()->is_connected()) {
            return;
        }
        
        
        bool has_g4g = core->get_config().get<bool>("display.title.g4g");
        bool has_maxlevel = core->get_config().get<bool>("display.title.maxlevel");
        bool has_dr = core->get_config().get<bool>("display.title.dr");
        bool has_mentor = core->get_config().get<bool>("display.title.mentor");
        
        
        try {
            if (has_g4g) {
                send_title_packet(server, netID, "id", "donor");
                spdlog::debug("DisplayManager: Applied G4G title for netID {}", netID);
            }
            
            if (has_maxlevel) {
                send_title_packet(server, netID, "id", "maxLevel");
                spdlog::debug("DisplayManager: Applied MaxLevel title for netID {}", netID);
            }
            
            if (has_dr) {
                send_title_packet(server, netID, "id", "doctor");
                spdlog::debug("DisplayManager: Applied Dr title for netID {}", netID);
            }
            
            if (has_mentor) {
                send_title_packet(server, netID, "id", "master");
                spdlog::debug("DisplayManager: Applied Mentor title for netID {}", netID);
            }
            
        } catch (const std::exception& e) {
            spdlog::error("DisplayManager: Failed to apply title effects: {}", e.what());
        }
    }
    
    
    static void send_title_packet(server::Server* server, uint32_t netID, const std::string& key, const std::string& value) {
        packet::Variant var{};
        var.add("OnCountryState");
        var.add(key + "|" + value);
        
        std::vector<std::byte> ext_data = var.serialize();
        packet::GameUpdatePacket pkt{};
        pkt.type = packet::PACKET_CALL_FUNCTION;
        pkt.net_id = netID;
        pkt.flags.extended = 1;
        pkt.data_size = static_cast<uint32_t>(ext_data.size());
        
        ByteStream<std::uint16_t> bs{};
        bs.write(packet::NET_MESSAGE_GAME_PACKET);
        bs.write(pkt);
        bs.write_data(ext_data.data(), ext_data.size());
        
        server->get_player()->send_packet(bs.get_data(), 0);
    }
    
    
    static void update_display(core::Core* core) {
        if (!core) return;
        
        auto& player_tracker = PlayerTracker::get_instance();
        auto player_info = player_tracker.get_local_player();
        
        if (player_info.netID == 0) return;
        
        apply_display_name(core, player_info.netID, player_info.name);
    }
};

} 
