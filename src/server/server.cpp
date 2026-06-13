#include <magic_enum/magic_enum.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/bin_to_hex.h>
#include <fmt/format.h>
#include <thread>
#include <chrono>

#include "server.hpp"
#include "../client/client.hpp"
#include "../packet/packet_types.hpp"
#include "../packet/packet_variant.hpp"
#include "../utils/byte_stream.hpp"
#include "../utils/network.hpp"
#include "../utils/api_client.hpp"
#include "../utils/world_manager.hpp"
#include "../utils/player_tracker.hpp"
#include "../packet/filter.hpp"
#include "../extension/command_handler/doorid_command.hpp"
#include "../extension/command_handler/position_command.hpp"

namespace server {
namespace {
void send_doorid_console(player::Player* player, const std::string& msg) {
    if (!player) return;
    packet::Variant var{};
    var.add("OnConsoleMessage");
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
}

Server::Server(core::Core* core)
    : core_{ core }
    , player_{ nullptr }
{
    ENetAddress address{};
    address.host = ENET_HOST_ANY;
    
    
    unsigned int port = 17091; 
    try {
        port = core->get_config().get<unsigned int>("server.port");
    } catch (...) {
        
    }
    address.port = port;

    host_ = enet_host_create(&address, 1, 2, 0, 0);
    if (!host_) {
        spdlog::error("Failed to create server host on port {}", address.port);
        return;
    }

    if (enet_host_compress_with_range_coder(host_) != 0) {
        spdlog::warn("Failed to enable compression, continuing without it");
    }

    host_->checksum = enet_crc32;
    host_->usingNewPacketForServer = 1;
    
    spdlog::info("Server started on port {}", address.port);
}

Server::~Server()
{
    if (host_) {
        enet_host_destroy(host_);
    }
    if (player_) {
        delete player_;
        player_ = nullptr;
    }
}

void Server::process()
{
    if (!host_) {
        return;
    }

    ENetEvent ev{};
    while (enet_host_service(host_, &ev, 0) > 0) { 
        switch (ev.type) {
        case ENET_EVENT_TYPE_CONNECT:
            on_connect(ev.peer);
            break;
        case ENET_EVENT_TYPE_DISCONNECT:
            on_disconnect(ev.peer);
            break;
        case ENET_EVENT_TYPE_RECEIVE:
            on_receive(ev.peer, ev.packet);
            break;
        default:
            break;
        }
    }
}

void Server::on_connect(ENetPeer* peer)
{
    spdlog::info(
        "Client connected to proxy: {}:{}",
        network::format_ip_address(peer->address.host),
        peer->address.port
    );

    
    enet_peer_timeout(peer, 10000, 15000, 20000); 

    if (player_) {
        spdlog::warn("Another player is already connected, disconnecting previous");
        delete player_;
    }
    
    player_ = new player::Player{ peer };

    
    core::EventConnection event_connection{ *player_ };
    event_connection.from = core::EventFrom::FromClient;
    core_->get_event_dispatcher().dispatch(event_connection);

    
    utils::APIClient::set_online_connected(true);
    
    spdlog::info("Client connection ready");
}

void Server::on_receive(ENetPeer* peer, ENetPacket* packet)
{
    if (!player_) {
        spdlog::warn("Received packet but no player object, disconnecting peer");
        enet_peer_disconnect(peer, 0);
        enet_packet_destroy(packet);
        return;
    }

    
    spdlog::debug("Server received {} bytes from client", packet->dataLength);

    
    const player::Player* to_player = core_->get_client()->get_player();
    
    
    if (!to_player) {
        spdlog::warn("Real server client not ready yet, queuing packet...");
        
        
        for (int i = 0; i < 5; i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            to_player = core_->get_client()->get_player();
            if (to_player) {
                spdlog::info("Client connected after waiting {}ms", (i + 1) * 10);
                break;
            }
        }
        
        if (!to_player) {
            spdlog::warn("Real server still not connected, dropping packet");
            enet_packet_destroy(packet);
            return; 
        }
    }

    ByteStream<std::uint16_t> byte_stream{ reinterpret_cast<std::byte*>(packet->data), packet->dataLength };
    
    
    if (byte_stream.get_size() < 4) {
        spdlog::warn("Packet too small from client: {}", byte_stream.get_size());
        enet_packet_destroy(packet);
        return; 
    }
    
    if (byte_stream.get_size() > 65536) { 
        spdlog::warn("Packet too large from client: {}", byte_stream.get_size());
        enet_packet_destroy(packet);
        return; 
    }

    enet_packet_destroy(packet);

    packet::NetMessageType type{};
    if (!byte_stream.read(type)) {
        spdlog::warn("Failed to read packet type from client");
        return; 
    }

    
    try {
        packet::filter::FilterContext context{
            .source_player = player_,
            .target_player = const_cast<player::Player*>(to_player),
            .incoming = true, 
            .timestamp = std::chrono::steady_clock::now()
        };

        auto action = packet::filter::PacketFilter::get_instance().process_packet(type, byte_stream, context);
        
        switch (action) {
            case packet::filter::Action::BLOCK:
                spdlog::debug("Packet blocked by filter");
                return;
                
            case packet::filter::Action::MODIFY:
                spdlog::debug("Packet modified by filter");
                break;
                
            case packet::filter::Action::REDIRECT:
                spdlog::debug("Packet redirected by filter");
                handle_redirected_packet(byte_stream, const_cast<player::Player*>(to_player));
                return;
                
            case packet::filter::Action::ALLOW:
            default:
                break;
        }
    } catch (const std::exception& e) {
        spdlog::error("Error in packet filtering: {}", e.what());
        
    }

    
    if (type == packet::NET_MESSAGE_GENERIC_TEXT || type == packet::NET_MESSAGE_GAME_MESSAGE) {
        std::string message{};
        size_t remaining = byte_stream.get_size() - byte_stream.get_read_offset();
        if (remaining > 0 && remaining < 16384) {
            if (!byte_stream.read(message, remaining)) {
                spdlog::warn("Failed to read message content");
                return;
            }
        } else {
            spdlog::warn("Invalid message size: {}", remaining);
            return;
        }

        TextParse text_parse{ message };
        
        
        
        std::string action = text_parse.get("action");
        if (action == "join_request") {
            std::string name = text_parse.get("name");
            spdlog::info("[CLIENT-ACTION] join_request to: {}", name);
            command::BackCommand::note_join_request_target(name);
            
            
            if (!name.empty() && name.find('|') != std::string::npos) {
                size_t pipe_pos = name.find('|');
                std::string world_name = name.substr(0, pipe_pos);
                std::string door_id = name.substr(pipe_pos + 1);
                
                if (!door_id.empty()) {
                    spdlog::info("[DOOR-ENTRY] World: {}, Door ID: {}", world_name, door_id);
                    
                    
                    if (command::DoorIDCommand::is_door_id_reveal_enabled()) {
                        
                        auto* server = core_->get_server();
                        if (server && server->get_player()) {
                            packet::Variant var{};
                            var.add("OnConsoleMessage");
                            var.add(fmt::format("`9[ID Door Leaked]: `2{}", door_id));
                            
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
                            spdlog::info("[ID Door Leaked]: {}", door_id);
                        }
                    }
                }
            }
        }
        
        
        bool printMessages = false;
        try {
            printMessages = core_->get_config().get<bool>("log.printMessage");
        } catch (...) {
            
        }
        
        if (printMessages) {
            spdlog::info("Incoming message from client:");
            for (const auto& key_value : text_parse.get_key_values()) {
                spdlog::info("  {}", key_value);
            }
        }

        core::EventMessage event_message{ *player_, *to_player, text_parse };
        event_message.from = core::EventFrom::FromClient;
        core_->get_event_dispatcher().dispatch(event_message);

        if (!event_message.canceled) {
            to_player->send_packet(byte_stream.get_data(), 0);
        }

        
        
        
    }
    else if (type == packet::NET_MESSAGE_GAME_PACKET) {
        
        size_t start_pos = byte_stream.get_read_offset();
        
        try {
            packet::GameUpdatePacket game_update_packet{};
            if (!byte_stream.read(game_update_packet)) {
                spdlog::warn("Failed to read game update packet header");
                
                ByteStream<std::uint16_t> original_stream{ byte_stream.get_data().data(), byte_stream.get_size() };
                original_stream.skip(start_pos); 
                to_player->send_packet(original_stream.get_data(), 0);
                return;
            }

            std::vector<std::byte> ext_data{};
            if (game_update_packet.data_size > 0) {
                
                if (game_update_packet.data_size < 256 * 1024 * 1024) { 
                    if (!byte_stream.read_vector(ext_data, static_cast<std::size_t>(game_update_packet.data_size))) {
                        spdlog::warn("Failed to read extended data, expected: {}, available: {}", 
                                    game_update_packet.data_size, 
                                    byte_stream.get_size() - byte_stream.get_read_offset());
                        
                        ByteStream<std::uint16_t> original_stream{ byte_stream.get_data().data(), byte_stream.get_size() };
                        original_stream.skip(start_pos);
                        to_player->send_packet(original_stream.get_data(), 0);
                        return;
                    }
                } else {
                    spdlog::warn("Extended data too large (>{}): {}", 256 * 1024 * 1024, game_update_packet.data_size);
                    ByteStream<std::uint16_t> original_stream{ byte_stream.get_data().data(), byte_stream.get_size() };
                    original_stream.skip(start_pos);
                    to_player->send_packet(original_stream.get_data(), 0);
                    return;
                }
            }

            
            if (game_update_packet.type == packet::PACKET_DISCONNECT) {
                spdlog::info("Client sent disconnect packet");
                player_->disconnect_now();
                on_disconnect(peer);
                return;
            }

            
            
            if (game_update_packet.type == packet::PACKET_USE_DOOR &&
                command::DoorIDCommand::is_door_id_reveal_enabled()) {
                try {
                    auto world = utils::WorldManager::get_instance().get_world_v2();
                    if (world.is_valid && !world.tiles.empty()) {
                        int px = -1;
                        int py = -1;

                        
                        auto local = utils::PlayerTracker::get_instance().get_local_player();
                        if (local.netID > 0) {
                            const auto pos = utils::PlayerTracker::get_instance().get_player_position(local.netID);
                            px = static_cast<int>(pos.x / 32.0f);
                            py = static_cast<int>(pos.y / 32.0f);
                        }

                        
                        if (px < 0 || py < 0) {
                            try {
                                const std::string xstr = core_->get_config().get<std::string>("player.position.x");
                                const std::string ystr = core_->get_config().get<std::string>("player.position.y");
                                const float fx = std::stof(xstr);
                                const float fy = std::stof(ystr);
                                px = static_cast<int>(fx / 32.0f);
                                py = static_cast<int>(fy / 32.0f);
                            } catch (...) {
                                
                            }
                        }

                        const world_v2::Tile* best = nullptr;
                        int best_dist = 999999;
                        for (const auto& t : world.tiles) {
                            if (!(t.extra_type == 1 || t.extra_type == 2)) continue;
                            if (!t.door_data.has_data()) continue;
                            int manhattan = 0;
                            if (px >= 0 && py >= 0) {
                                const int dx = static_cast<int>(t.x) - px;
                                const int dy = static_cast<int>(t.y) - py;
                                manhattan = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
                            }
                            if (!best || manhattan < best_dist) {
                                best = &t;
                                best_dist = manhattan;
                            }
                        }

                        if (best && !best->door_data.destination.empty()) {
                            send_doorid_console(
                                core_->get_server()->get_player(),
                                fmt::format("`9[ID Door Leaked]: `2{}", best->door_data.destination)
                            );
                            spdlog::info("[ID Door Leaked] (use_door): {} (dist={})", best->door_data.destination, best_dist);
                        } else {
                            send_doorid_console(core_->get_server()->get_player(), "`9[ID Door Leaked]: `4No DoorID Found");
                            spdlog::info("[ID Door Leaked] (use_door): no matching door tile with destination");
                        }
                    } else {
                        send_doorid_console(core_->get_server()->get_player(), "`9[ID Door Leaked]: `4No DoorID Found");
                        spdlog::info("[ID Door Leaked] (use_door): world data unavailable");
                    }
                } catch (const std::exception& e) {
                    spdlog::warn("DoorID use_door resolve failed: {}", e.what());
                }
            }

            
            if (game_update_packet.type == packet::PACKET_CALL_FUNCTION) {
                spdlog::info("[SERVER-DEBUG] Got PACKET_CALL_FUNCTION from client, ext_data size: {}", ext_data.size());
                
                if (!ext_data.empty()) {
                    try {
                        packet::Variant variant{};
                        if (variant.deserialize(ext_data)) {
                            auto variants = variant.get_variants();
                            if (variants.size() >= 1) {
                                std::string func_name = variant.get<std::string>(0);
                                spdlog::info("[SERVER-VARIANT] Function: {}, Params: {}", func_name, variants.size());
                                
                                if (func_name == "OnSendToServer" && variants.size() >= 5) {
                                    std::string server_info = variant.get<std::string>(4);
                                    spdlog::info("[SERVER-VARIANT] OnSendToServer detected! Info: {}", server_info);
                                    command::DoorIDCommand::handle_send_to_server(server_info);
                                }
                            }
                        }
                    } catch (const std::exception& e) {
                        spdlog::error("Error processing variant from client: {}", e.what());
                    }
                }
            }

            try {
                core::EventPacket event_packet{ *player_, *to_player, game_update_packet, ext_data };
                event_packet.from = core::EventFrom::FromClient;
                core_->get_event_dispatcher().dispatch(event_packet);

                
                bool printGamePackets = false;
                try {
                    printGamePackets = core_->get_config().get<bool>("log.printGameUpdatePacket");
                } catch (...) {
                    
                }
                
                if (printGamePackets) {
                    spdlog::info(
                        "Incoming GameUpdatePacket {} ({}) from client",
                        magic_enum::enum_name(game_update_packet.type),
                        magic_enum::enum_integer(game_update_packet.type)
                    );
                }

                if (!event_packet.canceled) {
                    to_player->send_packet(byte_stream.get_data(), 0);
                }
            } catch (const std::exception& e) {
                spdlog::error("Error in game packet event: {}", e.what());
                
                ByteStream<std::uint16_t> original_stream{ byte_stream.get_data().data(), byte_stream.get_size() };
                original_stream.skip(start_pos);
                to_player->send_packet(original_stream.get_data(), 0);
            }
        } catch (const std::exception& e) {
            spdlog::error("Error processing game packet: {}", e.what());
            
            ByteStream<std::uint16_t> original_stream{ byte_stream.get_data().data(), byte_stream.get_size() };
            to_player->send_packet(original_stream.get_data(), 0);
        }
    }
    else {
        
        try {
            spdlog::debug(
                "Forwarding unknown packet from {}:{}",
                network::format_ip_address(peer->address.host),
                peer->address.port
            );
            to_player->send_packet(byte_stream.get_data(), 0);
        } catch (const std::exception& e) {
            spdlog::error("Error forwarding unknown packet: {}", e.what());
        }
    }
}

void Server::handle_redirected_packet(ByteStream<std::uint16_t>& byte_stream, player::Player* to_player) {
    try {
        spdlog::info("Handling redirected packet from client");
        to_player->send_packet(byte_stream.get_data(), 0);
    } catch (const std::exception& e) {
        spdlog::error("Error handling redirected packet: {}", e.what());
    }
}

void Server::on_disconnect(ENetPeer* peer)
{
    spdlog::info(
        "Client disconnected from proxy: {}:{}",
        network::format_ip_address(peer->address.host),
        peer->address.port
    );

    if (!player_) {
        return;
    }

    try {
        core::EventDisconnection event_disconnection{ *player_ };
        event_disconnection.from = core::EventFrom::FromClient;
        core_->get_event_dispatcher().dispatch(event_disconnection);
    } catch (const std::exception& e) {
        spdlog::error("Error in disconnect event: {}", e.what());
    }

    
    utils::APIClient::set_online_connected(false);

    
    const player::Player* to_player = core_->get_client()->get_player();
    if (to_player) {
        spdlog::info("Also disconnecting from real server");
        to_player->disconnect_now();
    }

    delete player_;
    player_ = nullptr;
    
    if (host_) {
        enet_host_flush(host_);
    }
}
}
