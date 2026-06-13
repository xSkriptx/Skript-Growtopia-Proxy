#include "dat_command.hpp"
#include "../../client/client.hpp"
#include "../../player/player.hpp"
#include "../../server/server.hpp"
#include "../../core/core.hpp"
#include "../../utils/packet_utils.hpp"
#include "../../packet/packet_variant.hpp"
#include "../../packet/packet_types.hpp"
#include "../../packet/tank_packet.hpp"
#include "../../utils/byte_stream.hpp"
#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <unordered_map>
#include <sstream>
#include <cctype>
#include <regex>
#include <iomanip>
#include <algorithm>
#include <thread>
#include <chrono>
#include <atomic>
#include <utility>

namespace command {


static const std::unordered_map<uint16_t, const char*> ITEM_TYPES = {
    {12, "Door"},
    {546, "Blue Portal"},
    {1684, "Path Marker"},
    {6, "Sign"},
    {858, "Sign"},
    {24, "Vending Machine"},
    {656, "Vending Machine"},
    {658, "Vending Machine"},
    {608, "Vending Machine"},
    {242, "World Lock"},
    {202, "Small Lock"},
    {1452, "Donation Box"},
};

DatCommand::DatCommand() : CommandBase(
    {"dat"},
    {},
    "Show tile data. Use /dat scan [radius] [delay_ms] for donation auto-scan",
    0
) {}

std::unique_ptr<CommandBase> DatCommand::clone() const {
    return std::make_unique<DatCommand>();
}


static world_v2::World* g_current_world = nullptr;
static core::Core* g_core = nullptr;
static std::unordered_map<uint64_t, std::vector<std::string>> g_donation_dialog_cache;
static std::unordered_map<uint64_t, int> g_donation_count_cache;
static std::atomic<bool> g_donation_scan_running{ false };

static uint64_t make_xy_key(uint32_t x, uint32_t y) {
    return (static_cast<uint64_t>(x) << 32) | static_cast<uint64_t>(y);
}

static std::string bytes_to_hex_preview(const std::vector<uint8_t>& bytes, size_t max_bytes = 64) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    const size_t count = std::min(bytes.size(), max_bytes);
    for (size_t i = 0; i < count; ++i) {
        if (i > 0) oss << ' ';
        oss << std::setw(2) << static_cast<int>(bytes[i]);
    }
    if (bytes.size() > max_bytes) {
        oss << " ...";
    }
    return oss.str();
}

static bool read_player_tile_position(core::Core* core, int& tile_x, int& tile_y) {
    if (!core) return false;
    try {
        const std::string pos_x_str = core->get_config().get<std::string>("player.position.x");
        const std::string pos_y_str = core->get_config().get<std::string>("player.position.y");
        const float pos_x = std::stof(pos_x_str);
        const float pos_y = std::stof(pos_y_str);
        tile_x = static_cast<int>(pos_x / 32.0f);
        tile_y = static_cast<int>(pos_y / 32.0f);
        return true;
    } catch (...) {
        return false;
    }
}

static void send_tile_activate(player::Player* to_server_player, uint32_t tile_x, uint32_t tile_y) {
    if (!to_server_player) return;

    packet::TankUpdatePacket pkt{};
    pkt.type = packet::PACKET_TILE_ACTIVATE_REQUEST;
    pkt.net_id = -1;
    pkt.int_x = tile_x;
    pkt.int_y = tile_y;
    pkt.vec_x = static_cast<float>(tile_x) * 32.0f + 16.0f;
    pkt.vec_y = static_cast<float>(tile_y) * 32.0f + 16.0f;

    ByteStream<std::uint16_t> bs{};
    bs.write(packet::NET_MESSAGE_GAME_PACKET);
    bs.write(pkt);
    to_server_player->send_packet(bs.get_data(), 0);
}

static void start_donation_auto_scan(player::Player* local_player,
                                     player::Player* to_server_player,
                                     int center_x, int center_y,
                                     int radius, int delay_ms,
                                     std::vector<std::pair<uint32_t, uint32_t>> targets) {
    std::thread([local_player, to_server_player, center_x, center_y, radius, delay_ms, targets = std::move(targets)]() mutable {
        const size_t total = targets.size();
        utils::PacketUtils::send_chat_message(local_player,
            fmt::format("`2Dat:`o donation auto-scan started (`w{} boxes`` in radius `w{} ``), delay `w{}ms``",
                total, radius, delay_ms));

        for (size_t i = 0; i < total; ++i) {
            if (!to_server_player || !to_server_player->is_connected()) {
                break;
            }
            send_tile_activate(to_server_player, targets[i].first, targets[i].second);
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        }

        utils::PacketUtils::send_chat_message(local_player,
            fmt::format("`2Dat:`o donation auto-scan done near (`w{},{} ``). Run `w/dat`` to view cached results.",
                center_x, center_y));
        g_donation_scan_running = false;
    }).detach();
}

