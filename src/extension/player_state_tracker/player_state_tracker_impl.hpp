#pragma once
#include "player_state_tracker.hpp"
#include "../../core/core.hpp"
#include "../../packet/packet_types.hpp"
#include "../../packet/tank_packet.hpp"
#include "../../utils/byte_stream.hpp"
#include "../../utils/text_parse.hpp"
#include "../../packet/packet_variant.hpp"
#include "../../utils/player_tracker.hpp"  
#include <spdlog/spdlog.h>
#include <chrono>

namespace extension::player_state_tracker {

class PlayerStateTrackerExtension final : public IPlayerStateTrackerExtension {
    core::Core* core_;
    
    
    int32_t local_netid_ = -1;            
    float local_x_ = 0.0f;                 
    float local_y_ = 0.0f;                 
    bool is_spawned_ = false;              
    
    
    bool last_on_ground_ = true;
    uint32_t consecutive_jumps_ = 0;
    
public:
    explicit PlayerStateTrackerExtension(core::Core* core)
        : core_{ core }
    {
    }

    ~PlayerStateTrackerExtension() override = default;

    void init() override {
        spdlog::trace("`2[PlayerStateTracker]`` Initializing");
        
        
        core_->get_config().set("features.double_jump", true);
        core_->get_config().set("features.immune_damage", false);
        
        spdlog::trace("`2[PlayerStateTracker]`` Double jump `2ENABLED`` by default");
        spdlog::trace("`o[PlayerStateTracker]`` Damage immunity `4DISABLED`` - use /immune to enable");

        
        core_->get_event_dispatcher().prependListener(
            core::EventType::Packet,
            [this](const core::EventPacket& event) {
                handle_packet(event);
            }
        );
        
        spdlog::trace("`2[PlayerStateTracker]`` System ready");
    }

private:
    void handle_packet(const core::EventPacket& event) {
        const auto& game_packet = event.get_packet();
        
        
        if (game_packet.type == packet::PACKET_CALL_FUNCTION && event.from == core::EventFrom::FromServer) {
            try {
                const auto& ext_data = event.get_ext_data();
                packet::Variant variant{};
                variant.deserialize(ext_data);
                
                if (variant.size() >= 2) {
                    std::string function_name = variant.get<std::string>(0);
                    if (function_name == "OnSpawn") {
                        std::string spawn_data = variant.get<std::string>(1);
                        TextParse text_parse{ spawn_data };
                        
                        if (text_parse.get("type") == "local") {
                            local_netid_ = text_parse.get<int32_t>("netID");
                            is_spawned_ = true;
                            core_->get_config().set<int>("player.netid", local_netid_);
                            spdlog::info("[PlayerStateTracker] OnSpawn - Local NetID: {} detected and saved.", local_netid_);
                        }
                    }
                }
            } catch (...) {}
        }
        
        
        if (game_packet.type != packet::PACKET_STATE) {
            return;
        }

        
        
        auto* tank = reinterpret_cast<const packet::TankUpdatePacket*>(&game_packet);
        
        
        if (event.from == core::EventFrom::FromClient) {
            spdlog::info("\033[36m[STATE-CLIENT]\033[0m NetID: {} | vec_x: {:.1f}, vec_y: {:.1f} | flags: 0x{:X}", 
                       tank->net_id, tank->vec_x, tank->vec_y, tank->flags);
        } else {
            spdlog::info("\033[35m[STATE-SERVER]\033[0m NetID: {} | vec_x: {:.1f}, vec_y: {:.1f} | flags: 0x{:X}", 
                       tank->net_id, tank->vec_x, tank->vec_y, tank->flags);
        }
        
        
        if (tank->vec_x >= 0 && tank->vec_y >= 0) {
            int tile_x = static_cast<int>(tank->vec_x / 32.0f);
            int tile_y = static_cast<int>(tank->vec_y / 32.0f);
            
            
            if (event.from == core::EventFrom::FromClient) {
                local_x_ = tank->vec_x;
                local_y_ = tank->vec_y;
                
                
                core_->get_config().set<std::string>("player.position.x", std::to_string(tank->vec_x));
                core_->get_config().set<std::string>("player.position.y", std::to_string(tank->vec_y));
                
                spdlog::info("\033[36m[CLIENT POS]\033[0m NetID: {} | Tile ({}, {}) | Pixels ({:.1f}, {:.1f})", 
                           tank->net_id, tile_x, tile_y, tank->vec_x, tank->vec_y);
                handle_client_actions(event, tank);
            }
            
            
            if (event.from == core::EventFrom::FromServer) {
                bool is_local_player = (local_netid_ > 0 && tank->net_id == local_netid_);
                std::string player_type = is_local_player ? "[LOCAL]" : "[OTHER]";
                
                spdlog::info("\033[34m[SERVER POS]\033[0m {} NetID: {} | Tile ({}, {}) | Pixels ({:.1f}, {:.1f})", 
                           player_type, tank->net_id, tile_x, tile_y, tank->vec_x, tank->vec_y);
                
                utils::PlayerTracker::get_instance().update_player_position(
                    tank->net_id, tank->vec_x, tank->vec_y
                );
                
                
                if (is_local_player) {
                    local_x_ = tank->vec_x;
                    local_y_ = tank->vec_y;
                    
                    
                    core_->get_config().set<std::string>("player.position.x", std::to_string(tank->vec_x));
                    core_->get_config().set<std::string>("player.position.y", std::to_string(tank->vec_y));
                    
                    handle_server_updates(event, tank);
                }
            }
        }
    }

