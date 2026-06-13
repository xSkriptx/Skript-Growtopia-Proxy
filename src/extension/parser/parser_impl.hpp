#pragma once
#include "parser.hpp"
#include "../../core/core.hpp"
#include "../../server/server.hpp"
#include "../../utils/player_tracker.hpp"
#include "../../utils/packet_utils.hpp"
#include "../../utils/display_manager.hpp"
#include "../../utils/vend_logs_store.hpp"
#include "../../utils/api_client.hpp"
#include "../../utils/text_parse.hpp"
#include "../../packet/tank_packet.hpp"
#include "../../proxy_imgui_gui.hpp"
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <regex>
#include <unordered_set>
#include <unordered_map>
#include "../../utils/inventory_manager.hpp"
#include "../command_handler/dat_command.hpp"
#include "../command_handler/dropall_command.hpp"
#include "../command_handler/dropfast_command.hpp"
#include "../command_handler/trashfast_command.hpp"
#include "../command_handler/drop_currency_command.hpp"
#include "../command_handler/utility_commands.hpp"  

namespace extension::parser {


static const char* get_packet_type_name(uint8_t type) {
    switch (type) {
        case packet::PACKET_STATE: return "STATE";
        case packet::PACKET_CALL_FUNCTION: return "CALL_FUNCTION";
        case packet::PACKET_UPDATE_STATUS: return "UPDATE_STATUS";
        case packet::PACKET_TILE_CHANGE_REQUEST: return "TILE_CHANGE_REQUEST";
        case packet::PACKET_SEND_MAP_DATA: return "SEND_MAP_DATA";
        case packet::PACKET_SEND_TILE_UPDATE_DATA: return "SEND_TILE_UPDATE_DATA";
        case packet::PACKET_SEND_INVENTORY_STATE: return "SEND_INVENTORY_STATE";
        case packet::PACKET_ITEM_CHANGE_OBJECT: return "ITEM_CHANGE_OBJECT";
        case packet::PACKET_MODIFY_ITEM_INVENTORY: return "MODIFY_ITEM_INVENTORY";
        case packet::PACKET_SET_CHARACTER_STATE: return "SET_CHARACTER_STATE";
        case packet::PACKET_PING_REPLY: return "PING_REPLY";
        case packet::PACKET_PING_REQUEST: return "PING_REQUEST";
        case packet::PACKET_GOT_PUNCHED: return "GOT_PUNCHED";
        default: return "UNKNOWN";
    }
}

class ParserExtension final : public IParserExtension {
    core::Core* core_;
    EventDispatcher event_dispatcher_;
    mutable std::uint32_t tick_counter_ = 0;
    bool pending_balance_on_inventory_ = false;
    std::unordered_set<std::string> auto_ignored_names_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> recent_real_spins_;
    std::unordered_map<int, std::chrono::steady_clock::time_point> recent_real_spin_values_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> recent_fake_spins_;
    std::unordered_map<int, std::chrono::steady_clock::time_point> recent_fake_spin_values_;
    std::unordered_map<int, std::chrono::steady_clock::time_point> recent_playerchat_spin_values_;
    std::unordered_map<uint32_t, int> last_spin_by_netid_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> pending_instant_spin_keys_;
    std::unordered_map<int, std::chrono::steady_clock::time_point> pending_instant_spin_values_;
    std::unordered_map<int, std::chrono::steady_clock::time_point> pending_fake_override_spin_values_;

    
    
    struct DeferredBubble {
        std::string normalized_name; 
        std::string text;
        std::string spin_key;
        int spin_value = -1;
        std::chrono::steady_clock::time_point queued_at;
    };
    std::vector<DeferredBubble> deferred_bubbles_;

public:
    explicit ParserExtension(core::Core* core)
        : core_{ core }
    {
    }

    ~ParserExtension() override = default;

    void init() override {
        
        core_->get_event_dispatcher().prependListener(
            core::EventType::Tick,
            [this](const core::EventTick& event) {
                tick_counter_++;
                
                if (core_->get_config().get<bool>("display.show_ping") && (tick_counter_ % 60 == 0)) {
                    utils::DisplayManager::update_display(core_);
                }
            }
        );
        
        core_->get_event_dispatcher().prependListener(
            core::EventType::Message,
            [this](const core::EventMessage& event) {
                
                {
                    const char* dir = (event.from == core::EventFrom::FromClient)
                        ? "C\xe2\x86\x92S" : "S\xe2\x86\x92" "C";
                    std::string msg = event.get_message().get_raw();
                    if (msg.size() > 120) msg = msg.substr(0, 120) + "\xe2\x80\xa6";
                    
                    for (auto& c : msg) if (c == '\n') c = ' ';
                    AppendPacket(dir, "[TEXT] " + msg);
                }
                if (event.from == core::EventFrom::FromClient) {
                    parse_connection_info(event);
                }

                handle_chat_message(event);
            }
        );

        core_->get_event_dispatcher().prependListener(
            core::EventType::Packet,
            [this](const core::EventPacket& event) {
                
                {
                    const auto& pkt = event.get_packet();
                    const char* dir = (event.from == core::EventFrom::FromClient)
                        ? "C\xe2\x86\x92S" : "S\xe2\x86\x92" "C";
                    std::string data = fmt::format("type={} net_id={}",
                        get_packet_type_name(static_cast<uint8_t>(pkt.type)),
                        pkt.net_id);
                    AppendPacket(dir, data);
                }
                
                handle_server_messages(event);

                
                if (event.from == core::EventFrom::FromServer && event.get_packet().type == packet::PACKET_SEND_INVENTORY_STATE && pending_balance_on_inventory_) {
                    auto& inv_mgr = utils::InventoryManager::get_instance();
                    int wl = inv_mgr.get_item_count(242);
                    int dl = inv_mgr.get_item_count(1796);
                    int bgl = inv_mgr.get_item_count(7188);
                    int total_wl = wl + dl * 100 + bgl * 10000;
                    std::string balance_msg = fmt::format("`2Balance: `w{} ā ({} DL, {} BGL))", total_wl, dl, bgl);
                    utils::PacketUtils::send_chat_message(const_cast<player::Player*>(&event.get_player()), balance_msg, false);
                    spdlog::info("[BALANCE-CONSOLE] Sent balance message to chat: {} WL ({} DL, {} BGL)", total_wl, dl, bgl);
                    pending_balance_on_inventory_ = false;
                }

                
                track_player_positions(event);

                
                handle_game_features(event);

                
                try {
                    switch (event.get_packet().type) {
                    case packet::PacketType::PACKET_CALL_FUNCTION:
                        parse_call_function(event);
                        break;
                    case packet::PacketType::PACKET_STATE:
                        
                        break;
                    default:
                        break;
                    }
                } catch (const std::exception& e) {
                    spdlog::error("Error in packet parser: {}", e.what());
                    
                }
            }
        );
    }

    void free() override {
        delete this;
    }

