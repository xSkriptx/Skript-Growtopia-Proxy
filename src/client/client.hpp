#pragma once
#include <enet/enet.h>
#include <cstdint>

#include "../core/core.hpp"
#include "../player/player.hpp"
#include "../utils/byte_stream.hpp"

namespace client {
class Client final {
public:
    explicit Client(core::Core* core);
    ~Client();

    [[nodiscard]] ENetPeer* connect(const std::string& host, enet_uint16 port) const;
    void process();

    void on_connect(ENetPeer* peer);
    void on_receive(ENetPeer* peer, ENetPacket* packet);
    void on_disconnect(ENetPeer* peer);

    [[nodiscard]] player::Player* get_player() const { return player_; }
    [[nodiscard]] bool is_connected() const { return is_connected_; }  

private:
    
    void handle_server_hello(ByteStream<std::uint16_t>& byte_stream, player::Player* to_player);
    void handle_text_message(ByteStream<std::uint16_t>& byte_stream, player::Player* to_player, ENetPeer* peer);
    void handle_game_packet(ByteStream<std::uint16_t>& byte_stream, player::Player* to_player, ENetPeer* peer);
    void handle_unknown_packet(std::uint32_t raw_type, ByteStream<std::uint16_t>& byte_stream, player::Player* to_player, ENetPeer* peer);
    void handle_redirected_packet(ByteStream<std::uint16_t>& byte_stream, player::Player* to_player);

    ENetHost* host_;
    core::Core* core_;
    player::Player* player_;
    bool is_connected_;  
};
}
