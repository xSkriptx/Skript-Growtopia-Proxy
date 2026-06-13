#include "doorid_command.hpp"
#include "../../client/client.hpp"
#include "../../server/server.hpp"
#include "../../player/player.hpp"
#include "../../packet/packet_variant.hpp"
#include "../../packet/packet_types.hpp"
#include "../../utils/byte_stream.hpp"
#include "../../utils/world_manager.hpp"
#include "../../utils/player_tracker.hpp"
#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <regex>
#include <algorithm>
#include <chrono>

namespace command {

bool DoorIDCommand::s_reveal_enabled = false;
core::Core* DoorIDCommand::s_core = nullptr;
std::atomic<bool> DoorIDCommand::s_monitor_running = false;
std::thread DoorIDCommand::s_monitor_thread{};
int DoorIDCommand::s_last_tile_x = -1;
int DoorIDCommand::s_last_tile_y = -1;
bool DoorIDCommand::s_have_last_tile = false;
std::string DoorIDCommand::s_last_reported_door_id{};

DoorIDCommand::DoorIDCommand() : CommandBase(
    {"doorid"},
    {},
    "Toggle door ID reveal - shows door IDs when you enter them",
    0
) {}

std::unique_ptr<CommandBase> DoorIDCommand::clone() const {
    return std::make_unique<DoorIDCommand>(*this);
}

void DoorIDCommand::set_core(core::Core* core) {
    s_core = core;
}

void DoorIDCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    if (!client || !client->get_player()) {
        return;
    }

    if (!s_core) {
        spdlog::error("DoorIDCommand: No core set!");
        return;
    }

    auto* server = s_core->get_server();
    if (!server || !server->get_player()) {
        spdlog::error("DoorIDCommand: No server player!");
        return;
    }

    
    toggle_door_id_reveal();
    
    
    std::string message;
    if (s_reveal_enabled) {
        start_monitor();
        message = "`0[`bDOOR ID REVEAL`0] `2ENABLED`` - Door IDs will be shown in chat when you enter them";
        spdlog::info("DoorIDCommand: Door ID reveal ENABLED");
    } else {
        stop_monitor();
        message = "`0[`bDOOR ID REVEAL`0] `4DISABLED`` - Door IDs will not be shown";
        spdlog::info("DoorIDCommand: Door ID reveal DISABLED");
    }
    send_console_message(message);
}

void DoorIDCommand::toggle_door_id_reveal() {
    s_reveal_enabled = !s_reveal_enabled;
}

bool DoorIDCommand::is_door_id_reveal_enabled() {
    return s_reveal_enabled;
}

void DoorIDCommand::handle_send_to_server(const std::string& server_info) {
    if (!s_reveal_enabled || !s_core) {
        return;
    }

    auto* server = s_core->get_server();
    if (!server || !server->get_player()) {
        return;
    }

    
    try {
        std::string door_id;

        
        size_t first_pipe = server_info.find('|');
        size_t second_pipe = (first_pipe == std::string::npos) ? std::string::npos : server_info.find('|', first_pipe + 1);
        size_t third_pipe = (second_pipe == std::string::npos) ? std::string::npos : server_info.find('|', second_pipe + 1);
        if (second_pipe != std::string::npos && third_pipe != std::string::npos && third_pipe > second_pipe + 1) {
            door_id = server_info.substr(second_pipe + 1, third_pipe - second_pipe - 1);
        }

        
        if (door_id.empty()) {
            static const std::regex lua_like_rx(R"(\|([^|]+)\|)");
            std::smatch m;
            if (std::regex_search(server_info, m, lua_like_rx) && m.size() >= 2) {
                door_id = m[1].str();
            }
        }

        
        if (!door_id.empty()) {
            door_id.erase(std::remove_if(door_id.begin(), door_id.end(), [](unsigned char c) {
                return c == '\r' || c == '\n' || c == '\t';
            }), door_id.end());
            size_t start = door_id.find_first_not_of(' ');
            size_t end = door_id.find_last_not_of(' ');
            if (start == std::string::npos) {
                door_id.clear();
            } else {
                door_id = door_id.substr(start, end - start + 1);
            }
        }
        
        
        std::string message;
        if (door_id.empty()) {
            message = "`9[ID Door Leaked]: `4No DoorID Found";
            spdlog::info("[ID Door Leaked]: No DoorID Found");
        } else {
            message = fmt::format("`9[ID Door Leaked]: `2{}", door_id);
            spdlog::info("[ID Door Leaked]: {}", door_id);
        }
        
        send_console_message(message);
        
    } catch (const std::exception& e) {
        spdlog::error("DoorIDCommand: Error parsing door ID: {}", e.what());
    }
}

