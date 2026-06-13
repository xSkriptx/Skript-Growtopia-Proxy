#include "find_command.hpp"
#include "../../client/client.hpp"
#include "../../player/player.hpp"
#include "../../server/server.hpp"
#include "../../core/core.hpp"
#include "../../utils/packet_utils.hpp"
#include "../../utils/text_parse.hpp"
#include "../../packet/packet_variant.hpp"
#include "../../packet/packet_types.hpp"
#include "../../utils/byte_stream.hpp"
#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <algorithm>

namespace command {

static core::Core* g_core = nullptr;
static const std::string VENDING_DATA_FILE = "vending_data.txt";

FindCommand::FindCommand() : CommandBase(
    {"find"},
    {"item_name or item_id"},
    "Search for items in vending machines",
    1
) {}

std::unique_ptr<CommandBase> FindCommand::clone() const {
    return std::make_unique<FindCommand>();
}

void FindCommand::set_core(core::Core* core) {
    g_core = core;
}

void FindCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    spdlog::info("=== FIND COMMAND EXECUTED ===");
    
    if (!client || !client->get_player()) {
        spdlog::error("FindCommand: No client or player!");
        return;
    }

    if (args.empty()) {
        utils::PacketUtils::send_chat_message(client->get_player(), 
            "`4Usage: /find <item_name or item_id>");
        return;
    }

    if (!g_core) {
        spdlog::error("FindCommand: No core set!");
        return;
    }

    auto* server = g_core->get_server();
    if (!server || !server->get_player()) {
        spdlog::error("FindCommand: No server player!");
        return;
    }

    
    std::string query;
    for (size_t i = 0; i < args.size(); i++) {
        if (i > 0) query += " ";
        query += args[i];
    }

    spdlog::info("FindCommand: Searching for '{}'", query);

    
    auto grouped = search_and_group_by_item(query);

    if (grouped.empty()) {
        utils::PacketUtils::send_chat_message(client->get_player(), 
            fmt::format("`4No items found for '{}'", query));
        return;
    }

    spdlog::info("FindCommand: Found {} unique items", grouped.size());

    
    show_item_search_results(server->get_player(), query, grouped);

    spdlog::info("=== FIND COMMAND COMPLETED ===");
}

void FindCommand::save_world_vendings(const world_v2::World& world) {
    if (!world.is_valid || world.tiles.empty()) {
        return;
    }

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    std::string timestamp = ss.str();

    std::vector<VendingEntry> entries;
    
    for (const auto& tile : world.tiles) {
        if ((tile.fg == 2978 || tile.fg == 9268) && tile.vending_data.has_data()) {
            VendingEntry entry;
            entry.world_name = world.name;
            entry.timestamp = timestamp;
            entry.x = tile.x;
            entry.y = tile.y;
            entry.tile_id = tile.fg;
            entry.item_id = tile.vending_data.item_id;
            entry.price = tile.vending_data.get_price();
            entry.is_wl_price = tile.vending_data.is_world_lock_price();
            entries.push_back(entry);
        }
    }

    if (!entries.empty()) {
        spdlog::info("FindCommand: Found {} vendings in '{}'", entries.size(), world.name);
        append_to_file(entries);
    }
}

void FindCommand::append_to_file(const std::vector<VendingEntry>& entries) {
    try {
        std::ofstream file(VENDING_DATA_FILE, std::ios::app);
        if (!file.is_open()) return;

        for (const auto& entry : entries) {
            std::string price_str = entry.is_wl_price 
                ? fmt::format("{}/1 WL", entry.price)
                : fmt::format("{} WL", entry.price);

            file << fmt::format("[{}] {}|{}|{}|{}|Item {}|{}|{}\n",
                entry.timestamp, entry.world_name, entry.get_type(),
                entry.x, entry.y, entry.item_id, entry.item_id, price_str);
        }
        file.close();
    } catch (const std::exception& e) {
        spdlog::error("FindCommand: Failed to append: {}", e.what());
    }
}

std::unordered_map<uint32_t, std::vector<VendingEntry>> FindCommand::search_and_group_by_item(const std::string& query) {
    std::unordered_map<uint32_t, std::vector<VendingEntry>> grouped;

    try {
        std::ifstream file(VENDING_DATA_FILE);
        if (!file.is_open()) return grouped;

        std::string line;
        std::string query_lower = query;
        std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(), ::tolower);

        while (std::getline(file, line)) {
            size_t bracket_end = line.find(']');
            if (bracket_end == std::string::npos) continue;

            std::string timestamp = line.substr(1, bracket_end - 1);
            std::string data = line.substr(bracket_end + 2);

            std::vector<std::string> parts;
            std::stringstream ss(data);
            std::string part;
            while (std::getline(ss, part, '|')) {
                parts.push_back(part);
            }

            if (parts.size() < 7) continue;

            std::string item_id_str = parts[5];
            
            
            bool matches = item_id_str == query;

            if (matches) {
                VendingEntry entry;
                entry.timestamp = timestamp;
                entry.world_name = parts[0];
                entry.tile_id = (parts[1] == "Digivend") ? 9268 : 2978;
                entry.x = std::stoul(parts[2]);
                entry.y = std::stoul(parts[3]);
                entry.item_id = std::stoul(item_id_str);
                
                std::string price_str = parts[6];
                if (price_str.find("/1 WL") != std::string::npos) {
                    entry.is_wl_price = true;
                    entry.price = std::stoi(price_str.substr(0, price_str.find("/")));
                } else {
                    entry.is_wl_price = false;
                    entry.price = std::stoi(price_str.substr(0, price_str.find(" ")));
                }

                grouped[entry.item_id].push_back(entry);
            }
        }
        file.close();
    } catch (const std::exception& e) {
        spdlog::error("FindCommand: Error searching: {}", e.what());
    }

    return grouped;
}

