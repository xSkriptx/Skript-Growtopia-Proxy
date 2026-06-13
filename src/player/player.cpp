#include "player.hpp"
#include <cstring>
#include <string>

namespace player {
bool Player::send_packet(const std::vector<std::byte>& data, const int channel) const
{
    if (data.size() < 4 || data.size() > 786432 ) {
        return false;
    }

    ENetPacket* packet{ enet_packet_create(data.data(), data.size(), ENET_PACKET_FLAG_RELIABLE) };
    if (const int ret{ enet_peer_send(peer_, channel, packet) }; ret != 0) {
        enet_packet_destroy(packet);
        return false;
    }

    return true;
}

bool Player::send_packet_unreliable(const std::vector<std::byte>& data, const int channel) const
{
    if (data.size() < 4 || data.size() > 786432 ) {
        return false;
    }

    ENetPacket* packet{ enet_packet_create(data.data(), data.size(), 0) };
    if (const int ret{ enet_peer_send(peer_, channel, packet) }; ret != 0) {
        enet_packet_destroy(packet);
        return false;
    }

    return true;
}

void Player::send_log(const std::string& message) const
{
    if (!is_connected()) {
        return;
    }

    std::string full_message = "action|log\nmsg|" + message;
    
    std::vector<std::byte> packet_data;
    packet_data.resize(sizeof(uint32_t) + sizeof(uint16_t) + full_message.length());
    
    
    uint32_t message_type = 3;
    std::memcpy(packet_data.data(), &message_type, sizeof(uint32_t));
    
    
    uint16_t str_len = static_cast<uint16_t>(full_message.length());
    std::memcpy(packet_data.data() + sizeof(uint32_t), &str_len, sizeof(uint16_t));
    
    
    std::memcpy(packet_data.data() + sizeof(uint32_t) + sizeof(uint16_t), 
                full_message.c_str(), full_message.length());
    
    send_packet(packet_data);
}
} 
