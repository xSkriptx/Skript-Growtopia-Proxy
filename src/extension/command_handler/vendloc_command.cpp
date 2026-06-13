#include "vendloc_command.hpp"
#include "../../utils/strenc.hpp"
#include "../../client/client.hpp"
#include "../../player/player.hpp"
#include "../../server/server.hpp"
#include "../../core/core.hpp"
#include "../../utils/packet_utils.hpp"
#include "../../utils/text_parse.hpp"
#include "../../utils/world_manager.hpp"
#include "../../utils/player_tracker.hpp"
#include "../../packet/packet_variant.hpp"
#include "../../packet/packet_types.hpp"
#include "../../utils/byte_stream.hpp"
#include "../../utils/api_client.hpp"
#include "../item_finder/item_finder.hpp"
#include <spdlog/spdlog.h>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <thread>

namespace command {

static core::Core* g_core = nullptr;
static extension::item_finder::ItemDatabase* g_item_database = nullptr;
static std::vector<VendingEntry> g_last_world_vends;
static std::string g_last_world_name;
static std::unordered_map<std::string, int64_t> g_last_uploaded_signature_by_pos;

static std::string vend_pos_key(const std::string& world_name, uint32_t x, uint32_t y) {
    return world_name + "|" + std::to_string(x) + "|" + std::to_string(y);
}

static int64_t vend_signature(const VendingEntry& entry) {
    const int64_t signed_price = entry.is_wl_price ? -static_cast<int64_t>(entry.price) : static_cast<int64_t>(entry.price);
    return (static_cast<int64_t>(entry.item_id) << 32) ^
           (signed_price << 1) ^
           static_cast<int64_t>(entry.tile_id);
}

static void send_overlay(player::Player* player, const std::string& msg) {
    if (!player) return;
    packet::Variant var{};
    var.add("OnTextOverlay");
    var.add(msg);

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
    player->send_packet(bs.get_data(), 0);
}

static void send_particle(player::Player* player, int effect_id, float x, float y) {
    if (!player) return;
    packet::Variant var{};
    var.add("OnParticleEffect");
    var.add((uint32_t)effect_id);
    var.add(glm::vec2(x, y));
    var.add((uint32_t)0);
    var.add((uint32_t)0);

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
    player->send_packet(bs.get_data(), 0);
}

static void pulse_vend_marker(const std::string& target_world, uint32_t x, uint32_t y,
                              int repeats = 6, int delay_ms = 280, bool until_reached = false,
                              uint32_t expected_item_id = 0, bool warp_overlay_mode = false) {
    if (!g_core) return;
    std::thread([target_world, x, y, repeats, delay_ms, until_reached, expected_item_id, warp_overlay_mode]() {
        auto lower = [](std::string s) {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return s;
        };
        const std::string target_world_l = lower(target_world);

        std::this_thread::sleep_for(std::chrono::milliseconds(1200));
        if (!g_core || !g_core->get_server() || !g_core->get_server()->get_player()) return;

        auto* out = g_core->get_server()->get_player();
        auto& world_mgr = utils::WorldManager::get_instance();
        auto& tracker = utils::PlayerTracker::get_instance();

        if (!target_world.empty()) {
            constexpr int kWaitWorldMs = 15000;
            constexpr int kWaitStepMs = 200;
            int waited = 0;
            while (waited < kWaitWorldMs) {
                const std::string cur_world = world_mgr.get_world_name();
                if (!cur_world.empty() && lower(cur_world) == target_world_l && 
                    world_mgr.has_world() && tracker.has_local_player()) {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(kWaitStepMs));
                waited += kWaitStepMs;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        }

        if (warp_overlay_mode && expected_item_id != 0) {
            constexpr int kWaitVendCacheMs = 5000;
            constexpr int kVendStepMs = 100;
            int waited_cache = 0;
            while (waited_cache < kWaitVendCacheMs) {
                if (!g_last_world_name.empty() && lower(g_last_world_name) == target_world_l) {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(kVendStepMs));
                waited_cache += kVendStepMs;
            }

            const bool world_cache_ready = (!g_last_world_name.empty() && lower(g_last_world_name) == target_world_l);
            bool sold_confirmed = false;
            if (world_cache_ready) {
                bool found_vend = false;
                for (const auto& v : g_last_world_vends) {
                    if (lower(v.world_name) != target_world_l) continue;
                    if (v.x == x && v.y == y) {
                        found_vend = true;
                        if (v.item_id != expected_item_id) {
                            sold_confirmed = true;
                        }
                        break;
                    }
                }
                if (!found_vend) {
                    sold_confirmed = true;
                }
            }

            if (sold_confirmed) {
                send_overlay(out, "`4Item sold");
                return;
            }
            send_overlay(out, "`2Item highlighted");
        }

        const float px = static_cast<float>(x) * 32.0f + 16.0f;
        const float py = static_cast<float>(y) * 32.0f + 16.0f;
        auto has_reached_target = [&]() -> bool {
            auto& tracker = utils::PlayerTracker::get_instance();
            auto local = tracker.get_local_player();
            if (local.netID <= 0) return false;
            auto pos = tracker.get_player_position(local.netID);
            const int tx = static_cast<int>(pos.x / 32.0f);
            const int ty = static_cast<int>(pos.y / 32.0f);
            return (tx == static_cast<int>(x) && ty == static_cast<int>(y));
        };

        if (!until_reached) {
            for (int i = 0; i < repeats; ++i) {
                send_particle(out, 88, px, py);
                std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            }
            return;
        }

        constexpr int kMaxPulses = 900;
        for (int i = 0; i < kMaxPulses; ++i) {
            if (!target_world.empty()) {
                const std::string live_world = utils::WorldManager::get_instance().get_world_name();
                if (!live_world.empty() && lower(live_world) != target_world_l) {
                    return;
                }
            }
            if (has_reached_target()) {
                return;
            }
            send_particle(out, 88, px, py);
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        }
    }).detach();
}

VendLocCommand::VendLocCommand() : CommandBase(
    {"vendf", "vendloc"},
    {},
    "Search for items in vending machines",
    0
) {}

std::unique_ptr<CommandBase> VendLocCommand::clone() const {
    return std::make_unique<VendLocCommand>();
}

void VendLocCommand::set_core(core::Core* core) {
    g_core = core;
}

void VendLocCommand::set_item_database(extension::item_finder::ItemDatabase* database) {
    g_item_database = database;
    spdlog::trace("VendLocCommand: Item database set");
}

void VendLocCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    spdlog::info("=== VENDLOC COMMAND EXECUTED ===");
    
    if (!client || !client->get_player()) {
        spdlog::error("VendLocCommand: No client or player!");
        return;
    }

    if (!g_core) {
        spdlog::error("VendLocCommand: No core set!");
        return;
    }

    auto* server = g_core->get_server();
    if (!server || !server->get_player()) {
        spdlog::error("VendLocCommand: No server player!");
        return;
    }

    VendLocCommand cmd;
    cmd.show_search_gui(server->get_player(), "");
}

void VendLocCommand::save_world_vendings(const world_v2::World& world) {
    if (!world.is_valid || world.tiles.empty()) {
        return;
    }

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    std::string timestamp = ss.str();

    std::unordered_map<std::string, VendingEntry> by_pos;
    
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
            by_pos[vend_pos_key(entry.world_name, entry.x, entry.y)] = entry;
        }
    }

    std::vector<VendingEntry> entries;
    entries.reserve(by_pos.size());
    for (const auto& kv : by_pos) {
        entries.push_back(kv.second);
    }

    if (!entries.empty()) {
        spdlog::info("VendLocCommand: Saving {} vendings from world '{}'", entries.size(), world.name);
        g_last_world_name = world.name;
        g_last_world_vends = entries;
        append_to_file(entries);
        upload_to_api(entries);
    } else {
        g_last_world_name = world.name;
        g_last_world_vends.clear();
    }
}