void FindCommand::show_item_search_results(player::Player* player, const std::string& query,
                                           const std::unordered_map<uint32_t, std::vector<VendingEntry>>& grouped) {
    if (!player || grouped.empty()) return;

    try {
        std::ostringstream dialog;
        dialog << "set_default_color|`o\n";
        dialog << "add_label_with_icon|big|`wItem Finder``|left|2978|\n";
        dialog << "add_spacer|small|\n";
        dialog << fmt::format("add_textbox|`2Search: `w{}`` - Found {} items``|left|\n", query, grouped.size());
        dialog << "add_spacer|small|\n";

        
        for (const auto& [item_id, entries] : grouped) {
            dialog << fmt::format("add_button|view_item_{}|`wItem {} `2({} vendings)``|noflags|0|0|\n",
                item_id, item_id, entries.size());
        }

        dialog << "add_spacer|small|\n";
        dialog << "add_quick_exit|\n";
        dialog << "end_dialog|find_items|Close||";

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

        spdlog::info("FindCommand: Sent item list with {} items", grouped.size());

    } catch (const std::exception& e) {
        spdlog::error("FindCommand: Failed to send GUI: {}", e.what());
    }
}

void FindCommand::show_vending_locations(player::Player* player, uint32_t item_id,
                                         const std::vector<VendingEntry>& entries) {
    if (!player || entries.empty()) return;

    try {
        std::ostringstream dialog;
        dialog << "set_default_color|`o\n";
        dialog << fmt::format("add_label_with_icon|big|`wItem {} Locations``|left|{}|\n", item_id, item_id);
        dialog << "add_spacer|small|\n";
        dialog << fmt::format("add_textbox|`2Found {} vendings selling this item``|left|\n", entries.size());
        dialog << "add_spacer|small|\n";

        
        for (const auto& entry : entries) {
            std::string price_str = entry.is_wl_price 
                ? fmt::format("{}/1 WL", entry.price)
                : fmt::format("{} WL", entry.price);

            
            dialog << fmt::format("add_button|warp_{}|`5WARP``|noflags|0|0|\n", entry.world_name);
            dialog << fmt::format("add_smalltext|`w{} `5| {} ({}, {}) - `2{}``|\n",
                entry.world_name, entry.get_type(), entry.x, entry.y, price_str);
            dialog << fmt::format("add_smalltext|`oLast seen: {}``|\n", entry.timestamp);
            dialog << "add_spacer|small|\n";
        }

        dialog << "add_quick_exit|\n";
        dialog << "end_dialog|vending_locations|Close||";

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

        spdlog::info("FindCommand: Sent vending locations with {} entries", entries.size());

    } catch (const std::exception& e) {
        spdlog::error("FindCommand: Failed to send locations GUI: {}", e.what());
    }
}

void FindCommand::handle_dialog_click(player::Player* player, const std::string& button) {
    if (!player || !g_core) return;

    spdlog::info("FindCommand: Dialog click: '{}'", button);

    
    auto* server = g_core->get_server();
    if (!server || !server->get_player()) return;

    
    if (button.rfind("view_item_", 0) == 0) {
        std::string item_id_str = button.substr(10);
        uint32_t item_id = std::stoul(item_id_str);

        
        auto grouped = search_and_group_by_item(item_id_str);
        if (grouped.find(item_id) != grouped.end()) {
            
            FindCommand cmd;
            cmd.show_vending_locations(server->get_player(), item_id, grouped[item_id]);
        }
    }
    
    else if (button.rfind("warp_", 0) == 0) {
        std::string world_name = button.substr(5);
        
        
        TextParse warp_parse{};
        warp_parse.add("action", {"join_request"});
        warp_parse.add("name", {world_name});
        warp_parse.add("invitedWorld", {"0"});
        
        std::string warp_data = warp_parse.get_raw("\n");
        
        
        ByteStream<std::uint16_t> byte_stream{};
        byte_stream.write(2);  
        byte_stream.write_data(reinterpret_cast<const std::byte*>(warp_data.data()), warp_data.size());
        
        player->send_packet(byte_stream.get_data(), 0);
        
        utils::PacketUtils::send_chat_message(player, 
            fmt::format("`2Warping to {}...", world_name));
    }
}

} 