    void handle_client_actions(const core::EventPacket& event, const packet::TankUpdatePacket* tank) {
        bool needs_modification = false;
        packet::TankUpdatePacket modified_tank = *tank;
        
        
        spdlog::debug("[CLIENT STATE] Processing - flags: 0x{:X}", tank->flags);
        
        
        if (tank->vec_x > 0 && tank->vec_y > 0) {
            int tile_x = static_cast<int>(tank->vec_x / 32.0f);
            int tile_y = static_cast<int>(tank->vec_y / 32.0f);
            spdlog::debug("[CLIENT POS] Tile ({}, {}) | Pixels ({:.0f}, {:.0f})", 
                        tile_x, tile_y, tank->vec_x, tank->vec_y);
        }
        
        
        bool jumping = (tank->flags & packet::PACKET_FLAG_ON_JUMP) != 0;
        bool on_solid = (tank->flags & packet::PACKET_FLAG_ON_SOLID) != 0;
        
        spdlog::info("\033[33m[FLAG CHECK]\033[0m jumping={}, on_solid={}", jumping, on_solid);
        
        
        if (core_->get_config().get<bool>("features.double_jump")) {
            if (jumping) {
                
                if (!on_solid) {
                    consecutive_jumps_++;
                    spdlog::info("\033[32m[DOUBLE JUMP]\033[0m Mid-air jump #{} at tile ({}, {})", 
                               consecutive_jumps_, 
                               static_cast<int>(tank->vec_x / 32.0f),
                               static_cast<int>(tank->vec_y / 32.0f));
                } else {
                    consecutive_jumps_ = 1;
                    spdlog::debug("[JUMP] Ground jump at tile ({}, {})", 
                                static_cast<int>(tank->vec_x / 32.0f),
                                static_cast<int>(tank->vec_y / 32.0f));
                }
                
                
                modified_tank.flags |= packet::PACKET_FLAG_ON_SOLID;
                needs_modification = true;
                
                spdlog::info("\033[32m[DOUBLE JUMP]\033[0m Modified - Original: 0x{:X} -> New: 0x{:X}", 
                           tank->flags, modified_tank.flags);
            } else {
                
                if (consecutive_jumps_ > 0) {
                    spdlog::debug("[JUMP] Sequence ended ({} jumps total)", consecutive_jumps_);
                    consecutive_jumps_ = 0;
                }
            }
        }
        
        
        if (needs_modification) {
            spdlog::info("\033[32m[SENDING MODIFIED PACKET]\033[0m For double jump");
            send_modified_packet(event, modified_tank);
        }
    }