void VendLocCommand::append_to_file(const std::vector<VendingEntry>& entries) {
    try {
        std::string fname = DEC("vd.dat");
        std::ofstream file(fname, std::ios::app);
        if (!file.is_open()) return;

        for (const auto& entry : entries) {
            std::string price_str = entry.is_wl_price 
                ? fmt::format("{}/1 WL", entry.price)
                : fmt::format("{} WL", entry.price);

            file << fmt::format("[{}] {}|{}|{}|{}|{}|{}\n",
                entry.timestamp, entry.world_name, entry.get_type(),
                entry.x, entry.y, entry.item_id, price_str);
        }
        file.close();
    } catch (const std::exception& e) {
        spdlog::error("VendLocCommand: Failed to append: {}", e.what());
    }
}

void VendLocCommand::upload_to_api(const std::vector<VendingEntry>& entries) {
}

void VendLocCommand::show_search_gui(player::Player* player, const std::string& query) {
    if (!player) return;

    if (!g_item_database) {
        utils::PacketUtils::send_chat_message(player, "`4Item database not available!");
        return;
    }

    try {
        std::ostringstream dialog;
        dialog << "set_default_color|`o\n";
        dialog << "add_label_with_icon|big|`wVending Locator``|left|2978|\n";
        dialog << "add_spacer|small|\n";
        dialog << "add_text_input|vend_search|Search:|" << query << "|30|\n";
        dialog << "add_spacer|small|\n";
        
        size_t result_count = 0;
        
        if (!query.empty()) {
            auto results = g_item_database->search_items(query, 20, "all");
            result_count = results.size();
            
            if (results.empty()) {
                dialog << "add_textbox|`4No items found for: `w" << query << "``|\n";
                dialog << "add_spacer|small|\n";
            } else {
                dialog << "add_textbox|`2Found " << results.size() << " items``|\n";
                dialog << "add_spacer|small|\n";
                
                for (const auto& item : results) {
                    dialog << "add_label_with_icon|big|`w" << item.name << "``|left|" << item.id << "|\n";
                    dialog << "add_smalltext|`oID: " << item.id << "``|\n";
                    dialog << "add_button|viewworlds_" << item.id << "|`2View Worlds``|\n";
                    dialog << "add_spacer|small|\n";
                }
                
                if (results.size() >= 20) {
                    dialog << "add_textbox|`4Showing first 20 results. Refine your search.``|\n";
                    dialog << "add_spacer|small|\n";
                }
            }
        } else {
            dialog << "add_textbox|`oEnter an item name or ID to search``|\n";
            dialog << "add_textbox|`oExample: party, dragon, 2306``|\n";
        }
        
        dialog << "add_spacer|small|\n";
        dialog << "end_dialog|vendloc_search|Close|Search|\n";

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

        spdlog::info("VendLocCommand: Sent search GUI with {} results", result_count);

    } catch (const std::exception& e) {
        spdlog::error("VendLocCommand: Failed to send GUI: {}", e.what());
    }
}