    EventDispatcher& get_event_dispatcher() override {
        return event_dispatcher_;
    }

private:
    void parse_connection_info(const core::EventMessage& event) {
        if (event.from != core::EventFrom::FromClient) {
            return;
        }

        const TextParse& text_parse = event.get_message();
        
        
        std::string user_id = text_parse.get("user");
        std::string country = text_parse.get("country");
        std::string mac_address = text_parse.get("mac");
        std::string player_name = text_parse.get("name", 1);
        std::string platform_id = text_parse.get("platformID");

        
        if (!user_id.empty() || !country.empty() || !mac_address.empty()) {
            spdlog::info("=== PLAYER CONNECTION DETAILS ===");
            spdlog::info("UserID: {}", user_id.empty() ? "Unknown" : user_id);
            spdlog::info("Country: {}", country.empty() ? "Unknown" : country);
            spdlog::info("MAC Address: {}", mac_address.empty() ? "Unknown" : mac_address);
            spdlog::info("PlatformID: {}", platform_id.empty() ? "Unknown" : platform_id);
            if (!player_name.empty()) {
                spdlog::info("Player Name: {}", player_name);
            }
            spdlog::info("================================");
            
            
            if (event.get_player().is_connected()) {
                
                if (!mac_address.empty()) {
                    std::string mac_msg = fmt::format(
                        "`2MAC: `9{}",
                        mac_address
                    );
                    utils::PacketUtils::send_chat_message(
                        const_cast<player::Player*>(&event.get_player()), 
                        mac_msg,
                        false
                    );
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }

                
                if (!user_id.empty()) {
                    std::string user_msg = fmt::format(
                        "`2UserID: `9{}",
                        user_id
                    );
                    utils::PacketUtils::send_chat_message(
                        const_cast<player::Player*>(&event.get_player()), 
                        user_msg,
                        false
                    );
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }

                
                auto& inv_mgr = utils::InventoryManager::get_instance();
                int wl = inv_mgr.get_item_count(242);
                int dl = inv_mgr.get_item_count(1796);
                int bgl = inv_mgr.get_item_count(7188);
                int total_wl = wl + dl * 100 + bgl * 10000;
                
                
                std::string balance_msg = fmt::format("`2Balance: `w{} ā ({} DL, {} BGL))", total_wl, dl, bgl);
                utils::PacketUtils::send_chat_message(
                    const_cast<player::Player*>(&event.get_player()),
                    balance_msg,
                    false
                );
                spdlog::info("[BALANCE-CONSOLE] Sent balance message to chat: {} WL ({} DL, {} BGL)", total_wl, dl, bgl);
            }

            
            if (!user_id.empty()) {
                uint32_t net_id = event.get_player().get_peer()->connectID;
                try {
                    uint32_t user_id_int = std::stoul(user_id);
                    
                    utils::PlayerTracker::get_instance().update_player_info(
                        net_id, 
                        user_id_int, 
                        player_name.empty() ? "Unknown" : player_name,
                        country.empty() ? "us" : country,
                        true 
                    );

                    
                    utils::PlayerTracker::get_instance().update_connection_info(
                        net_id, mac_address, country
                    );
                    if (!platform_id.empty()) {
                        utils::PlayerTracker::get_instance().update_platform_info(
                            net_id, platform_id
                        );
                    }
                    
                    
                    if (core_) {
                        std::string saved_name = core_->get_config().get<std::string>("display.visual_name");
                        bool show_ping = core_->get_config().get<bool>("display.show_ping");
                        bool has_g4g = core_->get_config().get<bool>("display.title.g4g");
                        bool has_maxlevel = core_->get_config().get<bool>("display.title.maxlevel");
                        bool has_dr = core_->get_config().get<bool>("display.title.dr");
                        bool has_mentor = core_->get_config().get<bool>("display.title.mentor");
                        bool has_title = has_g4g || has_maxlevel || has_dr || has_mentor;
                        
                        spdlog::info("Display settings check: name='{}', ping={}, g4g={}, maxlevel={}, dr={}, mentor={}", 
                                    saved_name, show_ping, has_g4g, has_maxlevel, has_dr, has_mentor);
                        
                        
                        if (!saved_name.empty() || show_ping || has_title) {
                            spdlog::info("Auto-applying saved display settings for {} (netID: {})", player_name, net_id);
                            utils::DisplayManager::apply_display_name(
                                core_, 
                                net_id, 
                                player_name.empty() ? "Unknown" : player_name
                            );
                            spdlog::info("Display settings applied successfully");
                        } else {
                            spdlog::info("No saved display settings to apply");
                        }
                    }
                } catch (const std::exception& e) {
                    spdlog::warn("Failed to parse user ID: {}", e.what());
                }
            }
        }
    }


