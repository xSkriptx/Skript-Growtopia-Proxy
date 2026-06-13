#pragma once
#include "extension.hpp"
#include "../utils/world_logger.hpp"
#include "../utils/world_manager.hpp"
#include "../utils/world_parser_v2.h"
#include "../packet/filter.hpp"
#include "../packet/packet_types.hpp"
#include "../packet/tank_packet.hpp"
#include "../utils/byte_stream.hpp"
#include "../packet/packet_variant.hpp"
#include "../utils/hash.hpp"
#include "../utils/text_parse.hpp"
#include "command_handler/admin_command.hpp"
#include "command_handler/dat_command.hpp"
#include "command_handler/find_command.hpp"
#include "command_handler/vendloc_command.hpp"
#include <fstream>
#include <chrono>

namespace extension::world_logger {

class WorldLoggerExtension final : public IExtension {
    core::Core* core_;
    utils::WorldLogger& logger_;
    
    
    struct PlayerWorldMapping {
        uint32_t player_id;
        char world_name[64];
    };
    PlayerWorldMapping player_world_map_[100];
    size_t player_world_count_;
    
public:
    PROVIDE_EXT_UID(0x574F524C); 

    explicit WorldLoggerExtension(core::Core* core) 
        : core_{ core }
        , logger_{ utils::WorldLogger::get_instance() }
        , player_world_count_{ 0 }
    {
    }

    ~WorldLoggerExtension() override = default;

    void init() override {
        
        core_->get_event_dispatcher().prependListener(
            core::EventType::Packet,
            [this](const core::EventPacket& event) {
                if (event.from == core::EventFrom::FromServer) {
                    handle_server_packet(event);
                }
            }
        );

        core_->get_event_dispatcher().appendListener(
            core::EventType::Connection,
            [this](const core::EventConnection& event) {
                if (event.from == core::EventFrom::FromClient) {
                    uint32_t player_id = event.get_player().get_peer()->connectID;
                    logger_.log_world_entry(player_id, 0, "Connecting");
                }
            }
        );

        core_->get_event_dispatcher().appendListener(
            core::EventType::Disconnection,
            [this](const core::EventDisconnection& event) {
                uint32_t player_id = event.get_player().get_peer()->connectID;
                remove_player_world_mapping(player_id);
                logger_.log_world_exit(player_id, 0);
            }
        );

        spdlog::trace("World Logger extension loaded - GrowScan ready");
    }

    void free() override {
        logger_.generate_daily_report();
        delete this;
    }

private:
    
    void send_text_overlay(const std::string& text) {
        if (!core_) return;
        auto* server = core_->get_server();
        if (!server || !server->get_player()) return;

        packet::Variant var{};
        var.add("OnTextOverlay");
        var.add(text);

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
        spdlog::info("Sent OnTextOverlay: {}", text);
    }

    void handle_server_packet(const core::EventPacket& event) {
        const auto& game_packet = event.get_packet();
        
        
        static auto last_world_load = std::chrono::steady_clock::now();
        static bool logging_enabled = false;
        
        if (game_packet.type == packet::PACKET_SEND_MAP_DATA) {
            last_world_load = std::chrono::steady_clock::now();
            logging_enabled = true;
        }
        
        if (logging_enabled) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - last_world_load
            ).count();
            
            if (elapsed < 60) {  
                spdlog::info("[PACKET-LOG] Type={} Size={} (+{}s after world load)", 
                            (int)game_packet.type, event.get_ext_data().size(), (int)elapsed);
            } else {
                logging_enabled = false;
            }
        }
        
        
        if (game_packet.type == packet::PACKET_ITEM_CHANGE_OBJECT) {
            spdlog::info("[ITEM_CHANGE_OBJECT DETECTED] Size={} bytes", event.get_ext_data().size());
        }
        
