#include "growscan_command.hpp"
#include "../../client/client.hpp"
#include "../../server/server.hpp"
#include "../../core/core.hpp"
#include "../../player/player.hpp"
#include "../../packet/packet_variant.hpp"
#include "../../packet/packet_types.hpp"
#include "../../utils/packet_utils.hpp"
#include "../../utils/text_parse.hpp"
#include "../../utils/world_info.h"
#include "../../utils/world_tile.h"
#include "../../utils/world_manager.hpp"
#include "../../utils/lua_manager.hpp"
#include <spdlog/spdlog.h>
#include <unordered_map>
#include <sstream>

namespace command {

GrowScanCommand::GrowScanCommand() : CommandBase(
    {"growscan", "gs"},
    {},
    "Scan world for blocks and items",
    0
) {}

std::unique_ptr<CommandBase> GrowScanCommand::clone() const {
    return std::make_unique<GrowScanCommand>();
}

void GrowScanCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    spdlog::warn("GrowScanCommand::execute called without core - this shouldn't happen");
    if (client && client->get_player()) {
        utils::PacketUtils::send_chat_message(client->get_player(), "`4Error: Command requires core access.");
    }
}

void GrowScanCommand::execute_with_core(client::Client* client, const std::vector<std::string>& args, core::Core* core) {
    if (!client || !client->get_player()) {
        spdlog::error("GrowScanCommand: No client or player!");
        return;
    }

    auto* server = core->get_server();
    if (!server || !server->get_player()) {
        spdlog::error("GrowScanCommand: No server available!");
        utils::PacketUtils::send_chat_message(client->get_player(), "`4Error: Server not available.");
        return;
    }

    
    show_growscan_menu(server->get_player());
    spdlog::info("GrowScan menu displayed");
}

void GrowScanCommand::handle_button_click(player::Player* player, const std::string& button, core::Core* core) {
    if (!player || !core) return;

    spdlog::info("GrowScan button clicked: '{}'", button);

    auto& world_mgr = utils::WorldManager::get_instance();

    if (button == "scan_blocks") {
        spdlog::info("GrowScan: Checking world data availability...");
        spdlog::info("GrowScan: has_world = {}", world_mgr.has_world());
        spdlog::info("GrowScan: tiles count = {}", world_mgr.get_tiles().size());
        
        if (!world_mgr.has_world()) {
            utils::PacketUtils::send_chat_message(player, "`4Error: No world data available. Try entering/re-entering a world first.");
            spdlog::warn("GrowScan: No world data available for blocks scan");
            return;
        }

        
        std::string data = scan_world_tiles_from_manager();
        
        spdlog::info("GrowScan: Scan result length = {} chars", data.length());
        
        if (data.empty()) {
            utils::PacketUtils::send_chat_message(player, "`4No blocks found in world.");
            spdlog::warn("GrowScan: Scan returned empty data");
        } else {
            show_scan_results(player, data, "blocks");
            spdlog::info("World blocks scan completed - {} unique block types", count_items_in_data(data));
        }
    }
    else if (button == "scan_items") {
        spdlog::info("GrowScan: Checking world data availability...");
        spdlog::info("GrowScan: has_world = {}", world_mgr.has_world());
        spdlog::info("GrowScan: items count = {}", world_mgr.get_items().size());
        
        if (!world_mgr.has_world()) {
            utils::PacketUtils::send_chat_message(player, "`4Error: No world data available. Try entering/re-entering a world first.");
            spdlog::warn("GrowScan: No world data available for items scan");
            return;
        }

        
        std::string data;
        if (lua::LuaManager::get()) {
            spdlog::info("GrowScan: Calling Lua function scan_floating_items()");
            data = lua::LuaManager::get()->call_function_get_string("scan_floating_items");
            spdlog::info("GrowScan: Lua returned {} chars", data.length());
        }
        
        
        if (data.empty()) {
            spdlog::info("GrowScan: Lua empty, using C++ fallback");
            data = scan_floating_objects_from_manager();
        }
        
        spdlog::info("GrowScan: Final scan result length = {} chars", data.length());
        
        if (data.empty()) {
            utils::PacketUtils::send_chat_message(player, "`4No floating items found.");
            spdlog::warn("GrowScan: Scan returned empty data");
        } else {
            show_scan_results(player, data, "items");
            spdlog::info("Floating items scan completed - {} unique item types", count_items_in_data(data));
        }
    }
    else if (button == "back_to_menu") {
        show_growscan_menu(player);
    }
}

void GrowScanCommand::show_growscan_menu(player::Player* player) {
    if (!player) return;

    try {
        std::string dialog = 
            "set_default_color|`o\n"
            "add_label_with_icon|big|`wWorld Stats``|left|6016|\n"
            "add_spacer|small|\n"
            "add_textbox|`wGrowScan Check``|left|\n"
            "add_button|scan_blocks|`2World Blocks``|noflags|0|0|\n"
            "add_button|scan_items|`6Floating Items``|noflags|0|0|\n"
            "add_spacer|small|\n"
            "end_dialog|growscan_menu|Cancel||";

        packet::Variant variant{};
        variant.add("OnDialogRequest");
        variant.add(dialog);
        
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
        
        spdlog::info("GrowScan menu sent to player");
    } catch (const std::exception& e) {
        spdlog::error("Failed to send GrowScan menu: {}", e.what());
    }
}