void DatCommand::set_current_world(world_v2::World* world) {
    g_current_world = world;
    spdlog::info("DatCommand: World data set! {} tiles available", world ? world->tiles.size() : 0);
}

void DatCommand::set_core(core::Core* core) {
    g_core = core;
}

void DatCommand::ingest_donation_dialog(const std::string& dialog_content) {
    if (dialog_content.find("donation_box_edit") == std::string::npos &&
        dialog_content.find("Donation Box") == std::string::npos) {
        return;
    }

    auto extract_embed = [&](const std::string& key) -> uint32_t {
        const std::string marker = "embed_data|" + key + "|";
        size_t pos = dialog_content.find(marker);
        if (pos == std::string::npos) return 0;
        pos += marker.size();
        size_t end = pos;
        while (end < dialog_content.size() && std::isdigit(static_cast<unsigned char>(dialog_content[end]))) {
            ++end;
        }
        if (end == pos) return 0;
        return static_cast<uint32_t>(std::stoul(dialog_content.substr(pos, end - pos)));
    };

    const uint32_t tile_x = extract_embed("tilex");
    const uint32_t tile_y = extract_embed("tiley");
    if (tile_x == 0 && tile_y == 0) return;

    std::vector<std::string> gifts;
    size_t pos = 0;
    const std::string needle = "add_checkbox|checkbox|";
    while ((pos = dialog_content.find(needle, pos)) != std::string::npos) {
        const size_t start = pos + needle.size();
        size_t end = dialog_content.find('\n', start);
        if (end == std::string::npos) end = dialog_content.size();
        std::string line = dialog_content.substr(start, end - start);
        
        size_t state_sep = line.rfind('|');
        if (state_sep != std::string::npos) {
            bool all_digits = true;
            for (size_t i = state_sep + 1; i < line.size(); ++i) {
                if (!std::isdigit(static_cast<unsigned char>(line[i]))) {
                    all_digits = false;
                    break;
                }
            }
            if (all_digits) line = line.substr(0, state_sep);
        }
        if (!line.empty()) gifts.push_back(line);
        pos = end;
    }

    if (!gifts.empty()) {
        g_donation_dialog_cache[make_xy_key(tile_x, tile_y)] = gifts;
        spdlog::info("DatCommand: Cached {} donation entries for box at ({}, {})", gifts.size(), tile_x, tile_y);
    }

    
    
    {
        std::smatch m;
        std::regex re1(R"(You see `w([0-9]+)`` gifts in the box!)");
        std::regex re2(R"(You have `w([0-9]+)`` gift)");
        int count = -1;
        if (std::regex_search(dialog_content, m, re1) && m.size() > 1) {
            count = std::stoi(m[1].str());
        } else if (std::regex_search(dialog_content, m, re2) && m.size() > 1) {
            count = std::stoi(m[1].str());
        }
        if (count >= 0) {
            g_donation_count_cache[make_xy_key(tile_x, tile_y)] = count;
            spdlog::info("DatCommand: Cached donation count {} for box at ({}, {})", count, tile_x, tile_y);
        }
    }
}

bool DatCommand::is_special_item(uint16_t item_id) {
    return ITEM_TYPES.find(item_id) != ITEM_TYPES.end();
}

const char* DatCommand::get_item_type_name(uint16_t item_id, uint8_t extra_type) {
    auto it = ITEM_TYPES.find(item_id);
    if (it != ITEM_TYPES.end()) {
        return it->second;
    }
    
    
    if (extra_type == 1) return "Sign";
    if (extra_type == 2) return "Door";
    if (extra_type == 3) return "Lock";
    if (extra_type == 12) return "Donation Box";
    
    return "Unknown";
}

void DatCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    spdlog::info("=== DAT COMMAND EXECUTED ===");
    
    if (!client || !client->get_player()) {
        spdlog::error("DatCommand: No client or player!");
        return;
    }

    if (!g_core) {
        spdlog::error("DatCommand: No core set!");
        utils::PacketUtils::send_chat_message(client->get_player(), 
            "`4Error: Core not initialized!");
        return;
    }

    auto* server = g_core->get_server();
    if (!server || !server->get_player()) {
        spdlog::error("DatCommand: No server player!");
        utils::PacketUtils::send_chat_message(client->get_player(), 
            "`4Error: Server not available!");
        return;
    }

    
    if (!g_current_world || !g_current_world->is_valid) {
        spdlog::error("DatCommand: No valid world data!");
        utils::PacketUtils::send_chat_message(client->get_player(), 
            "`4Error: No world data! Enter/re-enter a world first.");
        return;
    }

    const auto& tiles = g_current_world->tiles;
    
    if (tiles.empty()) {
        spdlog::error("DatCommand: Tiles empty!");
        utils::PacketUtils::send_chat_message(client->get_player(), 
            "`4No tile data available!");
        return;
    }

    
    if (args.size() >= 2 && (args[1] == "scan" || args[1] == "autoscan")) {
        if (g_donation_scan_running.exchange(true)) {
            utils::PacketUtils::send_chat_message(client->get_player(),
                "`4Dat:`o donation auto-scan is already running.");
            return;
        }

        int radius = 10;
        int delay_ms = 220;
        if (args.size() >= 3) {
            try { radius = std::stoi(args[2]); } catch (...) {}
        }
        if (args.size() >= 4) {
            try { delay_ms = std::stoi(args[3]); } catch (...) {}
        }
        radius = std::clamp(radius, 1, 50);
        delay_ms = std::clamp(delay_ms, 80, 2000);

        int player_tile_x = 0;
        int player_tile_y = 0;
        if (!read_player_tile_position(g_core, player_tile_x, player_tile_y)) {
            g_donation_scan_running = false;
            utils::PacketUtils::send_chat_message(client->get_player(),
                "`4Dat:`o couldn't read your current position yet. Move once and try again.");
            return;
        }

        std::vector<std::pair<uint32_t, uint32_t>> targets;
        for (const auto& tile : tiles) {
            if (tile.fg != 1452 && tile.extra_type != 12) continue;
            const int dx = std::abs(static_cast<int>(tile.x) - player_tile_x);
            const int dy = std::abs(static_cast<int>(tile.y) - player_tile_y);
            if (dx <= radius && dy <= radius) {
                targets.emplace_back(tile.x, tile.y);
            }
        }

        if (targets.empty()) {
            g_donation_scan_running = false;
            utils::PacketUtils::send_chat_message(client->get_player(),
                fmt::format("`4Dat:`o no donation boxes found within radius `w{} ``.", radius));
            return;
        }

        auto* to_server_player = client->get_player();
        if (!to_server_player) {
            g_donation_scan_running = false;
            utils::PacketUtils::send_chat_message(client->get_player(),
                "`4Dat:`o unable to send scan packets (no client player).");
            return;
        }

        auto* local_player = g_core->get_server() ? g_core->get_server()->get_player() : nullptr;
        if (!local_player) {
            local_player = client->get_player();
        }

        start_donation_auto_scan(local_player, to_server_player, player_tile_x, player_tile_y,
            radius, delay_ms, std::move(targets));
        return;
    }

    spdlog::info("DatCommand: Scanning {} tiles for special items...", tiles.size());

    
    std::vector<const world_v2::Tile*> special_tiles;
    
    for (const auto& tile : tiles) {
        
        if (tile.has_extra() || is_special_item(tile.fg)) {
            special_tiles.push_back(&tile);
            spdlog::info("  Found {}: at ({}, {}) - extra_type: {}, fg: {}", 
                get_item_type_name(tile.fg, tile.extra_type), tile.x, tile.y, 
                tile.extra_type, tile.fg);
        }
    }

    spdlog::info("DatCommand: Found {} special tiles", special_tiles.size());

    if (special_tiles.empty()) {
        spdlog::warn("DatCommand: No special tiles found!");
        utils::PacketUtils::send_chat_message(client->get_player(), 
            "`2No doors, signs, or locks found in this world!");
        return;
    }

    
    show_data_gui(server->get_player(), special_tiles);

    spdlog::info("=== DAT COMMAND COMPLETED ===");
}