    void handle_server_messages(const core::EventPacket& event) {
        if (event.from != core::EventFrom::FromServer) {
            return;
        }

        const auto& ext_data = event.get_ext_data();
        if (ext_data.empty()) {
            return;
        }

        packet::Variant variant{};
        if (!variant.deserialize(ext_data)) {
            return;
        }

        std::vector variants{ variant.get_variants() };
        if (variants.empty()) {
            return;
        }

        try {
            if (variant.size() > 0) {
                std::string function_name = variant.get<std::string>(0);

                auto host_flag = [&](const std::string& key) -> bool {
                    try { return core_->get_config().get<bool>(key); }
                    catch (...) { return false; }
                };
                auto strip_gt_codes_local = [](const std::string& s) -> std::string {
                    std::string out;
                    out.reserve(s.size());
                    for (size_t i = 0; i < s.size(); ++i) {
                        if (s[i] == '`') {
                            if (i + 1 < s.size()) ++i;
                            continue;
                        }
                        out.push_back(s[i]);
                    }
                    return out;
                };
                auto trim_copy = [](std::string s) -> std::string {
                    const auto l = s.find_first_not_of(" \t\r\n");
                    const auto r = s.find_last_not_of(" \t\r\n");
                    if (l == std::string::npos || r == std::string::npos) return {};
                    return s.substr(l, r - l + 1);
                };
                auto extract_spin = [&](const std::string& raw, std::string& out_name, int& out_spin, std::string& out_key) -> bool {
                    out_name.clear(); out_spin = -1; out_key.clear();
                    const std::string clean = strip_gt_codes_local(raw);
                    const std::string lower = [&]() {
                        std::string s = clean;
                        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
                            return static_cast<char>(std::tolower(c));
                        });
                        return s;
                    }();

                    const std::string key_phrase = "spun the wheel and got";
                    const size_t phrase_pos = lower.find(key_phrase);
                    if (phrase_pos == std::string::npos) return false;

                    
                    std::smatch mnum;
                    static const std::regex num_rx(R"((\d{1,3}))");
                    const std::string tail = clean.substr(phrase_pos + key_phrase.size());
                    if (!std::regex_search(tail, mnum, num_rx) || mnum.size() < 2) return false;
                    try { out_spin = std::stoi(mnum[1].str()); } catch (...) { return false; }
                    if (out_spin < 0) return false;

                    
                    const std::string head = clean.substr(0, phrase_pos);
                    std::smatch mname;
                    static const std::regex name_rx(R"(([A-Za-z0-9_]{2,}))");
                    auto begin = std::sregex_iterator(head.begin(), head.end(), name_rx);
                    auto end = std::sregex_iterator();
                    if (begin == end) return false;
                    std::string last_name;
                    for (auto it = begin; it != end; ++it) {
                        last_name = (*it)[1].str();
                    }
                    out_name = trim_copy(last_name);
                    if (out_name.empty()) return false;

                    std::string key_name = out_name;
                    std::transform(key_name.begin(), key_name.end(), key_name.begin(), [](unsigned char c) {
                        return static_cast<char>(std::tolower(c));
                    });
                    out_key = key_name + "|" + std::to_string(out_spin);
                    return !out_name.empty();
                };
                auto sum_reme = [](int spin) -> int {
                    const int abs_spin = spin < 0 ? -spin : spin;
                    const int ones = abs_spin % 10;
                    const int tens = (abs_spin / 10) % 10;
                    return tens + ones;
                };
                auto qq_digit = [](int spin) -> int {
                    const int abs_spin = spin < 0 ? -spin : spin;
                    return abs_spin % 10;
                };
                auto find_netid_by_name = [&](const std::string& raw_name) -> uint32_t {
                    auto normalize_name = [&](const std::string& in) -> std::string {
                        std::string s = strip_gt_codes_local(in);
                        s = trim_copy(s);
                        std::string out;
                        out.reserve(s.size());
                        for (unsigned char c : s) {
                            if (std::isalnum(c) || c == '_') {
                                out.push_back(static_cast<char>(std::tolower(c)));
                            }
                        }
                        return out;
                    };

                    const std::string target = normalize_name(raw_name);
                    if (target.empty()) return 0;
                    const auto all = utils::PlayerTracker::get_instance().get_all_players();
                    for (const auto& [nid, info] : all) {
                        const std::string cand = normalize_name(info.name);
                        if (!cand.empty() && cand == target) return nid;
                    }
                    return 0;
                };
                auto is_player_chat_console = [&](const std::string& raw) -> bool {
                    const std::string clean = strip_gt_codes_local(raw);
                    std::string lower = clean;
                    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
                        return static_cast<char>(std::tolower(c));
                    });
                    std::string raw_lower = raw;
                    std::transform(raw_lower.begin(), raw_lower.end(), raw_lower.begin(), [](unsigned char c) {
                        return static_cast<char>(std::tolower(c));
                    });
                    const bool has_player_chat_tag =
                        lower.find("player_chat") != std::string::npos ||
                        raw_lower.find("player_chat") != std::string::npos;
                    const bool has_cp_pl1 =
                        ((lower.find("cp:") != std::string::npos || raw_lower.find("cp:") != std::string::npos) &&
                         (lower.find("_pl:1") != std::string::npos || raw_lower.find("_pl:1") != std::string::npos));
                    const bool has_spin_text =
                        lower.find("spun the wheel and got") != std::string::npos ||
                        raw_lower.find("spun the wheel and got") != std::string::npos;
                    return lower.find("player_chat=") != std::string::npos ||
                           lower.find("cp:_pl:") != std::string::npos ||
                           lower.find("_ct:[w]") != std::string::npos ||
                           raw_lower.find("player_chat=") != std::string::npos ||
                           raw_lower.find("cp:_pl:") != std::string::npos ||
                           has_player_chat_tag ||
                           (has_cp_pl1 && has_spin_text);
                };
                auto read_netid_variant = [&](const packet::Variant& v, std::size_t idx) -> uint32_t {
                    if (idx >= v.size()) return 0;
                    try {
                        const auto i = v.get<int32_t>(idx);
                        if (i > 0) return static_cast<uint32_t>(i);
                    } catch (...) {}
                    try {
                        const auto u = v.get<uint32_t>(idx);
                        if (u > 0) return u;
                    } catch (...) {}
                    try {
                        const auto f = v.get<float>(idx);
                        if (f > 0.0f) return static_cast<uint32_t>(f);
                    } catch (...) {}
                    return 0;
                };
                auto has_recent_fake_spin = [&](const std::string& key, int spin) -> bool {
                    const auto now = std::chrono::steady_clock::now();
                    constexpr auto kWindow = std::chrono::seconds(4);
                    auto it_key = recent_fake_spins_.find(key);
                    if (it_key != recent_fake_spins_.end() && (now - it_key->second) <= kWindow) {
                        return true;
                    }
                    auto it_val = recent_fake_spin_values_.find(spin);
                    if (it_val != recent_fake_spin_values_.end() && (now - it_val->second) <= kWindow) {
                        return true;
                    }
                    auto it_pc = recent_playerchat_spin_values_.find(spin);
                    
                    if (it_pc != recent_playerchat_spin_values_.end() && (now - it_pc->second) <= std::chrono::seconds(2)) {
                        return true;
                    }
                    return false;
                };
                auto send_name_changed = [&](uint32_t netid, const std::string& new_name) {
                    if (netid == 0 || new_name.empty()) return;
                    packet::Variant var{};
                    var.add("OnNameChanged");
                    var.add(new_name);
                    std::vector<std::byte> ext_data = var.serialize();
                    packet::GameUpdatePacket pkt{};
                    pkt.type = packet::PACKET_CALL_FUNCTION;
                    pkt.net_id = netid;
                    pkt.flags.extended = 1;
                    pkt.data_size = static_cast<uint32_t>(ext_data.size());
                    ByteStream<std::uint16_t> bs{};
                    bs.write(packet::NET_MESSAGE_GAME_PACKET);
                    bs.write(pkt);
                    bs.write_data(ext_data.data(), ext_data.size());
                    event.get_target().send_packet(bs.get_data(), 0);
                };
                auto apply_last_suffix_for = [&](uint32_t netid, const std::string& base_name) {
                    if (!host_flag("features.host.show_last_spin")) return;
                    if (netid == 0 || base_name.empty()) return;
                    auto it = last_spin_by_netid_.find(netid);
                    const std::string last = (it != last_spin_by_netid_.end()) ? std::to_string(it->second) : "null";
                    const std::string name_with_last = base_name + " `6[LAST:" + last + "]``";
                    send_name_changed(netid, name_with_last);
                };
                auto send_modified_call = [&](packet::Variant& new_variant) {
                    std::vector<std::byte> new_ext_data = new_variant.serialize();
                    packet::GameUpdatePacket new_game_packet = event.get_packet();
                    new_game_packet.data_size = static_cast<uint32_t>(new_ext_data.size());
                    ByteStream<std::uint16_t> out_bs{};
                    out_bs.write(packet::NET_MESSAGE_GAME_PACKET);
                    out_bs.write(new_game_packet);
                    out_bs.write_data(new_ext_data.data(), new_ext_data.size());
                    event.get_target().send_packet(out_bs.get_data(), 0);
                    const_cast<core::EventPacket&>(event).canceled = true;
                };
                auto send_injected_call = [&](packet::Variant& new_variant) {
                    std::vector<std::byte> new_ext_data = new_variant.serialize();
                    packet::GameUpdatePacket new_game_packet{};
                    new_game_packet.type = packet::PACKET_CALL_FUNCTION;
                    new_game_packet.net_id = -1;
                    new_game_packet.flags.extended = 1;
                    new_game_packet.data_size = static_cast<uint32_t>(new_ext_data.size());
                    ByteStream<std::uint16_t> out_bs{};
                    out_bs.write(packet::NET_MESSAGE_GAME_PACKET);
                    out_bs.write(new_game_packet);
                    out_bs.write_data(new_ext_data.data(), new_ext_data.size());
                    event.get_target().send_packet(out_bs.get_data(), 0);
                };
                auto send_talkbubble_packet = [&](uint32_t netid, const std::string& text) {
                    packet::Variant bubble_var{};
                    bubble_var.add("OnTalkBubble");
                    bubble_var.add(static_cast<int>(netid));
                    bubble_var.add(text);
                    
                    
                    std::vector<std::byte> ext_data = bubble_var.serialize();
                    packet::GameUpdatePacket pkt{};
                    pkt.type = packet::PACKET_CALL_FUNCTION;
                    pkt.net_id = static_cast<int>(netid);
                    pkt.flags.extended = 1;
                    pkt.data_size = static_cast<uint32_t>(ext_data.size());
                    ByteStream<std::uint16_t> bs{};
                    bs.write(packet::NET_MESSAGE_GAME_PACKET);
                    bs.write(pkt);
                    bs.write_data(ext_data.data(), ext_data.size());
                    event.get_target().send_packet(bs.get_data(), 0);
                };
                auto consume_pending_instant = [&](const std::string& key, int spin) -> bool {
                    const auto now = std::chrono::steady_clock::now();
                    constexpr auto kWindow = std::chrono::seconds(3);
                    bool matched = false;

                    auto it_key = pending_instant_spin_keys_.find(key);
                    if (it_key != pending_instant_spin_keys_.end()) {
                        if (now - it_key->second <= kWindow) {
                            matched = true;
                        }
                        pending_instant_spin_keys_.erase(it_key);
                    }

                    auto it_val = pending_instant_spin_values_.find(spin);
                    if (it_val != pending_instant_spin_values_.end()) {
                        if (now - it_val->second <= kWindow) {
                            matched = true;
                        }
                        pending_instant_spin_values_.erase(it_val);
                    }
                    return matched;
                };
                auto consume_pending_fake_override = [&](int spin) -> bool {
                    const auto now = std::chrono::steady_clock::now();
                    constexpr auto kWindow = std::chrono::seconds(3);
                    auto it = pending_fake_override_spin_values_.find(spin);
                    if (it == pending_fake_override_spin_values_.end()) return false;
                    const bool matched = (now - it->second) <= kWindow;
                    pending_fake_override_spin_values_.erase(it);
                    return matched;
                };
                
                if (function_name == "OnSpawn") {
                    spdlog::info("[OnSpawn] Detected player spawn");
                    if (variant.size() > 1) {
                        std::string spawn_data = variant.get<std::string>(1);
                        try {
                            TextParse spawn_parse{ spawn_data };
                            const std::string netid_str = spawn_parse.get("netID");
                            const std::string base_name = spawn_parse.get("name");
                            if (!netid_str.empty() && !base_name.empty()) {
                                const uint32_t spawn_netid = spawn_parse.get<uint32_t>("netID");
                                apply_last_suffix_for(spawn_netid, base_name);

                                
                                if (!deferred_bubbles_.empty() && spawn_netid != 0) {
                                    auto norm_name = [](const std::string& s) {
                                        std::string out;
                                        for (unsigned char c : s)
                                            if (std::isalnum(c) || c == '_') out.push_back(static_cast<char>(std::tolower(c)));
                                        return out;
                                    };
                                    const std::string spawn_norm = norm_name(base_name);
                                    constexpr auto kMaxAge = std::chrono::seconds(8);
                                    const auto now_flush = std::chrono::steady_clock::now();
                                    auto it = deferred_bubbles_.begin();
                                    while (it != deferred_bubbles_.end()) {
                                        const bool name_match = (it->normalized_name == spawn_norm);
                                        const bool fresh = (now_flush - it->queued_at) <= kMaxAge;
                                        if (name_match && fresh) {
                                            send_talkbubble_packet(spawn_netid, it->text);
                                            spdlog::debug("[InstantRoulette] Flushed deferred bubble for '{}' netid={}", base_name, spawn_netid);
                                            it = deferred_bubbles_.erase(it);
                                        } else if (!fresh) {
                                            it = deferred_bubbles_.erase(it); 
                                        } else {
                                            ++it;
                                        }
                                    }
                                }
                            }
                        } catch (...) {}
                        spdlog::info("`2[Double Jump]`` Auto-enabled for player");
                        core_->get_config().set("features.double_jump", true);
                        spdlog::debug("Double jump config set to: {}", core_->get_config().get<bool>("features.double_jump"));
                        
                        pending_balance_on_inventory_ = true;
                    }
                }
                
