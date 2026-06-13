#include "admin_command.hpp"
#include "../../client/client.hpp"
#include "../../player/player.hpp"
#include "../../server/server.hpp"
#include "../../core/core.hpp"
#include "../../utils/packet_utils.hpp"
#include "../../utils/world_manager.hpp"
#include "../../utils/world_parser_v2.h"
#include "../../packet/packet_variant.hpp"
#include "../../packet/packet_types.hpp"
#include "../../utils/byte_stream.hpp"
#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <unordered_map>
#include <sstream>

namespace command {


static const std::unordered_map<uint16_t, const char*> LOCK_TYPES = {
    {242, "World Lock"},
    {202, "Small Lock"},
    {204, "Big Lock"},
    {206, "Huge Lock"},
    {4802, "Royal Lock"},
    {1796, "Diamond Lock"},
    {4994, "Builder's Lock"},
    {4428, "Ruby Lock"},
    {2408, "Emerald Lock"},
    {2950, "Robotic Lock"},
    {13200, "Rayman Lock"},
    {5260, "Harmonic Lock"},
    {5980, "Bunny Lock"},
    {10410, "Legendary Lock"},
    {11550, "Blood Dragon Lock"},
    {8470, "Ecto Lock"},
    {5814, "Guild Lock"},
    {11586, "Prince of Persia Lock"},
    {11902, "Radical City Lock"},
    {12654, "Assassin's Creed Lock"},
    {14536, "Enchanted Lock"},
    {14538, "Royal Enchanted Lock"},
    {13636, "Steampunk Lock"},
    {14296, "Immortals Fenyx Rising Lock"},
    {9640, "My First World Lock"},
    {7188, "Blue Gem Lock"}
};

AdminCommand::AdminCommand() : CommandBase(
    {"admin"},
    {},
    "Scan world for locks and show owner/admin data",
    0
) {}

std::unique_ptr<CommandBase> AdminCommand::clone() const {
    return std::make_unique<AdminCommand>();
}


static world_v2::World* g_current_world = nullptr;
static core::Core* g_core = nullptr;

void AdminCommand::set_current_world(world_v2::World* world) {
    g_current_world = world;
    spdlog::info("AdminCommand: World data set! {} tiles available", world ? world->tiles.size() : 0);
}

void AdminCommand::set_core(core::Core* core) {
    g_core = core;
}

void AdminCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    spdlog::info("=== ADMIN COMMAND EXECUTED ===");
    
    if (!client || !client->get_player()) {
        spdlog::error("AdminCommand: No client or player!");
        return;
    }

    if (!g_core) {
        spdlog::error("AdminCommand: No core set!");
        utils::PacketUtils::send_chat_message(client->get_player(), 
            "`4Error: Core not initialized!");
        return;
    }

    auto* server = g_core->get_server();
    if (!server || !server->get_player()) {
        spdlog::error("AdminCommand: No server player!");
        utils::PacketUtils::send_chat_message(client->get_player(), 
            "`4Error: Server not available!");
        return;
    }

    
    if (!g_current_world || !g_current_world->is_valid) {
        spdlog::error("AdminCommand: No valid world data!");
        utils::PacketUtils::send_chat_message(client->get_player(), 
            "`4Error: No world data! Enter/re-enter a world first.");
        return;
    }

    const auto& tiles = g_current_world->tiles;
    
    if (tiles.empty()) {
        spdlog::error("AdminCommand: Tiles empty!");
        utils::PacketUtils::send_chat_message(client->get_player(), 
            "`4No tile data available!");
        return;
    }

    spdlog::info("AdminCommand: Scanning {} tiles for locks...", tiles.size());

    
    std::vector<const world_v2::Tile*> locks_found;
    
    for (const auto& tile : tiles) {
        if (is_lock_item(tile.fg) && tile.lock_data.has_data()) {
            locks_found.push_back(&tile);
            spdlog::info("  Found lock: {} at ({}, {}) - owner: {}, admins: {}", 
                get_lock_name(tile.fg), tile.x, tile.y, 
                tile.lock_data.owner_uid, tile.lock_data.access_list.size());
        }
    }

