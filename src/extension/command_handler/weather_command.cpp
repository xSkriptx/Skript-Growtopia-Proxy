#include "weather_command.hpp"
#include "../../client/client.hpp"
#include "../../player/player.hpp"
#include "../../server/server.hpp"
#include "../../packet/packet_variant.hpp"
#include "../../packet/packet_types.hpp"
#include "../../utils/byte_stream.hpp"
#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <random>

namespace command {

static core::Core* g_core = nullptr;

WeatherCommand::WeatherCommand() : CommandBase(
    {"weather"},
    {"weather_id"},
    "Change the world weather (1-255, or 0 for random)",
    0
) {}

std::unique_ptr<CommandBase> WeatherCommand::clone() const {
    return std::make_unique<WeatherCommand>(*this);
}

void WeatherCommand::set_core(core::Core* core) {
    g_core = core;
}

void WeatherCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    int weather_id = 0;
    
    
    if (args.size() < 2) {
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(1, 66);
        weather_id = dis(gen);
        spdlog::info("WeatherCommand: Using random weather ID: {}", weather_id);
    } else {
        
        try {
            weather_id = std::stoi(args[1]);
        } catch (...) {
            spdlog::error("WeatherCommand: Invalid weather ID '{}'", args[1]);
            return;
        }
        
        
        if (weather_id == 0) {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(1, 66);
            weather_id = dis(gen);
            spdlog::info("WeatherCommand: Using random weather ID: {}", weather_id);
        }
        
        else if (weather_id < 1 || weather_id > 255) {
            spdlog::error("WeatherCommand: Weather ID must be between 1-255 (got {})", weather_id);
            return;
        }
    }
    
    spdlog::info("WeatherCommand: Setting weather to ID {}", weather_id);
    send_weather_packet(client, weather_id);
}

void WeatherCommand::send_weather_packet(client::Client* client, int weather_id) {
    if (!client || !client->get_player()) {
        spdlog::error("WeatherCommand: No client or player!");
        return;
    }

    if (!g_core) {
        spdlog::error("WeatherCommand: No core set!");
        return;
    }

    
    auto* server = g_core->get_server();
    if (!server || !server->get_player()) {
        spdlog::error("WeatherCommand: No server player!");
        return;
    }
    
    
    packet::Variant variant{};
    variant.add("OnSetCurrentWeather");
    variant.add(weather_id);
    
    
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
    
    
    server->get_player()->send_packet(byte_stream.get_data(), 0);
    
    spdlog::info("WeatherCommand: Sent weather packet to game client (ID: {})", weather_id);
}

} 
