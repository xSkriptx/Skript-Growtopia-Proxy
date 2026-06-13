#include "vendfind_command.hpp"
#include "../../client/client.hpp"
#include "../../player/player.hpp"
#include "../../server/server.hpp"
#include "../../core/core.hpp"
#include "../../utils/packet_utils.hpp"
#include "../../utils/world_manager.hpp"
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

VendFindCommand::VendFindCommand() : CommandBase(
    {"vendf", "vendloc", "vendfind"},
    {"item_name or item_id"},
    "Search for vending machines selling a specific item",
    1
) {}

std::unique_ptr<CommandBase> VendFindCommand::clone() const {
    return std::make_unique<VendFindCommand>();
}

void VendFindCommand::set_core(core::Core* core) {
    g_core = core;
}

void VendFindCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    spdlog::info("=== VENDFIND COMMAND EXECUTED ===");
    
    if (!client || !client->get_player()) {
        spdlog::error("VendFindCommand: No client or player!");
        return;
    }

    if (args.empty()) {
        utils::PacketUtils::send_chat_message(client->get_player(), 
            "`4Usage: /vendf <item_name or item_id>");
        return;
    }

    if (!g_core) {
        spdlog::error("VendFindCommand: No core set!");
        utils::PacketUtils::send_chat_message(client->get_player(), 
            "`4Error: Core not initialized!");
        return;
    }

    auto* server = g_core->get_server();
    if (!server || !server->get_player()) {
        spdlog::error("VendFindCommand: No server player!");
        utils::PacketUtils::send_chat_message(client->get_player(), 
            "`4Error: Server not available!");
        return;
    }

    
    std::string query;
    for (size_t i = 0; i < args.size(); i++) {
        if (i > 0) query += " ";
        query += args[i];
    }

    spdlog::info("VendFindCommand: Searching for '{}'", query);

    
    auto results = search_vendings(query);

    if (results.empty()) {
        utils::PacketUtils::send_chat_message(client->get_player(), 
            fmt::format("`4No vendings found for '{}'", query));
        return;
    }

    spdlog::info("VendFindCommand: Found {} results", results.size());

    
    show_search_results(server->get_player(), query, results);

    spdlog::info("=== VENDFIND COMMAND COMPLETED ===");
}

void VendFindCommand::save_world_vendings(const world_v2::World& world) {
    if (!world.is_valid || world.tiles.empty()) {
        spdlog::warn("VendFindCommand: Invalid world data");
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

    if (entries.empty()) {
        spdlog::info("VendFindCommand: No vending machines found in world '{}'", world.name);
        return;
    }

    spdlog::info("VendFindCommand: Found {} vending machines in '{}'", entries.size(), world.name);

    
    append_to_file(entries);
}

void VendFindCommand::append_to_file(const std::vector<VendingEntry>& entries) {
    try {
        std::ofstream file(VENDING_DATA_FILE, std::ios::app);
        if (!file.is_open()) {
            spdlog::error("VendFindCommand: Failed to open file for writing");
            return;
        }

        for (const auto& entry : entries) {
            std::string item_name = get_item_name(entry.item_id);
            std::string price_str = entry.is_wl_price 
                ? fmt::format("{}/1 WL", entry.price)
                : fmt::format("{} WL", entry.price);

            
            file << fmt::format("[{}] {}|{}|{}|{}|{}|{}|{}\n",
                entry.timestamp,
                entry.world_name,
                entry.get_type(),
                entry.x,
                entry.y,
                item_name,
                entry.item_id,
                price_str);
        }

        file.close();
        spdlog::info("VendFindCommand: Saved {} entries to {}", entries.size(), VENDING_DATA_FILE);

    } catch (const std::exception& e) {
        spdlog::error("VendFindCommand: Failed to append to file: {}", e.what());
    }
}

std::vector<VendingEntry> VendFindCommand::search_vendings(const std::string& query) {
    std::vector<VendingEntry> results;

    try {
        std::ifstream file(VENDING_DATA_FILE);
        if (!file.is_open()) {
            spdlog::warn("VendFindCommand: vending_data.txt not found or empty");
            return results;
        }

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

            std::string world_name = parts[0];
            std::string type = parts[1];
            std::string item_name = parts[4];
            std::string item_id_str = parts[5];

            
            std::string item_name_lower = item_name;
            std::transform(item_name_lower.begin(), item_name_lower.end(), item_name_lower.begin(), ::tolower);

            
            bool matches = item_name_lower.find(query_lower) != std::string::npos || 
                          item_id_str == query;

            if (matches) {
                VendingEntry entry;
                entry.timestamp = timestamp;
                entry.world_name = world_name;
                entry.tile_id = (type == "Digivend") ? 9268 : 2978;
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

                results.push_back(entry);
            }
        }

        file.close();

    } catch (const std::exception& e) {
        spdlog::error("VendFindCommand: Error searching file: {}", e.what());
    }

    return results;
}

void VendFindCommand::show_search_results(player::Player* player, const std::string& query, 
                                         const std::vector<VendingEntry>& results) {
    if (!player || results.empty()) return;

    try {
        std::ostringstream dialog;
        dialog << "set_default_color|`o\n";
        dialog << "add_label_with_icon|big|`wVend Finder``|left|2978|\n";
        dialog << "add_spacer|small|\n";
        dialog << fmt::format("add_textbox|`2Search: `w{}`` - Found {} results``|left|\n", query, results.size());
        dialog << "add_spacer|small|\n";

        
        for (const auto& entry : results) {
            std::string item_name = get_item_name(entry.item_id);
            std::string price_str = entry.is_wl_price 
                ? fmt::format("{}/1 WL", entry.price)
                : fmt::format("{} WL", entry.price);

            
            dialog << fmt::format("add_label_with_icon|small|`w{} - {}``|left|{}|\n",
                item_name, price_str, entry.item_id);

            
            dialog << fmt::format("add_smalltext|`5{} `w| {} ({}, {})``|\n",
                entry.world_name, entry.get_type(), entry.x, entry.y);

            
            dialog << fmt::format("add_smalltext|`oLast seen: {}``|\n", entry.timestamp);

            dialog << "add_spacer|small|\n";
        }

        dialog << "add_quick_exit|\n";
        dialog << "end_dialog|vendfind_results|Close||";

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

        spdlog::info("Sent vend finder GUI with {} results", results.size());

    } catch (const std::exception& e) {
        spdlog::error("Failed to send vend finder GUI: {}", e.what());
    }
}

std::string VendFindCommand::get_item_name(uint32_t item_id) {
    
    
    return fmt::format("Item {}", item_id);
}

} 