                if (function_name == "OnConsoleMessage") {
                    if (variant.size() >= 2) {
                        std::string message = variant.get<std::string>(1);
                        
                        
                        
                        {
                            const bool cp_chat = is_player_chat_console(message);
                            if (cp_chat) {
                                const std::string clean_msg = strip_gt_codes_local(message);
                                std::string lower_msg = clean_msg;
                                std::transform(lower_msg.begin(), lower_msg.end(), lower_msg.begin(), [](unsigned char c) {
                                    return static_cast<char>(std::tolower(c));
                                });
                                const std::string key_phrase = "spun the wheel and got";
                                const size_t phrase_pos = lower_msg.find(key_phrase);
                                if (phrase_pos != std::string::npos) {
                                    static const std::regex num_rx(R"((\d{1,3}))");
                                    std::smatch mnum;
                                    const std::string tail = clean_msg.substr(phrase_pos + key_phrase.size());
                                    if (std::regex_search(tail, mnum, num_rx) && mnum.size() >= 2) {
                                        try {
                                            const int spin_val = std::stoi(mnum[1].str());
                                            if (spin_val >= 0) {
                                                const auto now = std::chrono::steady_clock::now();
                                                recent_playerchat_spin_values_[spin_val] = now;
                                                recent_fake_spin_values_[spin_val] = now;
                                            }
                                        } catch (...) {}
                                    }
                                }
                            }
                        }
                        {
                            std::string spin_name, spin_key;
                            int spin_value = -1;
                            if (extract_spin(message, spin_name, spin_value, spin_key)) {
                                const bool show_real = host_flag("features.host.show_real_spin");
                                const bool show_qq = host_flag("features.host.show_qq_number");
                                const bool show_reme = host_flag("features.host.show_reme_spin");
                                const bool instant = host_flag("features.host.instant_roulette_spin");
                                const bool show_last = host_flag("features.host.show_last_spin");
                                const bool from_player_chat = is_player_chat_console(message);

                                const auto now = std::chrono::steady_clock::now();
                                if (!from_player_chat) {
                                    recent_real_spins_[spin_key] = now;
                                    recent_real_spin_values_[spin_value] = now;
                                } else {
                                    recent_fake_spins_[spin_key] = now;
                                    recent_fake_spin_values_[spin_value] = now;
                                    recent_playerchat_spin_values_[spin_value] = now;
                                }

                                uint32_t spinner_netid = find_netid_by_name(spin_name);
                                if (spinner_netid != 0) {
                                    last_spin_by_netid_[spinner_netid] = spin_value;
                                    auto pinfo = utils::PlayerTracker::get_instance().get_player_by_netid(spinner_netid);
                                    if (!pinfo.name.empty()) {
                                        apply_last_suffix_for(spinner_netid, pinfo.name);
                                    }
                                }

                                std::string decorated = message;
                                std::string prefix;
                                
                                if (from_player_chat) {
                                    prefix += "`4[FAKE]`` ";
                                } else if (show_real) {
                                    prefix += "`2[REAL]`` ";
                                }
                                std::string suffix;
                                if (show_qq) suffix += fmt::format(" `8[QQ:{}]``", qq_digit(spin_value));
                                if (show_reme) suffix += fmt::format(" `5[REME:{}]``", sum_reme(spin_value));
                                if (show_last && spinner_netid != 0) suffix += fmt::format(" `6[LAST:{}]``", spin_value);
                                if (!prefix.empty() || !suffix.empty()) {
                                    decorated = prefix + message + suffix;
                                }

                                
                                
                                
                                if (from_player_chat && spinner_netid != 0) {
                                    send_talkbubble_packet(spinner_netid, decorated);
                                    pending_fake_override_spin_values_[spin_value] = now;
                                }

                                if (decorated != message) {
                                    packet::Variant console_var{};
                                    console_var.add("OnConsoleMessage");
                                    console_var.add(decorated);
                                    send_modified_call(console_var);
                                    return;
                                }
                            }
                        }
                        utils::VendLogsStore::try_add(message);

                        
                        auto get_ignorecsn_enabled = [&]() -> bool {
                            try { return core_->get_config().get<bool>("features.ignorecsn"); }
                            catch (...) { return false; }
                        };
                        auto get_ignorecsnchat_enabled = [&]() -> bool {
                            try { return core_->get_config().get<bool>("features.ignorecsnchat"); }
                            catch (...) { return false; }
                        };

                        auto strip_gt_codes = [](const std::string& s) -> std::string {
                            std::string out;
                            out.reserve(s.size());
                            for (size_t i = 0; i < s.size(); ++i) {
                                if (s[i] == '`') {
                                    
                                    if (i + 1 < s.size()) ++i;
                                    continue;
                                }
                                out.push_back(s[i]);
                            }
                            return out;
                        };

                        auto to_lower = [](std::string s) -> std::string {
                            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
                                return static_cast<char>(std::tolower(c));
                            });
                            return s;
                        };

                        auto normalize_alnum = [](const std::string& s) -> std::string {
                            std::string out;
                            out.reserve(s.size());
                            for (char c : s) {
                                if (std::isalnum(static_cast<unsigned char>(c))) out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
                            }
                            return out;
                        };

                        auto extract_sender = [](const std::string& clean_lower_msg) -> std::string {
                            
                            static const std::regex from_rx(R"(from\s*\(([^)]+)\))", std::regex::icase);
                            std::smatch m;
                            if (std::regex_search(clean_lower_msg, m, from_rx) && m.size() >= 2) {
                                std::string name = m[1].str();
                                
                                auto l = name.find_first_not_of(" \t\r\n");
                                auto r = name.find_last_not_of(" \t\r\n");
                                if (l == std::string::npos || r == std::string::npos) return "";
                                return name.substr(l, r - l + 1);
                            }
                            return "";
                        };
                        auto extract_chat_sender = [](const std::string& clean_msg) -> std::string {
                            
                            
                            static const std::regex chat_rx(R"(<\s*([A-Za-z0-9_]+))");
                            std::smatch m;
                            if (std::regex_search(clean_msg, m, chat_rx) && m.size() >= 2) {
                                return m[1].str();
                            }
                            return "";
                        };

                        
                        
                        {
                            const std::string clean_locke = strip_gt_codes(message);
                            const std::string lower_locke = to_lower(clean_locke);
                            static const std::regex locke_rx(
                                R"(locke\s+has\s+stopped\s+by\s+the\s+world\s+([a-z0-9_]+))",
                                std::regex::icase
                            );
                            std::smatch lm;
                            if (std::regex_search(lower_locke, lm, locke_rx) && lm.size() >= 2) {
                                std::string world_name = lm[1].str();
                                
                                static std::string last_locke_world;
                                static auto last_locke_time = std::chrono::steady_clock::time_point{};
                                const auto now_tp = std::chrono::steady_clock::now();
                                const bool dup_recent =
                                    (world_name == last_locke_world) &&
                                    (last_locke_time.time_since_epoch().count() != 0) &&
                                    (std::chrono::duration_cast<std::chrono::seconds>(now_tp - last_locke_time).count() < 30);
                                if (!dup_recent) {
                                    last_locke_world = world_name;
                                    last_locke_time = now_tp;
                                    utils::APIClient::upload_locke_data(world_name);
                                }
                            }
                        }