void DatCommand::show_data_gui(player::Player* player, const std::vector<const world_v2::Tile*>& tiles) {
    std::ostringstream dialog;
    dialog << "set_default_color|`o\n";
    dialog << "add_label_with_icon|big|`wWorld Tile Data``|left|546|\n";
    dialog << "add_spacer|small|\n";
    
    dialog << fmt::format("add_textbox|`2Found {} tiles with data``|left|\n", tiles.size());
    dialog << "add_spacer|small|\n";

    for (const auto* tile : tiles) {
        
        std::string item_name = get_item_type_name(tile->fg, tile->extra_type);
        
        
        if (tile->fg == 1452 || tile->extra_type == 12) {
            dialog << fmt::format("add_label_with_icon|small|`wDonation Box`` `0(ID: {})``|left|{}|\n",
                tile->fg, tile->fg);
        } else if (tile->extra_type == 1 || tile->extra_type == 2) {
            
            if (tile->fg == 12 || tile->fg == 546 || tile->fg == 1684) {
                dialog << fmt::format("add_label_with_icon|small|`wDoor: {}`` `0(ID: {})``|left|{}|\n", 
                    item_name, tile->fg, tile->fg);
            } else {
                dialog << fmt::format("add_label_with_icon|small|`wSign: {}`` `0(ID: {})``|left|{}|\n", 
                    item_name, tile->fg, tile->fg);
            }
        } else if (tile->is_lock()) {
            dialog << fmt::format("add_label_with_icon|small|`wLock: {}`` `0(ID: {})``|left|{}|\n", 
                item_name, tile->fg, tile->fg);
        } else {
            dialog << fmt::format("add_label_with_icon|small|`w{}`` `0(ID: {})``|left|{}|\n", 
                item_name, tile->fg, tile->fg);
        }
        
        
        dialog << fmt::format("add_smalltext|`oPosition: `5({}, {})``|\n", 
            tile->x, tile->y);
        
        
        if (tile->has_extra()) {
            dialog << fmt::format("add_smalltext|`bExtra Type: `w{}``|\n", tile->extra_type);
            
            
            if (tile->door_data.has_data()) {
                dialog << fmt::format("add_smalltext|`c✓ Door ID / Text: `w{}``|\n", 
                    tile->door_data.destination);
            }

            
            if (tile->fg == 1452 || tile->extra_type == 12) {
                auto it_count = g_donation_count_cache.find(make_xy_key(tile->x, tile->y));
                if (it_count != g_donation_count_cache.end()) {
                    dialog << fmt::format("add_smalltext|`2Visible Gift Count: `w{} ``|\n", it_count->second);
                }

                auto it_cache = g_donation_dialog_cache.find(make_xy_key(tile->x, tile->y));
                if (it_cache != g_donation_dialog_cache.end() && !it_cache->second.empty()) {
                    dialog << fmt::format("add_smalltext|`2Cached Gifts: `w{} entries``|\n", it_cache->second.size());
                    for (const auto& entry : it_cache->second) {
                        dialog << fmt::format("add_smalltext|`6- `w{}``|\n", entry);
                    }
                }

                if (tile->donation_data.has_data()) {
                    dialog << fmt::format("add_smalltext|`6Donation Extra 1: `w{}``|\n",
                        tile->donation_data.entry1.empty() ? "(empty)" : tile->donation_data.entry1);
                    dialog << fmt::format("add_smalltext|`6Donation Extra 2: `w{}``|\n",
                        tile->donation_data.entry2.empty() ? "(empty)" : tile->donation_data.entry2);
                    dialog << fmt::format("add_smalltext|`6Donation Extra 3: `w{}``|\n",
                        tile->donation_data.entry3.empty() ? "(empty)" : tile->donation_data.entry3);
                    dialog << fmt::format("add_smalltext|`6Donation Tail Flag: `w{}``|\n",
                        static_cast<int>(tile->donation_data.tail_flag));

                    if (!tile->donation_data.raw_extra_bytes.empty()) {
                        dialog << fmt::format("add_smalltext|`6Raw Extra Bytes: `w{} bytes``|\n",
                            tile->donation_data.raw_extra_bytes.size());
                        dialog << fmt::format("add_smalltext|`6Raw Hex Preview: `w{} ``|\n",
                            bytes_to_hex_preview(tile->donation_data.raw_extra_bytes));
                    }
                } else if (tile->door_data.has_data()) {
                    
                    dialog << fmt::format("add_smalltext|`6Donation Raw Extra: `w{}``|\n", tile->door_data.destination);
                } else {
                    dialog << "add_smalltext|`6Donation Raw Extra: `o(not present in map extra for this box)``|\n";
                }
            }
            
            
            if (tile->is_lock() && tile->lock_data.has_data()) {
                dialog << fmt::format("add_smalltext|`4Lock Owner UID: `w{}``|\n", 
                    tile->lock_data.owner_uid);
                dialog << fmt::format("add_smalltext|`4Admins: `w{}``|\n", 
                    tile->lock_data.access_list.size());
            }
            
            
            if (tile->is_vending() && tile->vending_data.has_data()) {
                dialog << fmt::format("add_smalltext|`3Vending Item: `w{}``|\n", 
                    tile->vending_data.item_id);
                dialog << fmt::format("add_smalltext|`3Price: `w{}``|\n", 
                    tile->vending_data.get_price());
            }
        }
        
        dialog << "add_spacer|small|\n";
    }

    dialog << "add_quick_exit|\n";
    dialog << "end_dialog|dat_info|Close||\n";

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
    
    spdlog::info("DatCommand: Sent tile data GUI");
}

} 
