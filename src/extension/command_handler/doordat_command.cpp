#include "doordat_command.hpp"
#include "../../client/client.hpp"
#include "../../player/player.hpp"
#include "../../server/server.hpp"
#include "../../core/core.hpp"
#include "../../utils/packet_utils.hpp"
#include "../../packet/packet_variant.hpp"
#include "../../packet/packet_types.hpp"
#include "../../utils/byte_stream.hpp"
#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <unordered_map>
#include <sstream>

namespace command {


static const std::unordered_map<uint16_t, const char*> DOOR_TYPES = {
    {12, "Door"},
    {546, "Blue Portal"},
    {1684, "Path Marker"}
};

DoordatCommand::DoordatCommand() : CommandBase(
    {"doordat"},
    {},
    "Scan world for doors and portals with their destination data",
    0
) {}

std::unique_ptr<CommandBase> DoordatCommand::clone() const {
    return std::make_unique<DoordatCommand>();
}


static world_v2::World* g_current_world = nullptr;
static core::Core* g_core = nullptr;

void DoordatCommand::set_current_world(world_v2::World* world) {
    g_current_world = world;
    spdlog::info("DoordatCommand: World data set! {} tiles available", world ? world->tiles.size() : 0);
}

void DoordatCommand::set_core(core::Core* core) {
    g_core = core;
}

bool DoordatCommand::is_door_item(uint16_t item_id) {
    return DOOR_TYPES.find(item_id) != DOOR_TYPES.end();
}

const char* DoordatCommand::get_door_name(uint16_t item_id) {
    auto it = DOOR_TYPES.find(item_id);
    return it != DOOR_TYPES.end() ? it->second : "Unknown Door";
}

void DoordatCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    spdlog::info("=== DOORDAT COMMAND EXECUTED ===");
    
    if (!client || !client->get_player()) {
        spdlog::error("DoordatCommand: No client or player!");
        return;
    }

    if (!g_core) {
        spdlog::error("DoordatCommand: No core set!");
        utils::PacketUtils::send_chat_message(client->get_player(), 
            "`4Error: Core not initialized!");
        return;
    }

    auto* server = g_core->get_server();
    if (!server || !server->get_player()) {
        spdlog::error("DoordatCommand: No server player!");
        utils::PacketUtils::send_chat_message(client->get_player(), 
            "`4Error: Server not available!");
        return;
    }

    
    if (!g_current_world || !g_current_world->is_valid) {
        spdlog::error("DoordatCommand: No valid world data!");
        utils::PacketUtils::send_chat_message(client->get_player(), 
            "`4Error: No world data! Enter/re-enter a world first.");
        return;
    }

    const auto& tiles = g_current_world->tiles;
    
    if (tiles.empty()) {
        spdlog::error("DoordatCommand: Tiles empty!");
        utils::PacketUtils::send_chat_message(client->get_player(), 
            "`4No tile data available!");
        return;
    }

    spdlog::info("DoordatCommand: Scanning {} tiles for doors...", tiles.size());

    
    std::vector<const world_v2::Tile*> doors_found;
    
    for (const auto& tile : tiles) {
        if (is_door_item(tile.fg)) {
            doors_found.push_back(&tile);
            spdlog::info("  Found {}: at ({}, {}) - has_extra: {}, extra_type: {}, flags: {}", 
                get_door_name(tile.fg), tile.x, tile.y, 
                tile.has_extra(), tile.extra_type, tile.flags);
        }
    }

    spdlog::info("DoordatCommand: Found {} doors/portals", doors_found.size());

    if (doors_found.empty()) {
        spdlog::warn("DoordatCommand: No doors found!");
        utils::PacketUtils::send_chat_message(client->get_player(), 
            "`2No doors or portals found in this world!");
        return;
    }

    
    show_door_gui(server->get_player(), doors_found);

    spdlog::info("=== DOORDAT COMMAND COMPLETED ===");
}

void DoordatCommand::show_door_gui(player::Player* player, const std::vector<const world_v2::Tile*>& doors) {
    std::ostringstream dialog;
    dialog << "set_default_color|`o\n";
    dialog << "add_label_with_icon|big|`wDoor & Portal Data``|left|12|\n";
    dialog << "add_spacer|small|\n";
    
    dialog << fmt::format("add_textbox|`2Found {} doors/portals``|left|\n", doors.size());
    dialog << "add_spacer|small|\n";

    for (const auto* tile : doors) {
        
        dialog << fmt::format("add_label_with_icon|small|`w{} `0(ID: {})``|left|{}|\n", 
            get_door_name(tile->fg), tile->fg, tile->fg);
        
        
        dialog << fmt::format("add_smalltext|`oPosition: `5({}, {})``|\n", 
            tile->x, tile->y);
        
        
        dialog << fmt::format("add_smalltext|`bItem ID: `w{}``|\n", tile->fg);
        dialog << fmt::format("add_smalltext|`bBackground: `w{}``|\n", tile->bg);
        dialog << fmt::format("add_smalltext|`bFlags: `w0x{:04X}`` `o({})``|\n", tile->flags, tile->flags);
        dialog << fmt::format("add_smalltext|`bExtra Type: `w{}``|\n", tile->extra_type);
        
        if (tile->has_parent()) {
            dialog << fmt::format("add_smalltext|`bParent Index: `w{}``|\n", tile->parent_index);
        }
        
        
        if (tile->has_extra()) {
            dialog << "add_smalltext|`2✓ Has extra data``|\n";
            
            
            
            
            if (tile->extra_type == 1 || tile->extra_type == 2) {
                dialog << fmt::format("add_smalltext|`5Type {}: Door/Portal data``|\n", tile->extra_type);
                
                
                if (tile->door_data.has_data()) {
                    dialog << fmt::format("add_smalltext|`c✓ Door ID: `w{}``|\n", tile->door_data.destination);
                } else {
                    dialog << "add_smalltext|`4No door ID found``|\n";
                }
            } else if (tile->extra_type == 14) {
                dialog << "add_smalltext|`5Type 14: Portal/Sign data``|\n";
            } else {
                dialog << fmt::format("add_smalltext|`5Type {}: Unknown data``|\n", tile->extra_type);
            }
            
            
            if (tile->is_lock()) {
                dialog << fmt::format("add_smalltext|`4Lock Owner UID: {}``|\n", tile->lock_data.owner_uid);
                dialog << fmt::format("add_smalltext|`4Admins: {}``|\n", tile->lock_data.access_list.size());
            }
            
            
            if (tile->is_vending()) {
                dialog << fmt::format("add_smalltext|`3Vending Item: {}``|\n", tile->vending_data.item_id);
                dialog << fmt::format("add_smalltext|`3Price: {}``|\n", tile->vending_data.get_price());
            }
        } else {
            dialog << "add_smalltext|`4✗ No extra data``|\n";
        }
        
        dialog << "add_spacer|small|\n";
    }

    dialog << "add_textbox|`oNote: Extra Type 1 = Door destination, Type 14 = Portal/Sign``|left|\n";
    dialog << "add_quick_exit|\n";
    dialog << "end_dialog|doordat_info|Close||\n";

    std::string dialog_data = dialog.str();

    
    packet::Variant variant{};
    variant.add("OnDialogRequest");
    variant.add(dialog_data);

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

    player->send_packet(byte_stream.get_data(), 0);
    
    spdlog::info("DoordatCommand: Sent door data GUI with detailed info");
}

} 