                        if (get_ignorecsn_enabled()) {
                            const std::string clean = strip_gt_codes(message);
                            const std::string lower = to_lower(clean);
                            const std::string normalized = normalize_alnum(clean);

                            bool hit = false;
                            if (lower.find("reme") != std::string::npos) {
                                hit = true;
                            } else {
                                static const std::vector<std::string> kNeedles = {
                                    "csn", "qq", "min", "m1n", "g4s", "c5n"
                                };
                                for (const auto& needle : kNeedles) {
                                    if (normalized.find(needle) != std::string::npos) {
                                        hit = true;
                                        break;
                                    }
                                }
                            }

                            if (hit) {
                                std::string sender = extract_sender(lower);
                                if (!sender.empty() && auto_ignored_names_.find(sender) == auto_ignored_names_.end()) {
                                    auto_ignored_names_.insert(sender);

                                    if (core_->get_client() && core_->get_client()->get_player()) {
                                        const std::string ignore_pkt = "action|input\ntext|/ignore " + sender;
                                        ByteStream<std::uint16_t> bs{};
                                        bs.write(packet::NET_MESSAGE_GENERIC_TEXT);
                                        bs.write(ignore_pkt, false);
                                        core_->get_client()->get_player()->send_packet(bs.get_data(), 0);
                                    }

                                    utils::PacketUtils::send_chat_message(
                                        const_cast<player::Player*>(&event.get_player()),
                                        fmt::format("ignored {} reason: csn", sender),
                                        false
                                    );
                                    spdlog::info("IgnoreCSN: auto ignored '{}'", sender);
                                }
                            }
                        }