std::vector<VendingEntry> VendLocCommand::get_vendings_for_item(uint32_t item_id) {
    std::vector<VendingEntry> results;
    std::unordered_map<std::string, VendingEntry> latest_entries;

    try {
        std::ifstream file("vd.dat");
        if (!file.is_open()) return results;

        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] != '[') continue;
            size_t close_bracket = line.find(']');
            if (close_bracket == std::string::npos) continue;

            std::string timestamp = line.substr(1, close_bracket - 1);
            std::string rest = line.substr(close_bracket + 2);

            std::vector<std::string> parts;
            std::stringstream ss(rest);
            std::string item;
            while (std::getline(ss, item, '|')) {
                parts.push_back(item);
            }

            if (parts.size() < 6) continue;

            std::string world_name = parts[0];
            std::string type = parts[1];
            std::string x_str = parts[2];
            std::string y_str = parts[3];
            std::string item_id_str = parts[4];
            std::string price_str = parts[5];

            while (!price_str.empty() && std::isspace(static_cast<unsigned char>(price_str.back()))) {
                price_str.pop_back();
            }

            uint32_t parsed_item_id = 0;
            try {
                parsed_item_id = std::stoul(item_id_str);
            } catch (...) {
                continue;
            }

            if (parsed_item_id == item_id) {
                bool is_wl_price = false;
                int price = 0;
                if (price_str.find("/1 WL") != std::string::npos) {
                    is_wl_price = true;
                    try {
                        price = std::stoi(price_str.substr(0, price_str.find("/1 WL")));
                    } catch (...) {}
                } else if (price_str.find(" WL") != std::string::npos) {
                    is_wl_price = false;
                    try {
                        price = std::stoi(price_str.substr(0, price_str.find(" WL")));
                    } catch (...) {}
                } else {
                    try {
                        price = std::stoi(price_str);
                    } catch (...) {}
                }

                uint32_t x = 0, y = 0;
                try {
                    x = std::stoul(x_str);
                    y = std::stoul(y_str);
                } catch (...) {
                    continue;
                }

                VendingEntry entry;
                entry.world_name = world_name;
                entry.timestamp = timestamp;
                entry.x = x;
                entry.y = y;
                entry.item_id = parsed_item_id;
                entry.tile_id = (type == "Mega Vending") ? 9268 : 2978;
                entry.is_wl_price = is_wl_price;
                entry.price = price;

                std::string key = vend_pos_key(world_name, x, y);
                latest_entries[key] = entry;
            }
        }
        file.close();

        for (const auto& pair : latest_entries) {
            results.push_back(pair.second);
        }
    } catch (const std::exception& e) {
        spdlog::error("VendLocCommand: Error reading local vd.dat: {}", e.what());
    }

    return results;
}