        switch (game_packet.type) {
            case packet::PACKET_SEND_MAP_DATA:
                handle_map_data(event);
                break;
            case packet::PACKET_CALL_FUNCTION:
                handle_function_call(event);
                break;
            
            default:
                break;
        }
    }

    void handle_map_data(const core::EventPacket& event) {
        spdlog::info("=== SEND_MAP_DATA RECEIVED ===");
        try {
            const auto& game_packet = event.get_packet();
            const auto& ext_data = event.get_ext_data();
            
            spdlog::info("Data size: {} bytes", ext_data.size());
            spdlog::info("Decompressed size field: {} bytes", game_packet.decompressed_data_size);
            spdlog::info("Packet flags.extended: {}", game_packet.flags.extended);
            
            if (!ext_data.empty()) {
                
                static world_v2::World g_parsed_world;
                
                if (g_parsed_world.parse((const uint8_t*)ext_data.data(), ext_data.size())) {
                    
                    if (g_parsed_world.had_warnings) {
                        send_text_overlay("`4World parse warning; GrowScan may be unreliable");
                    }

                    uint32_t player_id = event.get_player().get_peer()->connectID;
                    
                    
                    bool chest_enabled = core_->get_config().get<bool>("command.chest_enabled");
                    if (chest_enabled) {
                        int replaced = 0;
                        for (auto& tile : g_parsed_world.tiles) {
                            if (tile.fg == 596) {  
                                tile.fg = 598;      
                                replaced++;
                            }
                        }
                        if (replaced > 0) {
                            spdlog::info("ChestCommand: Replaced {} chests with dragon gates in world data", replaced);
                        }
                    }
                    
                    spdlog::info("✓ Parsed: {} ({}x{}, {} tiles, {} dropped items)", 
                                g_parsed_world.name, g_parsed_world.width, g_parsed_world.height, 
                                g_parsed_world.tiles.size(), g_parsed_world.dropped_items.size());
                    
                    
                    utils::WorldManager::get_instance().set_current_world_v2(g_parsed_world);
                    
                    
                    command::AdminCommand::set_current_world(&g_parsed_world);
                    
                    
                    command::DatCommand::set_current_world(&g_parsed_world);
                    
                    
                    command::FindCommand::save_world_vendings(g_parsed_world);
                    command::VendLocCommand::save_world_vendings(g_parsed_world);
                    
                    
                    utils::WorldManager::get_instance().clear_live_objects();
                    
                    update_player_world_mapping(player_id, g_parsed_world.name.c_str());
                    logger_.log_world_entry(player_id, 0, g_parsed_world.name.c_str());
                } else {
                    
                    send_text_overlay("`4World parse failed; GrowScan may be unreliable");
                    spdlog::error("✗ Parse failed: {}", g_parsed_world.error_message);
                }
            }
        } catch (const std::exception& e) {
            spdlog::error("Exception: {}", e.what());
        }
    }

    void handle_item_change_object(const core::EventPacket& event) {
        
        
    }

    void handle_function_call(const core::EventPacket& event) {
        try {
            const auto& ext_data = event.get_ext_data();
            if (ext_data.empty()) return;
            
            
            packet::Variant variant{};
            if (!variant.deserialize(ext_data)) return;
            
            auto params = variant.get_variants();
            if (params.empty()) return;
            
            
            std::string function_name;
            try {
                function_name = std::get<std::string>(params[0]);
            } catch (...) {
                return;
            }
            
            
            if (function_name == "OnConsoleMessage" && params.size() >= 2) {
                try {
                    std::string message = std::get<std::string>(params[1]);
                    
                    
                    if (message.find("Collected") != std::string::npos) {
                        spdlog::info("[DROP DETECTED] {}", message);
                        parse_collected_message(message);
                    }
                } catch (...) {
                    
                }
            }
        } catch (const std::exception& e) {
            
        }
    }
    
    void parse_collected_message(const std::string& message) {
        
        
        
        size_t collected_pos = message.find("Collected `w");
        if (collected_pos == std::string::npos) return;
        
        size_t start = collected_pos + 12; 
        size_t end = message.find("``", start);
        if (end == std::string::npos) return;
        
        std::string item_text = message.substr(start, end - start);
        
        
        size_t first_space = item_text.find(' ');
        if (first_space == std::string::npos) return;
        
        std::string count_str = item_text.substr(0, first_space);
        std::string item_name = item_text.substr(first_space + 1);
        
        int count = 0;
        try {
            count = std::stoi(count_str);
        } catch (...) {
            return;
        }
        
        spdlog::info("Parsed drop: {} x '{}'", count, item_name);
        
        
        
        
    }

    void update_player_world_mapping(uint32_t player_id, const char* world_name) {
        for (size_t i = 0; i < player_world_count_; ++i) {
            if (player_world_map_[i].player_id == player_id) {
                strncpy(player_world_map_[i].world_name, world_name, 63);
                player_world_map_[i].world_name[63] = '\0';
                return;
            }
        }
        
        if (player_world_count_ < 100) {
            player_world_map_[player_world_count_].player_id = player_id;
            strncpy(player_world_map_[player_world_count_].world_name, world_name, 63);
            player_world_map_[player_world_count_].world_name[63] = '\0';
            player_world_count_++;
        }
    }

    void remove_player_world_mapping(uint32_t player_id) {
        for (size_t i = 0; i < player_world_count_; ++i) {
            if (player_world_map_[i].player_id == player_id) {
                for (size_t j = i; j < player_world_count_ - 1; ++j) {
                    player_world_map_[j] = player_world_map_[j + 1];
                }
                player_world_count_--;
                return;
            }
        }
    }
};

} 
