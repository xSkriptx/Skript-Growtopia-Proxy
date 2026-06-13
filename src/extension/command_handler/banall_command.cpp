#include "banall_command.hpp"
#include "../../client/client.hpp"
#include "../../player/player.hpp"
#include "../../server/server.hpp"
#include "../../utils/byte_stream.hpp"
#include "../../packet/packet_types.hpp"
#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <algorithm>
#include <thread>
#include <chrono>

namespace command {

core::Core* BanallCommand::g_core = nullptr;
std::vector<std::string> BanallCommand::spawned_players_;

BanallCommand::BanallCommand() : CommandBase(
    {"banall"},
    {},
    "Ban all players who have spawned",
    0
) {}

std::unique_ptr<CommandBase> BanallCommand::clone() const {
    return std::make_unique<BanallCommand>(*this);
}

void BanallCommand::set_core(core::Core* core) {
    g_core = core;
}

void BanallCommand::add_spawned_player(const std::string& name) {
    
    if (std::find(spawned_players_.begin(), spawned_players_.end(), name) == spawned_players_.end()) {
        spawned_players_.push_back(name);
        spdlog::info("[BANALL-TRACKER] Added spawned player: '{}'", name);
    }
}

std::vector<std::string> BanallCommand::get_spawned_players() {
    return spawned_players_;
}

void BanallCommand::clear_spawned_players() {
    spawned_players_.clear();
}

void BanallCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    if (!client || !client->get_player() || !g_core) {
        spdlog::error("[BANALL] No client or core available");
        return;
    }

    spdlog::info("[BANALL] Ban all command executed - {} players tracked", spawned_players_.size());
    
    for (const auto& name : spawned_players_) {
        spdlog::info("[BANALL] Player in list: '{}'", name);
    }

    int banned_count = 0;

    for (const auto& player_name : spawned_players_) {
        
        handle_spawn_packet(client->get_player(), player_name);
        banned_count++;
        
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    spdlog::info("[BANALL] Sent {} ban packets", banned_count);
}

void BanallCommand::handle_spawn_packet(player::Player* player, const std::string& player_name) {
    if (!player) {
        return;
    }

    
    std::string clean_name = player_name;
    
    
    clean_name.erase(0, clean_name.find_first_not_of(" \t\r\n"));
    
    clean_name.erase(clean_name.find_last_not_of(" \t\r\n") + 1);
    
    
    size_t pos = 0;
    while ((pos = clean_name.find('`')) != std::string::npos) {
        if (pos + 1 < clean_name.length()) {
            clean_name.erase(pos, 2);
        } else {
            clean_name.erase(pos, 1);
            break;
        }
    }
    
    
    clean_name.erase(std::remove(clean_name.begin(), clean_name.end(), '\''), clean_name.end());
    clean_name.erase(std::remove(clean_name.begin(), clean_name.end(), '"'), clean_name.end());
    
    
    std::transform(clean_name.begin(), clean_name.end(), clean_name.begin(), ::tolower);
    
    spdlog::info("[BANALL] Executing /ban {} (cleaned from: '{}')", clean_name, player_name);
    
    
    std::string ban_packet = fmt::format("action|input\ntext|/ban {}", clean_name);
    
    
    ByteStream<std::uint16_t> byte_stream{};
    byte_stream.write(packet::NET_MESSAGE_GENERIC_TEXT);
    byte_stream.write(ban_packet, false);
    
    player->send_packet(byte_stream.get_data(), 0);
}

} 