void VendLocCommand::show_vending_locations(player::Player* player, uint32_t item_id,
                                           const std::string& item_name,
                                           const std::vector<VendingEntry>& entries) {
    if (!player) return;

    try {
        std::ostringstream dialog;
        dialog << "set_default_color|`o\n";
        dialog << fmt::format("add_label_with_icon|big|`w{} - Worlds``|left|{}|\n", item_name, item_id);
        dialog << "add_spacer|small|\n";
        
        if (entries.empty()) {
            dialog << "add_textbox|`4No vendings found for this item``|left|\n";
            dialog << "add_spacer|small|\n";
        } else {
            dialog << fmt::format("add_textbox|`2Found {} vendings``|left|\n", entries.size());
            dialog << "add_spacer|small|\n";

            for (const auto& entry : entries) {
                std::string price_str = entry.is_wl_price 
                    ? fmt::format("{}/1 WL", entry.price)
                    : fmt::format("{} WL", entry.price);

                std::string button_id = fmt::format("warp_{}_{}_{}_{}", entry.world_name, entry.x, entry.y, entry.item_id);
                
                dialog << fmt::format("add_button|{}|`5WARP``|noflags|0|0|\n", button_id);
                dialog << fmt::format("add_smalltext|`w{} `5({}, {}) - `2{}``|\n",
                    entry.world_name, entry.x, entry.y, price_str);
                dialog << fmt::format("add_smalltext|`o{} | Last seen: {}``|\n", 
                    entry.get_type(), entry.timestamp);
                    
                dialog << "add_spacer|small|\n";
            }
        }

        dialog << "add_quick_exit|\n";
        dialog << "end_dialog|vendloc_locations|Close||";

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

    } catch (const std::exception& e) {
        spdlog::error("VendLocCommand: Failed to send locations: {}", e.what());
    }
}

void VendLocCommand::handle_dialog_return(player::Player* player, const std::string& query) {
    if (!player || !g_core) return;

    auto* server = g_core->get_server();
    if (!server || !server->get_player()) return;

    spdlog::info("VendLocCommand: Search query: '{}'", query);

    VendLocCommand cmd;
    cmd.show_search_gui(server->get_player(), query);
}

void VendLocCommand::handle_dialog_click(player::Player* player, const std::string& button) {
    if (!player || !g_core) return;

    auto* server = g_core->get_server();
    if (!server || !server->get_player()) return;

    if (button.rfind("viewworlds_", 0) == 0) {
        std::string item_id_str = button.substr(11);
        uint32_t item_id = std::stoul(item_id_str);
        
        std::string item_name = "Item " + item_id_str;
        if (g_item_database) {
            auto search_results = g_item_database->search_items(item_id_str, 1, "all");
            if (!search_results.empty() && search_results[0].id == static_cast<int>(item_id)) {
                item_name = search_results[0].name;
            }
        }

        auto entries = get_vendings_for_item(item_id);
        
        VendLocCommand cmd;
        cmd.show_vending_locations(server->get_player(), item_id, item_name, entries);
    }
    else if (button.rfind("warp_", 0) == 0) {
        std::string button_data = button.substr(5);
        
        size_t third_last_underscore = std::string::npos;
        size_t last_underscore = button_data.rfind('_');
        size_t second_last_underscore = button_data.rfind('_', last_underscore - 1);
        if (second_last_underscore != std::string::npos) {
            third_last_underscore = button_data.rfind('_', second_last_underscore - 1);
        }
        
        if (last_underscore == std::string::npos || second_last_underscore == std::string::npos) {
            return;
        }

        std::string world_name;
        uint32_t vend_x = 0;
        uint32_t vend_y = 0;
        uint32_t expected_item_id = 0;
        bool vend_pos_ok = false;
        try {
            if (third_last_underscore != std::string::npos) {
                world_name = button_data.substr(0, third_last_underscore);
                vend_x = static_cast<uint32_t>(std::stoul(button_data.substr(third_last_underscore + 1, second_last_underscore - third_last_underscore - 1)));
                vend_y = static_cast<uint32_t>(std::stoul(button_data.substr(second_last_underscore + 1, last_underscore - second_last_underscore - 1)));
                expected_item_id = static_cast<uint32_t>(std::stoul(button_data.substr(last_underscore + 1)));
            } else {
                world_name = button_data.substr(0, second_last_underscore);
                vend_x = static_cast<uint32_t>(std::stoul(button_data.substr(second_last_underscore + 1, last_underscore - second_last_underscore - 1)));
                vend_y = static_cast<uint32_t>(std::stoul(button_data.substr(last_underscore + 1)));
            }
            vend_pos_ok = true;
        } catch (...) {
            world_name.clear();
            vend_x = 0;
            vend_y = 0;
            expected_item_id = 0;
            vend_pos_ok = false;
        }
        if (world_name.empty()) return;
        
        spdlog::info("VendLocCommand: Warping to world '{}'", world_name);
        
        TextParse text_parse{};
        text_parse.add("action", {"join_request"});
        text_parse.add("name", {world_name});
        text_parse.add("invitedWorld", {"0"});

        ByteStream<std::uint16_t> byte_stream{};
        byte_stream.write(packet::NET_MESSAGE_GAME_MESSAGE);
        byte_stream.write(text_parse.get_raw(), false);

        auto* game_client = g_core->get_client();
        if (!game_client || !game_client->get_player()) {
            utils::PacketUtils::send_chat_message(player, "`4VendLoc warp failed: client route unavailable.");
            spdlog::error("VendLocCommand: Warp failed - no client player route");
            return;
        }
        game_client->get_player()->send_packet(byte_stream.get_data(), 0);
        
        utils::PacketUtils::send_chat_message(player, 
            fmt::format("`2Warping to {}...", world_name));
        if (vend_pos_ok) {
            pulse_vend_marker(world_name, vend_x, vend_y, 6, 280, true, expected_item_id, true);
        }
        
        spdlog::info("VendLocCommand: Warp packet sent to '{}'", world_name);
    }
}