                        if (get_ignorecsnchat_enabled()) {
                            const std::string clean = strip_gt_codes(message);
                            const std::string lower = to_lower(clean);
                            const std::string normalized = normalize_alnum(clean);
                            const bool is_player_chat =
                                lower.find("cp:_pl:1_") != std::string::npos ||
                                lower.find("_ct:[w]") != std::string::npos;

                            if (is_player_chat) {
                                bool hit = false;
                                if (lower.find("reme") != std::string::npos) {
                                    hit = true;
                                } else {
                                    static const std::vector<std::string> kChatNeedles = {
                                        "csn", "qq", "min", "m1n", "g4s", "c5n", "bj"
                                    };
                                    for (const auto& needle : kChatNeedles) {
                                        if (normalized.find(needle) != std::string::npos) {
                                            hit = true;
                                            break;
                                        }
                                    }
                                    if (!hit && lower.find("q!q") != std::string::npos) {
                                        hit = true;
                                    }
                                }

                                if (hit) {
                                    std::string sender = extract_chat_sender(clean);
                                    if (!sender.empty() && auto_ignored_names_.find(sender) == auto_ignored_names_.end()) {
                                        auto_ignored_names_.insert(sender);

                                        if (core_->get_client() && core_->get_client()->get_player()) {
                                            const std::string ignore_pkt = "action|input\ntext|/ignore " + sender;
                                            ByteStream<std::uint16_t> bs{};
                                            bs.write(packet::NET_MESSAGE_GENERIC_TEXT);
                                            bs.write(ignore_pkt, false);
                                            core_->get_client()->get_player()->send_packet(bs.get_data(), 0);
                                        }

                                        utils::PacketUtils::send_chat_message(
                                            const_cast<player::Player*>(&event.get_player()),
                                            fmt::format("ignored {} reason: csn", sender),
                                            false
                                        );
                                        spdlog::info("IgnoreCSNChat: auto ignored '{}'", sender);
                                    }
                                }
                            }
                        }

                        
                        if (message.find("SkriptProxy") == std::string::npos) {
                            
                            std::string prefixed_message = "`^[SkriptProxy]`o " + message;
                            
                            packet::Variant new_variant{};
                            new_variant.add("OnConsoleMessage");
                            new_variant.add(prefixed_message);
                            std::vector<std::byte> new_ext_data = new_variant.serialize();
                            
                            packet::GameUpdatePacket new_game_packet = event.get_packet();
                            new_game_packet.data_size = static_cast<uint32_t>(new_ext_data.size());
                            ByteStream<std::uint16_t> byte_stream{};
                            byte_stream.write(packet::NET_MESSAGE_GAME_PACKET);
                            byte_stream.write(new_game_packet);
                            byte_stream.write_data(new_ext_data.data(), new_ext_data.size());
                            
                            event.get_target().send_packet(byte_stream.get_data(), 0);
                            
                            const_cast<core::EventPacket&>(event).canceled = true;
                            spdlog::debug("Prefixed OnConsoleMessage: {}", message);
                            return; 
                        }
                    }
                }

                if (function_name == "OnTalkBubble" && variant.size() >= 3) {
                    const std::string bubble_text = variant.get<std::string>(2);

                    
                    
                    
                    
                    
                    static const std::regex level_req_rx(
                        R"(You need to be Level\s*(\d{1,2}) to break that!)",
                        std::regex::icase
                    );
                    std::smatch lvlm;
                    if (std::regex_search(bubble_text, lvlm, level_req_rx)) {
                        if (lvlm.size() >= 2) {
                            try {
                                int lvl = std::stoi(lvlm[1].str());
                                if (lvl >= 0 && lvl <= 25) {
                                    
                                    const_cast<core::EventPacket&>(event).canceled = true;
                                    return;
                                }
                            } catch (...) {
                                
                            }
                        }
                    }

                    std::string spin_name, spin_key;
                    int spin_value = -1;
                    if (extract_spin(bubble_text, spin_name, spin_value, spin_key)) {
                        const bool show_real = host_flag("features.host.show_real_spin");
                        const bool show_qq = host_flag("features.host.show_qq_number");
                        const bool show_reme = host_flag("features.host.show_reme_spin");
                        const bool instant = host_flag("features.host.instant_roulette_spin");
                        const bool show_last = host_flag("features.host.show_last_spin");

                        const std::string bubble_clean = strip_gt_codes_local(bubble_text);
                        std::string bubble_lower = bubble_clean;
                        std::transform(bubble_lower.begin(), bubble_lower.end(), bubble_lower.begin(), [](unsigned char c) {
                            return static_cast<char>(std::tolower(c));
                        });
                        const bool has_fake_chat_wrapper =
                            bubble_lower.find("<") != std::string::npos &&
                            bubble_lower.find(">") != std::string::npos &&
                            bubble_lower.find("spun the wheel and got") != std::string::npos;
                        const bool has_cp_pl1_marker =
                            (bubble_lower.find("cp:") != std::string::npos && bubble_lower.find("_pl:1") != std::string::npos) ||
                            bubble_lower.find("player_chat") != std::string::npos;
                        const bool from_player_chat =
                            is_player_chat_console(bubble_text) ||
                            has_fake_chat_wrapper ||
                            has_cp_pl1_marker;
                        const bool recent_fake = has_recent_fake_spin(spin_key, spin_value);
                        const bool is_fake = from_player_chat || recent_fake;
                        const bool is_real = !is_fake;

                        uint32_t netid = read_netid_variant(variant, 1);
                        if (netid == 0) netid = find_netid_by_name(spin_name);

                        spdlog::info("[InstantSpin] bubble='{}' instant={} is_real={} is_fake={} netid={} from_player_chat={} recent_fake={}",
                            bubble_text, instant, is_real, is_fake, netid, from_player_chat, recent_fake);
                        if (netid != 0) {
                            last_spin_by_netid_[netid] = spin_value;
                            auto pinfo = utils::PlayerTracker::get_instance().get_player_by_netid(netid);
                            if (!pinfo.name.empty()) {
                                apply_last_suffix_for(netid, pinfo.name);
                            }
                        }

                        
                        
                        if (consume_pending_fake_override(spin_value)) {
                            const_cast<core::EventPacket&>(event).canceled = true;
                            return;
                        }

                        
                        if (instant && is_real && netid != 0) {
                            std::string decorated = bubble_text;
                            std::string prefix;
                            if (show_real) prefix = "`2[REAL]`` ";
                            std::string suffix;
                            if (show_qq) suffix += fmt::format(" `8[QQ:{}]``", qq_digit(spin_value));
                            if (show_reme) suffix += fmt::format(" `5[REME:{}]``", sum_reme(spin_value));
                            if (show_last) {
                                auto li = last_spin_by_netid_.find(netid);
                                if (li != last_spin_by_netid_.end())
                                    suffix += fmt::format(" `6[LAST:{}]``", li->second);
                            }
                            if (!prefix.empty() || !suffix.empty())
                                decorated = prefix + bubble_text + suffix;
                            
                            packet::Variant bubble_var{};
                            bubble_var.add("OnTalkBubble");
                            bubble_var.add(static_cast<int>(netid));
                            bubble_var.add(decorated);
                            std::vector<std::byte> ext_data = bubble_var.serialize();
                            packet::GameUpdatePacket pkt{};
                            pkt.type = packet::PACKET_CALL_FUNCTION;
                            pkt.net_id = -1;
                            pkt.flags.extended = 1;
                            pkt.data_size = static_cast<uint32_t>(ext_data.size());
                            ByteStream<std::uint16_t> bs{};
                            bs.write(packet::NET_MESSAGE_GAME_PACKET);
                            bs.write(pkt);
                            bs.write_data(ext_data.data(), ext_data.size());
                            
                            if (core_->get_server() && core_->get_server()->get_player()) {
                                core_->get_server()->get_player()->send_packet(bs.get_data(), 0);
                            }
                            spdlog::info("[InstantSpin] SENT visual bubble: '{}'", decorated);
                            
                            const_cast<core::EventPacket&>(event).canceled = true;
                            return;
                        }

                        std::string decorated = bubble_text;
                        std::string prefix;
                        
                        if (is_fake) {
                            prefix += "`4[FAKE]`` ";
                        } else if (show_real && is_real) {
                            prefix += "`2[REAL]`` ";
                        }
                        std::string suffix;
                        if (show_qq) suffix += fmt::format(" `8[QQ:{}]``", qq_digit(spin_value));
                        if (show_reme) suffix += fmt::format(" `5[REME:{}]``", sum_reme(spin_value));
                        if (show_last) {
                            if (netid != 0) {
                                auto li = last_spin_by_netid_.find(netid);
                                if (li != last_spin_by_netid_.end()) {
                                    suffix += fmt::format(" `6[LAST:{}]``", li->second);
                                }
                            } else {
                                suffix += fmt::format(" `6[LAST:{}]``", spin_value);
                            }
                        }
                        if (!prefix.empty() || !suffix.empty()) {
                            decorated = prefix + bubble_text + suffix;
                            
                            packet::Variant new_bubble = variant;
                            new_bubble.set(2, packet::variant{ decorated });
                            send_modified_call(new_bubble);
                            return;
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            spdlog::warn("Error handling server message: {}", e.what());
        }
    }

    void track_player_positions(const core::EventPacket& event) {
        const auto& game_packet = event.get_packet();
        const auto& ext_data = event.get_ext_data();
        
        
        if (event.from == core::EventFrom::FromClient && game_packet.type == packet::PACKET_STATE) {
            if (ext_data.size() >= sizeof(packet::TankUpdatePacket)) {
                const packet::TankUpdatePacket* tank = 
                    reinterpret_cast<const packet::TankUpdatePacket*>(ext_data.data());
                
                
                int tile_x = static_cast<int>(tank->vec_x / 32.0f);
                int tile_y = static_cast<int>(tank->vec_y / 32.0f);
                
                spdlog::info("[YOUR POSITION] Tile: ({}, {}) | Pixels: ({:.0f}, {:.0f})", 
                            tile_x, tile_y, tank->vec_x, tank->vec_y);
                
                
                auto& tracker = utils::PlayerTracker::get_instance();
                auto local_player = tracker.get_local_player();
                if (local_player.netID > 0) {
                    tracker.update_player_position(local_player.netID, tank->vec_x, tank->vec_y);
                }
            }
        }
        
        
        if (event.from == core::EventFrom::FromServer && game_packet.type == packet::PACKET_STATE) {
            if (ext_data.size() >= sizeof(packet::TankUpdatePacket)) {
                const packet::TankUpdatePacket* tank = 
                    reinterpret_cast<const packet::TankUpdatePacket*>(ext_data.data());
                
                int tile_x = static_cast<int>(tank->vec_x / 32.0f);
                int tile_y = static_cast<int>(tank->vec_y / 32.0f);
                
                auto& tracker = utils::PlayerTracker::get_instance();
                auto local_player = tracker.get_local_player();
                
                if (local_player.netID > 0 && static_cast<uint32_t>(tank->net_id) != local_player.netID) {
                    spdlog::debug("[OTHER PLAYER {}] Tile: ({}, {}) | Pixels: ({:.0f}, {:.0f})", 
                                 tank->net_id, tile_x, tile_y, tank->vec_x, tank->vec_y);
                }
                
                tracker.update_player_position(tank->net_id, tank->vec_x, tank->vec_y);
            } else {
                
                spdlog::debug("[STATE PACKET] NetID: {} has no position data (size: {})", 
                             game_packet.net_id, ext_data.size());
            }
        }
        
        
        if (event.from == core::EventFrom::FromServer) {
            const auto& ext_data_var = event.get_ext_data();
            if (!ext_data_var.empty()) {
                packet::Variant variant{};
                if (variant.deserialize(ext_data_var)) {
                    if (variant.size() > 0) {
                        std::string function_name = variant.get<std::string>(0);
                        
                        
                        if (function_name == "OnSetPos") {
                            
                            if (variant.size() > 1) {
                                auto var_type = packet::Variant::get_type(variant.get_variants()[1]);
                                if (var_type == packet::VariantType::VEC2) {
                                    glm::vec2 pos = variant.get<glm::vec2>(1);
                                    utils::PlayerTracker::get_instance().update_player_position(
                                        game_packet.net_id, pos.x, pos.y
                                    );
                                    
                                    
                                    
                                    int tile_x = static_cast<int>(pos.x / 32.0f);
                                    int tile_y = static_cast<int>(pos.y / 32.0f);
                                    
                                    auto& tracker = utils::PlayerTracker::get_instance();
                                    auto local_player = tracker.get_local_player();
                                    
                                    
                                    if (local_player.netID == 0) {
                                        
                                        spdlog::info("[POSITION DEBUG] Player netID {} at X: {} Y: {} (local player not initialized yet)", 
                                                    game_packet.net_id, tile_x, tile_y);
                                    } else if (local_player.netID == game_packet.net_id) {
                                        
                                        spdlog::info("[PLAYER POSITION] X: {} Y: {} (pixels: {:.0f}, {:.0f})", 
                                                    tile_x, tile_y, pos.x, pos.y);
                                    } else {
                                        
                                        spdlog::debug("[OTHER PLAYER {}] X: {} Y: {}", game_packet.net_id, tile_x, tile_y);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    void handle_game_features(const core::EventPacket& event) {
        const auto& game_packet = event.get_packet();
        
        
        if (game_packet.type != packet::PACKET_STATE) {
            return;
        }

        const auto& ext_data = event.get_ext_data();
        if (ext_data.size() < sizeof(packet::TankUpdatePacket)) {
            return;
        }

        
        auto* tank = reinterpret_cast<const packet::TankUpdatePacket*>(ext_data.data());
        packet::TankUpdatePacket modified_tank = *tank;
        bool needs_modification = false;

        
        
        
        if (command::GhostCharCommand::is_enabled() && event.from == core::EventFrom::FromClient) {
            if (tank->flags != 2356) {
                modified_tank.flags = 0;
                needs_modification = true;
                spdlog::info("[Ghost] Cleared animation flags (was {})", tank->flags);
            }
        }

        
        if (event.from == core::EventFrom::FromClient) {
            bool double_jump_enabled = core_->get_config().get<bool>("features.double_jump");
            if (double_jump_enabled) {
                if (tank->flags & packet::PACKET_FLAG_ON_JUMP) {
                    
                    modified_tank.flags |= packet::PACKET_FLAG_ON_SOLID;
                    needs_modification = true;
                    spdlog::info("[Double Jump] Applied on_solid flag (flags: {} -> {})", tank->flags, modified_tank.flags);
                }
            }
        }

        
        if (event.from == core::EventFrom::FromServer) {
            bool immune_enabled = core_->get_config().get<bool>("features.immune_damage");
            if (immune_enabled) {
                bool removed_damage = false;
                if (tank->flags & packet::PACKET_FLAG_ON_FIRE_DAMAGE) {
                    modified_tank.flags &= ~packet::PACKET_FLAG_ON_FIRE_DAMAGE;
                    needs_modification = true;
                    removed_damage = true;
                }
                if (tank->flags & packet::PACKET_FLAG_ON_ACID_DAMAGE) {
                    modified_tank.flags &= ~packet::PACKET_FLAG_ON_ACID_DAMAGE;
                    needs_modification = true;
                    removed_damage = true;
                }
                if (removed_damage) {
                    spdlog::info("[Immune] Blocked fire/acid damage (flags: {} -> {})", tank->flags, modified_tank.flags);
                }
            }
        }

        
        if (needs_modification) {
            ByteStream<std::uint16_t> byte_stream{};
            byte_stream.write(packet::NET_MESSAGE_GAME_PACKET);
            byte_stream.write(game_packet);
            
            std::vector<std::byte> modified_data(reinterpret_cast<std::byte*>(&modified_tank),
                                                 reinterpret_cast<std::byte*>(&modified_tank) + sizeof(packet::TankUpdatePacket));
            byte_stream.write_data(modified_data.data(), modified_data.size());
            
            
            event.get_target().send_packet(byte_stream.get_data(), 0);
            
            
            const_cast<core::EventPacket&>(event).canceled = true;
        }
    }

    void handle_chat_message(const core::EventMessage& event) {
        if (event.from != core::EventFrom::FromClient) {
            return;
        }

        TextParse text_parse(event.get_message().get_raw());
        std::string message = text_parse.get("text");

        if (message.size() > 1 && message[0] == '/') {
            std::string command_text = message.substr(1);

            
            if (command_text == "gui" || command_text == "menu" || command_text == "interface") {
                
                return;
            }

            
            std::vector<std::string> args;
            std::string current_arg;
            bool in_quotes = false;

            for (char c : command_text) {
                if (c == '"') {
                    in_quotes = !in_quotes;
                } else if (c == ' ' && !in_quotes) {
                    if (!current_arg.empty()) {
                        args.push_back(current_arg);
                        current_arg.clear();
                    }
                } else {
                    current_arg += c;
                }
            }

            if (!current_arg.empty()) {
                args.push_back(current_arg);
            }

            if (!args.empty()) {
                std::string command_name = args[0];
                spdlog::info("Command received: {} from player {}", command_name, event.get_player().get_peer()->connectID);

            }
        }
    }

    void parse_call_function(const core::EventPacket& event) const
    {
        if (event.from == core::EventFrom::FromClient) {
            return;
        }

        
        if (event.canceled) {
            return;
        }

        const auto& ext_data = event.get_ext_data();
        if (ext_data.empty()) {
            return; 
        }

        packet::Variant variant{};
        if (!variant.deserialize(ext_data)) {
            spdlog::debug("Failed to deserialize variant (this might be normal for some packets)");
            return; 
        }

        std::vector variants{ variant.get_variants() };
        if (variants.empty()) {
            return; 
        }

        
        if (event.from == core::EventFrom::FromServer && variants.size() >= 2) {
            try {
                if (std::get<std::string>(variants[0]) == "OnDialogRequest") {
                    std::string dialog = std::get<std::string>(variants[1]);
                    if (dialog.find("dialog_name|drop_item") != std::string::npos &&
                        command::DropWLCommand::is_dropping()) {
                        const_cast<core::EventPacket&>(event).canceled = true;
                        return;
                    }
                }
            } catch (...) {}
        }

        
        if (core_->get_config().get<bool>("log.printVariant"))  {
            
            if (event.from == core::EventFrom::FromServer && variants.size() >= 2) {
                try {
                    if (std::get<std::string>(variants[0]) == "OnDialogRequest") {
                        std::string dialog = std::get<std::string>(variants[1]);
                        if (dialog.find("dialog_name|drop_item") != std::string::npos &&
                            (command::DropAllCommand::is_running() || command::DropWLCommand::is_dropping())) {
                            
                            const_cast<core::EventPacket&>(event).canceled = true;
                            return;
                        }
                    }
                } catch (...) {}
            }

            spdlog::info("Incoming variant from {}:", event.from == core::EventFrom::FromClient ? "client" : "server");
            for (size_t i = 0; i < variants.size(); ++i) {
                try {
                    switch (packet::Variant::get_type(variants[i])) {
                    case packet::VariantType::FLOAT:
                        spdlog::info("val_: {}", std::get<float>(variants[i]));
                        break;
                    case packet::VariantType::STRING: {
                        std::string str_val = std::get<std::string>(variants[i]);
                        if (str_val.empty()) {
                            spdlog::info("[SERVER] (empty string)");
                            break;
                        }

                        
                        try {
                            TextParse text_parse{ str_val };
                            if (!text_parse.empty()) {
                                std::vector key_values{ text_parse.get_key_values() };
                                if (key_values.size() == 1) {
                                    spdlog::info("[SERVER] {}", key_values[0]);
                                    break;
                                }

                                spdlog::info("[SERVER]");
                                for (const auto& key_value : text_parse.get_key_values()) {
                                    spdlog::info("{}", key_value);
                                }
                                break;
                            }
                        } catch (const std::exception& e) {
                            
                            spdlog::info("[SERVER] {}", str_val);
                            break;
                        }

                        spdlog::info("[SERVER] {}", str_val);
                        break;
                    }
                    case packet::VariantType::VEC2:
                        {
                            const glm::vec2 vec2{ std::get<glm::vec2>(variants[i]) };
                            spdlog::info("[POSITION] X: {}, Y: {}", vec2.x, vec2.y);
                            
                            
                            if (variant.size() > 0) {
                                std::string function_name = variant.get<std::string>(0);
                                if (function_name == "OnSetPos" || function_name == "OnSpawn") {
                                    utils::PlayerTracker::get_instance().update_player_position(
                                        event.get_packet().net_id, vec2.x, vec2.y
                                    );
                                }
                            }
                        }
                        break;
                    case packet::VariantType::VEC3:
                        {
                            const glm::vec3 vec3{ std::get<glm::vec3>(variants[i]) };
                            spdlog::info("[VECTOR3] X: {}, Y: {}, Z: {}", vec3.x, vec3.y, vec3.z);
                            
                            
                            utils::PlayerTracker::get_instance().update_player_position(
                                event.get_packet().net_id, vec3.x, vec3.y
                            );
                        }
                        break;
                    case packet::VariantType::UNSIGNED:
                        spdlog::info("[CLIENT] {}", std::get<uint32_t>(variants[i]));
                        break;
                    case packet::VariantType::SIGNED:
                        spdlog::info("[CLIENT] {}", std::get<int32_t>(variants[i]));
                        break;
                    default:
                        spdlog::info("[UNKNOWN_TYPE] index: {}", i);
                        break;
                    }
                } catch (const std::exception& e) {
                    spdlog::debug("Error processing variant at index {}: {}", i, e.what());
                    continue; 
                }
            }
        }

        
        try {
            if (variant.size() > 0) {
                std::string function_name = variant.get<std::string>(0);
                
                
                if (function_name == "OnSpawn") {
                    if (variant.size() >= 2) {
                        std::string spawn_data = variant.get<std::string>(1);
                        TextParse text_parse{ spawn_data };
                        
                        uint32_t netID = 0;
                        uint32_t userID = 0;
                        const std::string net_id_str = text_parse.get("netID");
                        const std::string user_id_str = text_parse.get("userID");
                        if (!net_id_str.empty()) {
                            netID = text_parse.get<uint32_t>("netID");
                        }
                        if (!user_id_str.empty()) {
                            userID = text_parse.get<uint32_t>("userID");
                        }

                        std::string name = text_parse.get("name");
                        std::string country = text_parse.get("country");
                        std::string platform_id = text_parse.get("platformID");
                        const std::string spawn_type = text_parse.get("type");
                        const bool is_local = (spawn_type == "local");

                        if (netID != 0) {
                            utils::PlayerTracker::get_instance().update_player_info(
                                netID, userID, name, country, is_local
                            );
                            if (!name.empty()) {
                                utils::PlayerTracker::get_instance().update_player_name(netID, name);
                            }
                            if (!platform_id.empty()) {
                                utils::PlayerTracker::get_instance().update_platform_info(netID, platform_id);
                            }
                        }

                        if (is_local) {
                            spdlog::info("=== LOCAL PLAYER SPAWNED ===");
                            spdlog::info("NetID: {}", netID);
                            spdlog::info("UserID: {}", userID);
                            spdlog::info("Name: {}", name);
                            spdlog::info("Country: {}", country);
                            spdlog::info("PlatformID: {}", platform_id.empty() ? "Unknown" : platform_id);
                            spdlog::info("===========================");
                            
                            
                            if (core_) {
                                std::string saved_name = core_->get_config().get<std::string>("display.visual_name");
                                bool show_ping = core_->get_config().get<bool>("display.show_ping");
                                bool has_g4g = core_->get_config().get<bool>("display.title.g4g");
                                bool has_maxlevel = core_->get_config().get<bool>("display.title.maxlevel");
                                bool has_dr = core_->get_config().get<bool>("display.title.dr");
                                bool has_mentor = core_->get_config().get<bool>("display.title.mentor");
                                
                                spdlog::info("[OnSpawn] Auto-applying ALL display settings: name='{}', ping={}, g4g={}, maxlevel={}, dr={}, mentor={}", 
                                            saved_name, show_ping, has_g4g, has_maxlevel, has_dr, has_mentor);
                                
                                
                                std::thread([this, netID, name]() {
                                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                                    if (core_) {
                                        spdlog::info("[OnSpawn] Applying display settings now (netID: {})", netID);
                                        utils::DisplayManager::apply_display_name(core_, netID, name);
                                        spdlog::info("[OnSpawn] Display settings applied successfully");
                                    }
                                }).detach();
                            }
                        } else {
                            spdlog::debug("Tracked spawned player: netID={}, name='{}', platformID='{}'",
                                netID,
                                name.empty() ? "Unknown" : name,
                                platform_id.empty() ? "Unknown" : platform_id
                            );
                        }
                    }
                }

                if (function_name == "OnDialogRequest" && variant.size() >= 2) {
                    try {
                        const std::string dialog_content = variant.get<std::string>(1);
                        bool wrench_auto_pull = false;
                        bool wrench_auto_kick = false;
                        bool wrench_auto_ban = false;
                        try { wrench_auto_pull = core_->get_config().get<bool>("features.wrench.auto_pull"); } catch (...) {}
                        try { wrench_auto_kick = core_->get_config().get<bool>("features.wrench.auto_kick"); } catch (...) {}
                        try { wrench_auto_ban = core_->get_config().get<bool>("features.wrench.auto_ban"); } catch (...) {}

                        if (dialog_content.find("end_dialog|drop_item") != std::string::npos) {
                            if (command::DropAllCommand::is_running() || command::DropWLCommand::is_dropping()) {
                                spdlog::info("Parser: suppressing drop_item dialog");
                                const_cast<core::EventPacket&>(event).canceled = true;
                                return;
                            }
                            if (command::DropFastCommand::is_enabled()) {
                                std::string item_id, count;
                                std::smatch m;
                                if (std::regex_search(dialog_content, m, std::regex("embed_data\\|itemID\\|([0-9]+)")))
                                    item_id = m[1].str();
                                if (std::regex_search(dialog_content, m, std::regex("add_text_input\\|count\\|\\|([0-9]*)")))
                                    count = m[1].str();
                                if (!item_id.empty()) {
                                    std::string resp;
                                    if (!count.empty()) {
                                        
                                        resp = "action|dialog_return\ndialog_name|drop_item\nitemID|" + item_id + "|\ncount|" + count + "|\nbuttonClicked|yes";
                                        spdlog::info("DropFast: auto-confirmed drop itemID={} count={}", item_id, count);
                                    } else {
                                        
                                        resp = "action|dialog_return\ndialog_name|drop_item\nitemID|" + item_id + "|\nbuttonClicked|yes";
                                        spdlog::info("DropFast: auto-confirmed warning dialog itemID={}", item_id);
                                    }
                                    core::Core* core = core_;
                                    std::thread([core, resp]() {
                                        std::this_thread::sleep_for(std::chrono::milliseconds(150));
                                        if (core && core->get_client() && core->get_client()->get_player()) {
                                            ByteStream<std::uint16_t> bs{};
                                            bs.write(packet::NET_MESSAGE_GENERIC_TEXT);
                                            bs.write(resp, false);
                                            core->get_client()->get_player()->send_packet(bs.get_data(), 0);
                                        }
                                    }).detach();
                                    const_cast<core::EventPacket&>(event).canceled = true;
                                    return;
                                }
                            }
                        }

                        if (dialog_content.find("end_dialog|trash_item") != std::string::npos) {
                            if (command::TrashFastCommand::is_enabled()) {
                                std::string item_id, count;
                                std::smatch m;
                                if (std::regex_search(dialog_content, m, std::regex("embed_data\\|itemID\\|([0-9]+)")))
                                    item_id = m[1].str();
                                
                                if (std::regex_search(dialog_content, m, std::regex("add_text_input\\|count\\|\\|([0-9]*)")))
                                    count = m[1].str();
                                
                                if (count.empty() || count == "0") {
                                    if (std::regex_search(dialog_content, m, std::regex("you have ([0-9]+)")))
                                        count = m[1].str();
                                }
                                if (count.empty() || count == "0") count = "1";
                                if (!item_id.empty()) {
                                    std::string resp;
                                    if (!count.empty()) {
                                        resp = "action|dialog_return\ndialog_name|trash_item\nitemID|" + item_id + "|\ncount|" + count + "|\nbuttonClicked|yes";
                                        spdlog::info("TrashFast: auto-confirmed trash itemID={} count={}", item_id, count);
                                    } else {
                                        resp = "action|dialog_return\ndialog_name|trash_item\nitemID|" + item_id + "|\nbuttonClicked|yes";
                                        spdlog::info("TrashFast: auto-confirmed warning dialog itemID={}", item_id);
                                    }
                                    core::Core* core = core_;
                                    std::thread([core, resp]() {
                                        std::this_thread::sleep_for(std::chrono::milliseconds(150));
                                        if (core && core->get_client() && core->get_client()->get_player()) {
                                            ByteStream<std::uint16_t> bs{};
                                            bs.write(packet::NET_MESSAGE_GENERIC_TEXT);
                                            bs.write(resp, false);
                                            core->get_client()->get_player()->send_packet(bs.get_data(), 0);
                                        }
                                    }).detach();
                                    const_cast<core::EventPacket&>(event).canceled = true;
                                    return;
                                }
                            }
                        }

                        if ((wrench_auto_pull || wrench_auto_kick || wrench_auto_ban) &&
                            dialog_content.find("add_popup_name|WrenchMenu") != std::string::npos &&
                            dialog_content.find("end_dialog|popup|") != std::string::npos) {
                            std::smatch netid_match;
                            std::string target_netid = "0";
                            const std::regex netid_re("embed_data\\|(?:netID|netid)\\|([0-9]+)");
                            if (std::regex_search(dialog_content, netid_match, netid_re) && netid_match.size() > 1) {
                                target_netid = netid_match[1].str();
                            }

                            std::string player_name;
                            std::smatch name_match;
                            const std::regex name_re("add_label_with_icon\\|big\\|`w([^`\\|\\(]+)");
                            if (std::regex_search(dialog_content, name_match, name_re) && name_match.size() > 1) {
                                player_name = name_match[1].str();
                            }

                            if (!player_name.empty()) {
                                while (!player_name.empty() && std::isspace(static_cast<unsigned char>(player_name.back()))) {
                                    player_name.pop_back();
                                }
                            }

                            if (!player_name.empty()) {
                                std::string cmd = "/pull " + player_name + " ";
                                if (wrench_auto_ban) cmd = "/ban " + player_name + " ";
                                else if (wrench_auto_kick) cmd = "/kick " + player_name + " ";

                                core::Core* core = core_;
                                std::thread([core, cmd]() {
                                    std::this_thread::sleep_for(std::chrono::milliseconds(70));
                                    for (int i = 0; i < 3; ++i) {
                                        if (!core || !core->get_client() || !core->get_client()->get_player()) {
                                            break;
                                        }
                                        ByteStream<std::uint16_t> bs{};
                                        bs.write(packet::NET_MESSAGE_GENERIC_TEXT);
                                        bs.write("action|input\ntext|" + cmd, false);
                                        core->get_client()->get_player()->send_packet(bs.get_data(), 0);
                                        std::this_thread::sleep_for(std::chrono::milliseconds(90));
                                    }
                                }).detach();

                                spdlog::info("AutoWrench: target_netid={}, name='{}', cmd='{}'", target_netid, player_name, cmd);
                                const_cast<core::EventPacket&>(event).canceled = true;
                                return;
                            }
                        }

                        bool hide_unaccess_dialog = false;
                        try { hide_unaccess_dialog = core_->get_config().get<bool>("features.rema_hide_unaccess_dialog"); }
                        catch (...) { hide_unaccess_dialog = false; }
                        if (hide_unaccess_dialog &&
                            dialog_content.find("end_dialog|unaccess") != std::string::npos) {
                            const_cast<core::EventPacket&>(event).canceled = true;
                            return;
                        }
                        command::DatCommand::ingest_donation_dialog(dialog_content);
                    } catch (...) {
                        
                    }
                }
                
                const EventCallFunction event_call_function{
                    event.get_player(),
                    event.get_target(),
                    function_name,
                    variant
                };
                event_call_function.from = event.from;

                event_dispatcher_.dispatch(event_call_function);
                event.canceled = event_call_function.canceled;
            }
        } catch (const std::exception& e) {
            spdlog::debug("Failed to dispatch variant event: {}", e.what());
        }
    }
};
}