void GrowScanCommand::show_scan_results(player::Player* player, const std::string& item_data, const std::string& scan_type) {
    if (!player) return;

    try {
        std::string title = (scan_type == "blocks") ? "World Blocks" : "Floating Items";
        
        std::string dialog = 
            "set_default_color|`o\n"
            "add_label_with_icon|big|`wGrowScan``|left|6016|\n"
            "add_spacer|small|\n"
            "add_label_with_icon_button_list|small|`w%s : %s``|left|findTile_|itemIDseed2tree_itemAmount|" + item_data + "\n"
            "add_spacer|small|\n"
            "add_spacer|small|\n"
            "add_button|back_to_menu|`5Back``|noflags|0|0|\n"
            "embed_data|DialogDwi|0\n"
            "end_dialog|growscan_results|Cancel||";

        packet::Variant variant{};
        variant.add("OnDialogRequest");
        variant.add(dialog);
        
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
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to send GrowScan results: {}", e.what());
    }
}

std::string GrowScanCommand::scan_world_tiles(const world::WorldInfo* world_info) {
    if (!world_info || !world_info->Tiles) {
        return "";
    }
    
    
    std::unordered_map<uint16_t, uint32_t> tile_counts;
    
    for (size_t i = 0; i < world_info->TilesCount; ++i) {
        const auto& tile = world_info->Tiles[i];
        
        
        
        if (tile.Fg != 0) {
            tile_counts[tile.Fg]++;
        }
    }
    
    
    std::ostringstream result;
    for (const auto& pair : tile_counts) {
        result << pair.first << "," << pair.second << ",";
    }
    
    return result.str();
}

std::string GrowScanCommand::scan_world_tiles_from_manager() {
    auto& world_mgr = utils::WorldManager::get_instance();
    
    const std::vector<utils::WorldManager::TileData>& tiles = world_mgr.get_tiles();
    
    if (tiles.empty()) {
        return "";
    }
    
    
    std::unordered_map<uint16_t, uint32_t> tile_counts;
    
    
    spdlog::info("GrowScan: Scanning {} tiles", tiles.size());
    int logged = 0;
    
    for (const auto& tile : tiles) {
        
        if (logged < 10 && (tile.Fg != 0 || tile.Bg != 0)) {
            spdlog::info("Tile {}: Fg={}, Bg={}", logged, tile.Fg, tile.Bg);
            logged++;
        }
        
        
        if (tile.Fg != 0 && tile.Fg != 256) {
            tile_counts[tile.Fg]++;
        }
        
        
        if (tile.Bg != 0 && tile.Bg != 256) {
            tile_counts[tile.Bg]++;
        }
    }
    
    spdlog::info("GrowScan: Found {} unique tile types", tile_counts.size());
    
    
    std::ostringstream result;
    for (const auto& pair : tile_counts) {
        result << pair.first << "," << pair.second << ",";
    }
    
    return result.str();
}

std::string GrowScanCommand::scan_floating_objects(const world::WorldInfo* world_info) {
    
    auto& world_mgr = utils::WorldManager::get_instance();
    const auto& objects = world_mgr.get_items();
    
    if (objects.empty()) {
        spdlog::info("GrowScan: No floating objects in map data");
        return "";
    }
    
    spdlog::info("GrowScan: Scanning {} objects from map data", objects.size());
    
    
    std::unordered_map<uint16_t, uint32_t> object_counts;
    
    for (const auto& obj : objects) {
        object_counts[obj.ItemId] += obj.Amount;
        spdlog::info("  Object: {} x{} at ({:.1f}, {:.1f})", 
                    obj.ItemId, obj.Amount, obj.X, obj.Y);
    }
    
    
    std::ostringstream result;
    for (const auto& pair : object_counts) {
        result << pair.first << "," << pair.second << ",";
    }
    
    return result.str();
}

std::string GrowScanCommand::scan_floating_objects_from_manager() {
    auto& world_mgr = utils::WorldManager::get_instance();
    
    
    const std::vector<world::DroppedItemInfo>& items = world_mgr.get_items();
    const std::vector<world::DroppedItemInfo>& live_objects = world_mgr.get_live_objects();
    
    spdlog::info("[GrowScan] Scanning items: {} from world parse, {} live objects", items.size(), live_objects.size());
    
    if (items.empty() && live_objects.empty()) {
        spdlog::warn("[GrowScan] No items found in either source!");
        return "";
    }
    
    
    std::unordered_map<uint16_t, uint32_t> object_counts;
    
    
    for (const auto& drop : items) {
        object_counts[drop.ItemId] += drop.Amount;
    }
    
    
    for (const auto& drop : live_objects) {
        object_counts[drop.ItemId] += drop.Amount;
    }
    
    
    std::ostringstream result;
    for (const auto& pair : object_counts) {
        result << pair.first << "," << pair.second << ",";
    }
    
    return result.str();
}

int GrowScanCommand::count_items_in_data(const std::string& data) {
    if (data.empty()) return 0;
    
    int count = 0;
    for (char c : data) {
        if (c == ',') count++;
    }
    return count / 2; 
}

} 
