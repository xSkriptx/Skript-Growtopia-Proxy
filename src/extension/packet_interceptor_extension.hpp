#pragma once
#include "../extension/extension.hpp"
#include "../utils/world_manager.hpp"
#include "../utils/inventory_manager.hpp"
#include "../utils/byte_stream.hpp"
#include "../packet/tank_packet.hpp"
#include "../packet/packet_types.hpp"
#include "command_handler/autocollect_command.hpp"  
#include <spdlog/spdlog.h>
#include <fmt/format.h>


extern bool g_teleport_mode;

namespace extension::packet_interceptor {

class PacketInterceptor final : public IExtension {
    core::Core* core_;
    
public:
    PROVIDE_EXT_UID(0x504B5443); 

    explicit PacketInterceptor(core::Core* core) : core_{core} {}
    ~PacketInterceptor() override = default;

    void init() override {
        spdlog::trace("=== PacketInterceptor::init() called ===");
        
        
        core_->get_event_dispatcher().prependListener(
            core::EventType::Packet,
            [this](const core::EventPacket& event) {
                handle_server_packet(event);
            }
        );
        
        spdlog::trace("Packet Interceptor extension loaded - tracking live objects");
    }

    void free() override {
        delete this;
    }

private:
    void handle_server_packet(const core::EventPacket& event) {
        if (event.from != core::EventFrom::FromServer) return;
        
        const auto& game_packet = event.get_packet();
        const auto& ext_data = event.get_ext_data();
        
        
        if (game_packet.type == packet::PACKET_SEND_INVENTORY_STATE) {
            spdlog::info(">>> PACKET_SEND_INVENTORY_STATE received! Size: {}", ext_data.size());
            handle_inventory_state(ext_data);
            return;
        }
        
        
        if (game_packet.type == packet::PACKET_ITEM_CHANGE_OBJECT) {
            spdlog::info(">>> PACKET_ITEM_CHANGE_OBJECT received! Size: {}", ext_data.size());

            
            
            
            
            if (ext_data.empty()) {
                auto& world_mgr = utils::WorldManager::get_instance();
                const auto& items = world_mgr.get_items();
                const auto& live = world_mgr.get_live_objects();
                
                for (const auto& item : items) {
                    bool found = false;
                    for (const auto& lo : live) {
                        if (lo.Uid == item.Uid) { found = true; break; }
                    }
                    if (!found) {
                        world_mgr.add_live_object(item);
                        spdlog::info("Converted world item UID {} -> live object", item.Uid);
                        command::AutoCollectCommand::notify_item_drop(item.X, item.Y);
                        break;
                    }
                }
            }
        }
        
        
        if (ext_data.size() >= sizeof(packet::TankUpdatePacket)) {
            const packet::TankUpdatePacket* tank = 
                reinterpret_cast<const packet::TankUpdatePacket*>(ext_data.data());
            
            switch (game_packet.type) {
                case packet::PACKET_ITEM_CHANGE_OBJECT:
                    spdlog::info(">>> Handling: net_id={}, int_data={}, float_var={}", 
                                tank->net_id, tank->int_data, tank->float_var);
                    handle_item_change_object(tank);
                    break;
                case packet::PACKET_MODIFY_ITEM_INVENTORY:
                    handle_modify_inventory(tank);
                    break;
                case packet::PACKET_TILE_CHANGE_REQUEST:
                    handle_tile_change(tank);
                    break;
                default:
                    break;
            }
        }
    }

    void handle_item_change_object(const packet::TankUpdatePacket* tank) {
        auto& world_mgr = utils::WorldManager::get_instance();
        
        
        spdlog::info("[ITEM-CHANGE] net_id={}, target={}, flags=0x{:X}, float_var={}, int_data={}, x2={}, y2={}, pos=({:.1f},{:.1f})", 
                    tank->net_id, tank->target_net_id, tank->flags, tank->float_var, tank->int_data,
                    tank->vec_x2, tank->vec_y2, tank->vec_x, tank->vec_y);
        
        
        if (tank->is_item_drop()) {
            world::DroppedItemInfo item;
            item.ItemId = static_cast<uint16_t>(tank->int_data);
            item.X = tank->vec_x;
            item.Y = tank->vec_y;
            item.Amount = static_cast<uint32_t>(tank->float_var);
            item.Flag = static_cast<uint32_t>(tank->flags);
            
            static uint32_t uid_counter = 1;
            item.Uid = uid_counter++;
            
            world_mgr.add_live_object(item);
            spdlog::info("✓ Live object spawned: {} x{} at ({:.1f}, {:.1f})", 
                        item.ItemId, item.Amount, item.X, item.Y);
            
            command::AutoCollectCommand::notify_item_drop(item.X, item.Y);
        }
        
        else if (tank->is_item_collect()) {
            uint32_t uid = tank->int_data;
            world_mgr.remove_live_object(uid);
            world_mgr.remove_dropped_item_by_uid(uid);
            spdlog::info("✓ Live object collected: UID {}", uid);
        }
        
        else if (tank->is_item_update()) {
            spdlog::info("✓ Live object count updated");
        }
        else {
            spdlog::warn("[ITEM-CHANGE] Unknown net_id pattern - not drop/collect/update");
        }
    }

    void handle_inventory_state(const std::vector<std::byte>& ext_data) {
        auto& inv_mgr = utils::InventoryManager::get_instance();
        inv_mgr.parse_inventory(ext_data);
    }

    void handle_modify_inventory(const packet::TankUpdatePacket* tank) {
        spdlog::debug("Inventory modified: item {} count {}", tank->int_data, (int)tank->float_var);
    }

    void handle_tile_change(const packet::TankUpdatePacket* tank) {
        spdlog::debug("Tile changed at ({}, {})", tank->int_x, tank->int_y);
        
        
        spdlog::info("[TILE CLICK] Player clicked tile at ({}, {})", tank->int_x, tank->int_y);
        
        
        if (g_teleport_mode) {
            spdlog::info("[TELEPORT MODE] Attempting to teleport to ({}, {})", tank->int_x, tank->int_y);
            
            
            float target_x = static_cast<float>(tank->int_x) * 32.0f + 16.0f;
            float target_y = static_cast<float>(tank->int_y) * 32.0f + 16.0f;
            
            
            std::string move_cmd = fmt::format("action|move\nx|{}\ny|{}", 
                static_cast<int>(target_x), static_cast<int>(target_y));
            
            
            if (core_ && core_->get_client() && core_->get_client()->get_player()) {
                packet::GameUpdatePacket pkt{};
                pkt.type = packet::PACKET_CALL_FUNCTION;
                pkt.flags.extended = 1;
                pkt.data_size = static_cast<uint32_t>(move_cmd.size());
                
                ByteStream<std::uint16_t> bs{};
                bs.write(packet::NET_MESSAGE_GAME_PACKET);
                bs.write(pkt);
                bs.write_data(reinterpret_cast<const std::byte*>(move_cmd.c_str()), move_cmd.size());
                
                core_->get_client()->get_player()->send_packet(bs.get_data(), 0);
                    
                spdlog::info("[TELEPORT MODE] Sent move packet to ({}, {}) - pixels ({:.0f}, {:.0f})", 
                            tank->int_x, tank->int_y, target_x, target_y);
            }
        }
    }
};

} 