    spdlog::info("AdminCommand: Found {} locks with data", locks_found.size());

    if (locks_found.empty()) {
        spdlog::warn("AdminCommand: No locks found!");
        utils::PacketUtils::send_chat_message(client->get_player(), 
            "`2No locks found in this world!");
        return;
    }

    
    show_admin_gui(server->get_player(), locks_found);

    spdlog::info("=== ADMIN COMMAND COMPLETED ===");
}

void AdminCommand::show_admin_gui(player::Player* player, const std::vector<const world_v2::Tile*>& locks) {
    if (!player || locks.empty()) return;

    try {
        
        std::ostringstream dialog;
        dialog << "set_default_color|`o\n";
        dialog << "add_label_with_icon|big|`wLock Scanner``|left|242|\n";
        dialog << "add_spacer|small|\n";
        dialog << fmt::format("add_textbox|`2Found {} locks in this world``|left|\n", locks.size());
        dialog << "add_spacer|small|\n";
        
        
        for (const auto* tile : locks) {
            std::string lock_name = get_lock_name(tile->fg);
            
            
            dialog << fmt::format("add_label_with_icon|small|`w{} `5({}, {})``|left|{}|\n",
                lock_name, tile->x, tile->y, tile->fg);
            
            
            dialog << fmt::format("add_smalltext|`2Owner: `w{}``|\n", tile->lock_data.owner_uid);
            
            
            if (tile->lock_data.access_list.empty()) {
                dialog << "add_smalltext|`4No admins``|\n";
            } else {
                std::vector<std::string> admin_strs;
                for (const auto& admin_uid : tile->lock_data.access_list) {
                    admin_strs.push_back(std::to_string(admin_uid));
                }
                dialog << fmt::format("add_smalltext|`5Admins ({}): `w{}``|\n", 
                    tile->lock_data.access_list.size(),
                    fmt::join(admin_strs, ", "));
            }
            
            dialog << "add_spacer|small|\n";
        }
        
        dialog << "add_quick_exit|\n";
        dialog << "end_dialog|admin_locks|Close||";
        
        std::string dialog_data = dialog.str();
        
        spdlog::info("AdminCommand: Dialog length = {}", dialog_data.length());
        spdlog::info("AdminCommand: Dialog preview: {}", dialog_data.substr(0, 200));
        
        
        packet::Variant variant{};
        variant.add("OnDialogRequest");
        variant.add(dialog_data);
        
        std::vector<std::byte> ext_data = variant.serialize();
        
        spdlog::info("AdminCommand: Variant serialized, {} bytes", ext_data.size());
        
        
        packet::GameUpdatePacket game_packet{};
        game_packet.type = packet::PACKET_CALL_FUNCTION;
        game_packet.net_id = -1;
        game_packet.flags.extended = 1;
        game_packet.data_size = static_cast<uint32_t>(ext_data.size());
        
        
        ByteStream<std::uint16_t> byte_stream{};
        byte_stream.write(packet::NET_MESSAGE_GAME_PACKET);
        byte_stream.write(game_packet);
        byte_stream.write_data(ext_data.data(), ext_data.size());
        
        spdlog::info("AdminCommand: Byte stream size = {}", byte_stream.get_size());
        
        
        player->send_packet(byte_stream.get_data(), 0);
        
        spdlog::info("Sent admin GUI with {} lock entries (ALL locks)", locks.size());
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to send admin GUI: {}", e.what());
    }
}

bool AdminCommand::is_lock_item(uint16_t item_id) {
    return LOCK_TYPES.find(item_id) != LOCK_TYPES.end();
}

const char* AdminCommand::get_lock_name(uint16_t item_id) {
    auto it = LOCK_TYPES.find(item_id);
    if (it != LOCK_TYPES.end()) {
        return it->second;
    }
    return "Unknown Lock";
}

} 