VendTPCommand::VendTPCommand() : CommandBase(
    {"vendtp"},
    {"item_name"},
    "Highlight matching vends in current world with particle effects",
    1
) {}

std::unique_ptr<CommandBase> VendTPCommand::clone() const {
    return std::make_unique<VendTPCommand>();
}

void VendTPCommand::set_core(core::Core* core) {
    g_core = core;
}

void VendTPCommand::set_item_database(extension::item_finder::ItemDatabase* database) {
    g_item_database = database;
}

void VendTPCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    if (!client || !client->get_player() || !g_core || !g_core->get_server() || !g_core->get_server()->get_player()) {
        return;
    }

    auto* out = g_core->get_server()->get_player();

    if (args.size() < 2) {
        utils::PacketUtils::send_chat_message(out, "`4Usage: /vendtp <item_name_or_id>");
        return;
    }

    std::string query;
    for (size_t i = 1; i < args.size(); ++i) {
        if (!query.empty()) query += " ";
        query += args[i];
    }

    std::unordered_set<uint32_t> target_ids;
    try {
        if (!query.empty() && std::all_of(query.begin(), query.end(), [](unsigned char c) { return std::isdigit(c) != 0; })) {
            target_ids.insert(static_cast<uint32_t>(std::stoul(query)));
        }
    } catch (...) {
    }

    if (g_item_database) {
        auto results = g_item_database->search_items(query, 50, "all");
        for (const auto& item : results) {
            if (item.id >= 0) target_ids.insert(static_cast<uint32_t>(item.id));
        }
    }

    if (target_ids.empty()) {
        utils::PacketUtils::send_chat_message(out, fmt::format("`4VendTP: no item ID found for '{}'", query));
        return;
    }

    const std::string cur_world = utils::WorldManager::get_instance().get_world_name();
    std::vector<VendingEntry> matches;
    for (const auto& v : g_last_world_vends) {
        if (!cur_world.empty() && !v.world_name.empty() && v.world_name != cur_world) continue;
        if (target_ids.find(v.item_id) != target_ids.end()) {
            matches.push_back(v);
        }
    }

    if (matches.empty()) {
        utils::PacketUtils::send_chat_message(out, fmt::format("`4VendTP: no matching vends in current world for '{}'", query));
        return;
    }

    for (const auto& v : matches) {
        pulse_vend_marker(cur_world, v.x, v.y, 8, 260, true);
    }

    utils::PacketUtils::send_chat_message(out, fmt::format("`2VendTP:`o highlighted {} vend(s) for `w{} ``", matches.size(), query));
}

}
