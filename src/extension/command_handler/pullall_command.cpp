#include "pullall_command.hpp"
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

core::Core* PullallCommand::g_core = nullptr;

PullallCommand::PullallCommand() : CommandBase(
    {"pullall"},
    {},
    "Pull all players who have spawned",
    0
) {}

std::unique_ptr<CommandBase> PullallCommand::clone() const {
    return std::make_unique<PullallCommand>(*this);
}

void PullallCommand::set_core(core::Core* core) {
    g_core = core;
}

void PullallCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    if (!client || !client->get_player() || !g_core) {
        spdlog::error("[PULLALL] No client or core available");
        return;
    }

    
    auto spawned_players = BanallCommand::get_spawned_players();
    
    spdlog::info("[PULLALL] Pull all command executed - {} players tracked", spawned_players.size());
    
    for (const auto& name : spawned_players) {
        spdlog::info("[PULLALL] Player in list: '{}'", name);
    }

    int pulled_count = 0;

    for (const auto& player_name : spawned_players) {
        
        handle_spawn_packet(client->get_player(), player_name);
        pulled_count++;
        
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    spdlog::info("[PULLALL] Sent {} pull packets", pulled_count);
}

void PullallCommand::handle_spawn_packet(player::Player* player, const std::string& player_name) {
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
    
    spdlog::info("[PULLALL] Executing /pull {} (cleaned from: '{}')", clean_name, player_name);
    
    
    std::string pull_packet = fmt::format("action|input\ntext|/pull {}", clean_name);
    
    
    ByteStream<std::uint16_t> byte_stream{};
    byte_stream.write(packet::NET_MESSAGE_GENERIC_TEXT);
    byte_stream.write(pull_packet, false);
    
    player->send_packet(byte_stream.get_data(), 0);
}

} 