    void handle_server_updates(const core::EventPacket& event, const packet::TankUpdatePacket* tank) {
        bool needs_modification = false;
        packet::TankUpdatePacket modified_tank = *tank;
        
        spdlog::info("\033[33m[SERVER STATE]\033[0m Got packet for our netID: {} - flags: 0x{:X}", tank->net_id, tank->flags);
        
        
        bool is_our_player = (local_netid_ > 0 && tank->net_id == local_netid_);
        
        spdlog::info("\033[33m[NETID CHECK]\033[0m local_netid_={}, tank->net_id={}, is_our_player={}", 
                    local_netid_, tank->net_id, is_our_player);
        
        
        if (is_our_player && tank->vec_x > 0 && tank->vec_y > 0) {
            int tile_x = static_cast<int>(tank->vec_x / 32.0f);
            int tile_y = static_cast<int>(tank->vec_y / 32.0f);
            spdlog::debug("[SERVER UPDATE] Position confirmed: tile ({}, {})", tile_x, tile_y);
        }
        
        
        bool fire_damage = (tank->flags & packet::PACKET_FLAG_ON_FIRE_DAMAGE) != 0;
        bool acid_damage = (tank->flags & packet::PACKET_FLAG_ON_ACID_DAMAGE) != 0;
        
        spdlog::info("\033[33m[DAMAGE FLAGS]\033[0m fire={}, acid={}", fire_damage, acid_damage);
        
        
        if (is_our_player && (fire_damage || acid_damage)) {
            spdlog::info("\033[31m[DAMAGE DETECTED]\033[0m Fire: {} | Acid: {} at tile ({}, {})", 
                       fire_damage, acid_damage,
                       static_cast<int>(tank->vec_x / 32.0f),
                       static_cast<int>(tank->vec_y / 32.0f));
        }
        
        
        if (is_our_player && core_->get_config().get<bool>("features.immune_damage")) {
            spdlog::info("\033[33m[IMMUNITY CHECK]\033[0m Immunity is ENABLED");
            if (fire_damage) {
                modified_tank.flags &= ~packet::PACKET_FLAG_ON_FIRE_DAMAGE;
                needs_modification = true;
                spdlog::info("\033[32m[IMMUNE]\033[0m Blocked FIRE damage!");
            }
            
            if (acid_damage) {
                modified_tank.flags &= ~packet::PACKET_FLAG_ON_ACID_DAMAGE;
                needs_modification = true;
                spdlog::info("\033[32m[IMMUNE]\033[0m Blocked ACID damage!");
            }
            
            if (needs_modification) {
                spdlog::info("\033[32m[IMMUNE]\033[0m Modified packet - Original: 0x{:X} -> New: 0x{:X}", 
                           tank->flags, modified_tank.flags);
            }
        }
        
        
        if (needs_modification) {
            spdlog::info("\033[32m[SENDING MODIFIED PACKET]\033[0m For immunity");
            send_modified_packet(event, modified_tank);
        }
    }

    void send_modified_packet(const core::EventPacket& event, const packet::TankUpdatePacket& modified_tank) {
        ByteStream<std::uint16_t> byte_stream{};
        byte_stream.write(packet::NET_MESSAGE_GAME_PACKET);
        
        
        auto game_packet = event.get_packet();
        byte_stream.write(game_packet);
        
        
        const std::byte* tank_bytes = reinterpret_cast<const std::byte*>(&modified_tank);
        byte_stream.write_data(tank_bytes, sizeof(packet::TankUpdatePacket));
        
        
        event.get_target().send_packet(byte_stream.get_data(), 0);
        
        
        const_cast<core::EventPacket&>(event).canceled = true;
        
        spdlog::debug("[PlayerStateTracker] Sent modified packet, canceled original");
    }
};

} 