void DoorIDCommand::start_monitor() {
    if (s_monitor_running.exchange(true)) {
        return;
    }
    s_have_last_tile = false;
    s_last_tile_x = -1;
    s_last_tile_y = -1;
    s_last_reported_door_id.clear();
    s_monitor_thread = std::thread(&DoorIDCommand::monitor_loop);
}

void DoorIDCommand::stop_monitor() {
    if (!s_monitor_running.exchange(false)) {
        return;
    }
    if (s_monitor_thread.joinable()) {
        s_monitor_thread.join();
    }
}

void DoorIDCommand::send_console_message(const std::string& message) {
    if (!s_core) return;
    auto* server = s_core->get_server();
    if (!server || !server->get_player()) return;

    packet::Variant var{};
    var.add("OnConsoleMessage");
    var.add(message);

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

    server->get_player()->send_packet(bs.get_data(), 0);
}

int DoorIDCommand::find_door_id_near(int tx, int ty, std::string& out_id) {
    out_id.clear();
    auto world = utils::WorldManager::get_instance().get_world_v2();
    if (!world.is_valid || world.tiles.empty()) {
        return 999999;
    }

    int best_dist = 999999;
    for (const auto& t : world.tiles) {
        if (!(t.extra_type == 1 || t.extra_type == 2)) continue;
        if (!t.door_data.has_data()) continue;
        if (t.door_data.destination.empty()) continue;

        const int dx = static_cast<int>(t.x) - tx;
        const int dy = static_cast<int>(t.y) - ty;
        const int dist = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
        if (dist < best_dist) {
            best_dist = dist;
            out_id = t.door_data.destination;
        }
    }
    return best_dist;
}

void DoorIDCommand::monitor_loop() {
    while (s_monitor_running) {
        try {
            if (!s_reveal_enabled || !s_core) {
                std::this_thread::sleep_for(std::chrono::milliseconds(120));
                continue;
            }

            int tx = -1;
            int ty = -1;

            auto local = utils::PlayerTracker::get_instance().get_local_player();
            if (local.netID > 0) {
                const auto pos = utils::PlayerTracker::get_instance().get_player_position(local.netID);
                tx = static_cast<int>(pos.x / 32.0f);
                ty = static_cast<int>(pos.y / 32.0f);
            }

            if (tx < 0 || ty < 0) {
                try {
                    const float fx = std::stof(s_core->get_config().get<std::string>("player.position.x"));
                    const float fy = std::stof(s_core->get_config().get<std::string>("player.position.y"));
                    tx = static_cast<int>(fx / 32.0f);
                    ty = static_cast<int>(fy / 32.0f);
                } catch (...) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(120));
                    continue;
                }
            }

            if (!s_have_last_tile) {
                s_last_tile_x = tx;
                s_last_tile_y = ty;
                s_have_last_tile = true;
                std::this_thread::sleep_for(std::chrono::milliseconds(120));
                continue;
            }

            if (tx != s_last_tile_x || ty != s_last_tile_y) {
                std::string id_now;
                std::string id_prev;
                const int dist_now = find_door_id_near(tx, ty, id_now);
                const int dist_prev = find_door_id_near(s_last_tile_x, s_last_tile_y, id_prev);

                std::string picked_id;
                int picked_dist = 999999;
                if (!id_prev.empty() && dist_prev <= 2) {
                    picked_id = id_prev;
                    picked_dist = dist_prev;
                }
                if (!id_now.empty() && dist_now <= 2 && dist_now < picked_dist) {
                    picked_id = id_now;
                    picked_dist = dist_now;
                }

                if (!picked_id.empty() && picked_id != s_last_reported_door_id) {
                    send_console_message(fmt::format("`9[ID Door Leaked]: `2{}", picked_id));
                    s_last_reported_door_id = picked_id;
                    spdlog::info("[ID Door Leaked] monitor: {} (dist={})", picked_id, picked_dist);
                }

                s_last_tile_x = tx;
                s_last_tile_y = ty;
            }
        } catch (const std::exception& e) {
            spdlog::warn("DoorID monitor error: {}", e.what());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(120));
    }
}

} 
