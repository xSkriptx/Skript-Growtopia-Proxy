#include <magic_enum/magic_enum.hpp>
#include "../utils/proxy_logger.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/fmt/bin_to_hex.h>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <chrono>

#include "client.hpp"
#include "../utils/strenc.hpp"
#include "../packet/packet_helper.hpp"
#include "../packet/message/core.hpp"
#include "../packet/packet_variant.hpp"
#include "../server/server.hpp"
#include "../utils/network.hpp"
#include "../utils/player_tracker.hpp"
#include "../packet/filter.hpp"
#include "../extension/command_handler/join_command.hpp"
#include "../extension/command_handler/banall_command.hpp"
#include "../extension/command_handler/doorid_command.hpp"
#include "../extension/command_handler/moddetect_command.hpp"

#include "../utils/socks5_tunnel.hpp"
#include "../utils/inventory_manager.hpp"
#include "../utils/world_manager.hpp"
#include "../utils/world_info.h"
#include <cmath>

namespace client {
namespace {

std::chrono::steady_clock::time_point g_connect_attempt_time{};
bool g_connect_pending = false;
constexpr int CONNECT_TIMEOUT_SEC = 10;


void send_connection_error_msg(player::Player* to_player) {
    if (!to_player) return;
    packet::Variant var{};
    var.add("OnConsoleMessage");
    var.add("`4[Connection Error]`` Can't reach the server. Try using a `$VPN``, or `4close GT and the proxy then relog``.");
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
    to_player->send_packet(bs.get_data(), 0);
}


} 

Client::Client(core::Core* core)
    : core_{ core }
    , player_{ nullptr }
{
    host_ = enet_host_create(nullptr, 1, 2, 0, 0);
    if (!host_) {
        return;
    }

    if (enet_host_compress_with_range_coder(host_) != 0) {
        return;
    }

    host_->checksum = enet_crc32;
    host_->usingNewPacket = 1;

    core_->get_event_dispatcher().appendListener(
        core::EventType::Connection,
        [&](const core::EventConnection& evt)
        {
            if (evt.from != core::EventFrom::FromClient) {
                return;
            }

            
            if (const auto ext{ core_->get_extension(0x153bd697) }; ext) {
                return;
            }

            const core::Config& config{ core_->get_config() };
            player::Player* local = core_->get_server()->get_player();

            
            auto send_console = [&](const std::string& msg) {
                if (!local) return;
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
                local->send_packet(bs.get_data(), 0);
            };

            
            bool proxy_enabled = config.get<bool>("proxy.enabled");
            if (proxy_enabled) {
                std::string proxy_host = config.get<std::string>("proxy.host");
                uint16_t    proxy_port = static_cast<uint16_t>(
                                            config.get<unsigned int>("proxy.port"));
                std::string proxy_user = config.get<std::string>("proxy.username");
                std::string proxy_pass = config.get<std::string>("proxy.password");

                send_console("`5[SOCKS5]`` Connecting to proxy " + proxy_host
                             + ":" + std::to_string(proxy_port) + "...");
                spdlog::info("[SOCKS5] Connecting to proxy {}:{}", proxy_host, proxy_port);

                try {
                    socks5_tunnel::connect(proxy_host, proxy_port, proxy_user, proxy_pass);
                    send_console("`2[SOCKS5]`` Tunnel established — connecting to GT server...");
                    spdlog::info("[SOCKS5] Tunnel established successfully");
                } catch (const std::exception& e) {
                    send_console(std::string("`4[SOCKS5]`` Tunnel failed: ") + e.what());
                    spdlog::error("[SOCKS5] Tunnel failed: {}", e.what());
                    
                }
            } else {
                
                if (socks5_tunnel::is_active()) socks5_tunnel::disconnect();
            }

            std::ignore = connect(
                config.get<std::string>("server.address"),
                config.get<unsigned int>("server.port")
            );
            g_connect_attempt_time = std::chrono::steady_clock::now();
            g_connect_pending = true;
        }
    );
}

Client::~Client()
{
    enet_host_destroy(host_);
    delete player_;
}

ENetPeer* Client::connect(const std::string& host, const enet_uint16 port) const
{
    if (!host_) {
        return nullptr;
    }

    ENetAddress address{};
    enet_address_set_host(&address, host.c_str());
    address.port = port;

    return enet_host_connect(host_, &address, 2, 0);
}

void Client::process()
{
    if (!host_) {
        return;
    }

    
    if (g_connect_pending) {
        const auto now = std::chrono::steady_clock::now();
        const auto sec = std::chrono::duration_cast<std::chrono::seconds>(now - g_connect_attempt_time).count();
        if (sec >= CONNECT_TIMEOUT_SEC) {
            g_connect_pending = false;
            spdlog::warn("[CONNECTION] Server did not respond within {}s", CONNECT_TIMEOUT_SEC);
            const player::Player* local = core_->get_server()->get_player();
            send_connection_error_msg(const_cast<player::Player*>(local));
        }
    }

    ENetEvent ev{};
    while (enet_host_service(host_, &ev, 16) > 0) {
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

void Client::on_connect(ENetPeer* peer)
{
    g_connect_pending = false; 
    spdlog::info(
        "Server connection established: {}:{}",
        network::format_ip_address(peer->address.host),
        peer->address.port
    );

    player_ = new player::Player{ peer };

    core::EventConnection event_connection{ *player_ };
    event_connection.from = core::EventFrom::FromServer;
    core_->get_event_dispatcher().dispatch(event_connection);
}

void Client::on_receive(ENetPeer* peer, ENetPacket* packet)
{
    if (!player_) {
        enet_peer_disconnect(peer, 0);
        return;
    }

    player::Player* to_player{ core_->get_server()->get_player() };
    if (!to_player) {
        player_->disconnect();
        return;
    }

    ByteStream<std::uint16_t> byte_stream{ reinterpret_cast<std::byte*>(packet->data), packet->dataLength };
    
    
    constexpr std::size_t kMaxIncomingPacketSize = 8 * 1024 * 1024; 
    if (byte_stream.get_size() < 4 || byte_stream.get_size() > kMaxIncomingPacketSize) {
        spdlog::warn("Incoming packet size out of bounds: {} bytes", byte_stream.get_size());
        if (byte_stream.get_size() > kMaxIncomingPacketSize) {
            
            enet_packet_destroy(packet);
            return;
        }
        enet_packet_destroy(packet);
        player_->disconnect();
        return;
    }

    enet_packet_destroy(packet);

    packet::NetMessageType type{};
    if (!byte_stream.read(type)) {
        player_->disconnect();
        return;
    }

    
    packet::filter::FilterContext context{
        .source_player = player_,
        .target_player = to_player,
        .incoming = false, 
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
            handle_redirected_packet(byte_stream, to_player);
            return;
            
        case packet::filter::Action::ALLOW:
        default:
            break;
    }

    
    switch (type) {
        case packet::NET_MESSAGE_SERVER_HELLO:
            handle_server_hello(byte_stream, to_player);
            break;
            
        case packet::NET_MESSAGE_GENERIC_TEXT:
        case packet::NET_MESSAGE_GAME_MESSAGE:
            handle_text_message(byte_stream, to_player, peer);
            break;
            
        case packet::NET_MESSAGE_GAME_PACKET:
            handle_game_packet(byte_stream, to_player, peer);
            break;

        case packet::NET_MESSAGE_TRACK:
            
            
            to_player->send_packet(byte_stream.get_data(), 0);
            break;
            
        default:
            handle_unknown_packet(static_cast<std::uint32_t>(type), byte_stream, to_player, peer);
            break;
    }
}

void Client::handle_server_hello(ByteStream<std::uint16_t>& byte_stream, player::Player* to_player)
{
    packet::core::ServerHello server_hello{};
    packet::PacketHelper::send(server_hello, *to_player);
}

void Client::handle_text_message(ByteStream<std::uint16_t>& byte_stream, player::Player* to_player, ENetPeer* peer)
{
    std::string message{};
    byte_stream.read(message, byte_stream.get_size() - sizeof(packet::NetMessageType) - 1);

    TextParse text_parse{ message };

    auto send_console_local = [&](const std::string& msg) {
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
        to_player->send_packet(bs.get_data(), 0);
    };

    if (core_->get_config().get<bool>("log.printMessage")) {
        spdlog::info("Received server message:");
        for (const auto& key_value : text_parse.get_key_values()) {
            spdlog::info("  {}", key_value);
        }
    }

    
    if (!text_parse.get(DEC("tankIDName"), 0).empty() || !text_parse.get(DEC("ltoken"), 0).empty() || !text_parse.get(DEC("mac"), 0).empty()) {
        utils::ProxyLogger::log_login_packet(text_parse);
    }



    core::EventMessage event_message{ *player_, *to_player, text_parse };
    event_message.from = core::EventFrom::FromServer;
    core_->get_event_dispatcher().dispatch(event_message);

    if (!event_message.canceled) {
        to_player->send_packet(byte_stream.get_data(), 0);
    }
}

void Client::handle_game_packet(ByteStream<std::uint16_t>& byte_stream, player::Player* to_player, ENetPeer* peer)
{
    try {
        packet::GameUpdatePacket game_update_packet{};
        if (!byte_stream.read(game_update_packet)) {
            spdlog::warn("Failed to read game update packet header");
            
            return;
        }

        std::vector<std::byte> ext_data{};
        const std::size_t remaining_bytes = byte_stream.get_size() - byte_stream.get_read_offset();

        
        
        
        if (game_update_packet.type == packet::PACKET_SEND_MAP_DATA) {
            if (remaining_bytes > 0 &&
                game_update_packet.data_size > 0 &&
                game_update_packet.data_size != remaining_bytes) {
                spdlog::info(
                    "SEND_MAP_DATA: data_size field={} but packet has {} bytes remaining; using remaining bytes",
                    game_update_packet.data_size, remaining_bytes
                );
            }

            if (remaining_bytes > 0) {
                ext_data.resize(remaining_bytes);
                if (!byte_stream.read_data(ext_data.data(), remaining_bytes)) {
                    spdlog::warn("SEND_MAP_DATA: failed to read remaining {} bytes", remaining_bytes);
                    ext_data.clear();
                }
            }

            game_update_packet.data_size = static_cast<uint32_t>(ext_data.size());
        } else {
            if (game_update_packet.data_size > 0) {
                
                
                if (game_update_packet.data_size < 256 * 1024 * 1024) { 
                    if (!byte_stream.read_vector(ext_data, static_cast<std::size_t>(game_update_packet.data_size))) {
                        spdlog::warn("Failed to read extended data for game packet, expected: {}, available: {}", 
                                    game_update_packet.data_size, 
                                    byte_stream.get_size() - byte_stream.get_read_offset());
                        
                    }
                } else {
                    spdlog::warn("Extended data too large (>{}): {}", 256 * 1024 * 1024, game_update_packet.data_size);
                    
                }
            }
        }

        
        
        
        
        
        
        if (game_update_packet.type == packet::PACKET_ITEM_CHANGE_OBJECT) {
            const auto& raw = byte_stream.get_data();
            if (raw.size() >= 4 + 32) {
                const uint8_t* b = reinterpret_cast<const uint8_t*>(raw.data()) + 4;
                uint8_t  obj_type      = b[1];
                uint8_t  jump_count    = b[2];
                uint32_t pkt_net_id    = 0;
                float    float_var     = 0.f;
                uint32_t pkt_value     = 0;
                float    vec_x         = 0.f;
                float    vec_y         = 0.f;
                memcpy(&pkt_net_id, b + 4,  4);
                memcpy(&float_var,  b + 16, 4);
                memcpy(&pkt_value,  b + 20, 4);
                memcpy(&vec_x,      b + 24, 4);
                memcpy(&vec_y,      b + 28, 4);

                auto& wm = utils::WorldManager::get_instance();

                if (pkt_net_id == 0xFFFFFFFF) {
                    
                    
                    uint32_t new_uid = 1;
                    {
                        const auto& live  = wm.get_live_objects();
                        const auto& items = wm.get_items();
                        for (const auto& it : live)  if (it.Uid >= new_uid) new_uid = it.Uid + 1;
                        for (const auto& it : items) if (it.Uid >= new_uid) new_uid = it.Uid + 1;
                    }
                    world::DroppedItemInfo di{};
                    di.ItemId = static_cast<uint16_t>(pkt_value);
                    di.X      = std::ceil(vec_x);
                    di.Y      = std::ceil(vec_y);
                    di.Amount = static_cast<uint32_t>(static_cast<uint8_t>(float_var));
                    di.Flag   = obj_type;
                    di.Uid    = new_uid;
                    wm.add_dropped_item(di);
                    spdlog::info("[ITEM_DROP] id={} uid={} x={:.0f} y={:.0f} count={}",
                                 di.ItemId, di.Uid, di.X, di.Y, di.Amount);

                } else if (pkt_net_id == 0xFFFFFFFC) {
                    

                } else if (pkt_net_id > 0) {
                    
                    wm.remove_dropped_item_by_uid(pkt_value);
                    wm.remove_live_object(pkt_value);
                    spdlog::info("[ITEM_COLLECT] uid={} by net_id={}", pkt_value, pkt_net_id);
                }
            }
        }

        
        if (game_update_packet.type == packet::PACKET_CALL_FUNCTION && !ext_data.empty()) {
            try {
                
                packet::Variant variant{};
                if (variant.deserialize(ext_data)) {
                    auto variants = variant.get_variants();
                    if (variants.size() >= 2) {
                        const std::string function_name = variant.get<std::string>(0);

                        
                        if (function_name == "OnSpawn") {
                            
                            std::string spawn_data = variant.get<std::string>(1);
                            TextParse text_parse{ spawn_data };
                            
                            
                            std::string player_name = text_parse.get("name");
                            uint32_t net_id = 0;
                            std::string spawn_platform_id = text_parse.get("platformID");
                            try {
                                std::string net_id_str = text_parse.get("netID");
                                if (!net_id_str.empty()) {
                                    net_id = std::stoul(net_id_str);
                                }
                            } catch (...) {
                                
                            }
                            
                            if (!player_name.empty() && net_id > 0) {
                                
                                
                                player_name.erase(std::remove(player_name.begin(), player_name.end(), '\''), player_name.end());
                                player_name.erase(std::remove(player_name.begin(), player_name.end(), '"'), player_name.end());
                                
                                size_t pos = 0;
                                while ((pos = player_name.find('`')) != std::string::npos) {
                                    if (pos + 1 < player_name.length()) {
                                        player_name.erase(pos, 2);
                                    } else {
                                        player_name.erase(pos, 1);
                                        break;
                                    }
                                }
                                
                                player_name.erase(0, player_name.find_first_not_of(" \t\r\n"));
                                player_name.erase(player_name.find_last_not_of(" \t\r\n") + 1);
                                
                                if (!player_name.empty()) {
                                    
                                    utils::PlayerTracker::get_instance().update_player_name(net_id, player_name);
                                    if (!spawn_platform_id.empty()) {
                                        utils::PlayerTracker::get_instance().update_platform_info(net_id, spawn_platform_id);
                                    }
                                    spdlog::info("[SPAWN-TRACKER] Tracked player spawn: netid={}, name='{}'", net_id, player_name);
                                }
                            }
                            
                            
                            std::string spawn_type = text_parse.get("type");
                            if (spawn_type != "local") {
                                
                                player_name = text_parse.get("name");
                                
                                
                                player_name.erase(std::remove(player_name.begin(), player_name.end(), '\''), player_name.end());
                                player_name.erase(std::remove(player_name.begin(), player_name.end(), '"'), player_name.end());
                                
                                spdlog::info("[CLIENT-SPAWN] Got player_name from spawn: '{}' ({} bytes)", 
                                            player_name, player_name.length());
                                if (!player_name.empty() && core_->get_server() && core_->get_server()->get_player()) {
                                    spdlog::info("[CLIENT-SPAWN] Calling JoinCommand::handle_spawn_packet with name: '{}'", 
                                               player_name);
                                    command::JoinCommand::handle_spawn_packet(
                                        core_->get_server()->get_player(),
                                        player_name
                                    );
                                    
                                    
                                    command::BanallCommand::add_spawned_player(player_name);
                                }

                                
                                command::ModDetectCommand::handle_spawn_packet(spawn_data);
                            }
                            
                            
                            if (text_parse.get("type") == "local") {
                                spdlog::info("Detected local player spawn - applying zoom mod, JP flag, and balance console message");
                                
                                std::string modified_data = spawn_data;
                                auto replace_or_add_field = [](std::string& data, const std::string& field, const std::string& value) {
                                    std::string search_pattern = field + "|";
                                    size_t pos = data.find(search_pattern);
                                    if (pos != std::string::npos) {
                                        size_t value_start = pos + search_pattern.length();
                                        size_t value_end = data.find('\n', value_start);
                                        if (value_end == std::string::npos) {
                                            value_end = data.length();
                                        }
                                        data.replace(value_start, value_end - value_start, value);
                                    } else {
                                        size_t type_pos = data.find("type|local");
                                        if (type_pos != std::string::npos) {
                                            data.insert(type_pos, field + "|" + value + "\n");
                                        }
                                    }
                                };
                                
                                replace_or_add_field(modified_data, "country", "jp");
                                bool invis_enabled = core_->get_config().get<bool>("player.invis_enabled");
                                replace_or_add_field(modified_data, "invis", invis_enabled ? "1" : "0");
                                replace_or_add_field(modified_data, "mstate", "1");
                                bool sm_enabled = core_->get_config().get<bool>("player.sm_enabled");
                                replace_or_add_field(modified_data, "smstate", sm_enabled ? "1" : "0");
                                int title_icon = core_->get_config().get<int>("player.title_icon");
                                if (title_icon > 0) {
                                    replace_or_add_field(modified_data, "titleIcon", std::to_string(title_icon));
                                }
                                bool vision_enabled = core_->get_config().get<bool>("player.vision_enabled");
                                replace_or_add_field(modified_data, "IsNightVision", vision_enabled ? "1" : "0");
                                
                                auto local_info = utils::PlayerTracker::get_instance().get_local_player();
                                if (!local_info.platform_id.empty()) {
                                    replace_or_add_field(modified_data, "platformID", local_info.platform_id);
                                }
                                
                                packet::Variant modified_variant{};
                                modified_variant.add("OnSpawn");
                                modified_variant.add(modified_data);
                                std::vector<std::byte> modified_ext_data = modified_variant.serialize();
                                game_update_packet.data_size = static_cast<uint32_t>(modified_ext_data.size());
                                ByteStream<std::uint16_t> modified_byte_stream{};
                                modified_byte_stream.write(packet::NET_MESSAGE_GAME_PACKET);
                                modified_byte_stream.write(game_update_packet);
                                modified_byte_stream.write_data(modified_ext_data.data(), modified_ext_data.size());
                                byte_stream = std::move(modified_byte_stream);
                                spdlog::info("Applied zoom mod and JP flag to local player spawn");
                                
                                
                                if (sm_enabled) {
                                    packet::Variant warning_var{};
                                    warning_var.add("OnAddNotification");
                                    warning_var.add("interface/atomic_button.rttex");
                                    warning_var.add("`4Long Punch Enabled `w(BANNABLE use /sm to turn off)");
                                    warning_var.add("audio/hub_open.wav");
                                    warning_var.add(0);
                                    std::vector<std::byte> warning_ext_data = warning_var.serialize();
                                    packet::GameUpdatePacket warning_pkt{};
                                    warning_pkt.type = packet::PACKET_CALL_FUNCTION;
                                    warning_pkt.net_id = -1;
                                    warning_pkt.flags.extended = 1;
                                    warning_pkt.data_size = static_cast<uint32_t>(warning_ext_data.size());
                                    ByteStream<std::uint16_t> warning_bs{};
                                    warning_bs.write(packet::NET_MESSAGE_GAME_PACKET);
                                    warning_bs.write(warning_pkt);
                                    warning_bs.write_data(warning_ext_data.data(), warning_ext_data.size());
                                    to_player->send_packet(warning_bs.get_data(), 0);
                                    spdlog::warn("Sent long punch warning notification - SM mod is enabled");
                                }
                            }
                        }
                        
                        else if (function_name == "OnSendToServer") {
                            bool handled = false;
                            for (size_t i = 1; i < variants.size(); ++i) {
                                std::string server_info = variant.get<std::string>(i);
                                if (!server_info.empty()) {
                                    command::DoorIDCommand::handle_send_to_server(server_info);
                                    handled = true;
                                }
                            }

                            
                            if (!handled) {
                                std::string server_info = variant.get<std::string>(4);
                                if (!server_info.empty()) {
                                    command::DoorIDCommand::handle_send_to_server(server_info);
                                }
                            }
                        }
                    }
                }
            } catch (const std::exception& e) {
                spdlog::warn("Error processing variant packet: {}", e.what());
                
            }
        }

        
        try {
            core::EventPacket event_packet{ *player_, *to_player, game_update_packet, ext_data };
            event_packet.from = core::EventFrom::FromServer;
            core_->get_event_dispatcher().dispatch(event_packet);

            if (core_->get_config().get<bool>("log.printGameUpdatePacket")) {
                spdlog::info(
                    "Game packet: {} ({})",
                    magic_enum::enum_name(game_update_packet.type),
                    magic_enum::enum_integer(game_update_packet.type)
                );
            }

            if (!event_packet.canceled) {
                to_player->send_packet(byte_stream.get_data(), 0);
            }
        } catch (const std::exception& e) {
            spdlog::error("Error in game packet event dispatch: {}", e.what());
            
            to_player->send_packet(byte_stream.get_data(), 0);
        }
    } catch (const std::exception& e) {
        spdlog::error("Error processing game packet: {}", e.what());
        
        try {
            to_player->send_packet(byte_stream.get_data(), 0);
        } catch (const std::exception& send_error) {
            spdlog::error("Failed to forward game packet after error: {}", send_error.what());
        }
    }
}

void Client::handle_unknown_packet(std::uint32_t raw_type, ByteStream<std::uint16_t>& byte_stream, player::Player* to_player, ENetPeer* peer)
{
    const auto type = static_cast<packet::NetMessageType>(raw_type);

    spdlog::info(
        "Unknown packet from {}:{} - Type: {} ({})",
        network::format_ip_address(peer->address.host),
        peer->address.port,
        magic_enum::enum_name(type),
        raw_type
    );

    to_player->send_packet(byte_stream.get_data(), 0);
}

void Client::handle_redirected_packet(ByteStream<std::uint16_t>& byte_stream, player::Player* to_player) {
    spdlog::info("Handling redirected packet from server");
    
    
    to_player->send_packet(byte_stream.get_data(), 0);
}

void Client::on_disconnect(ENetPeer* peer)
{
    spdlog::info(
        "Server connection terminated: {}:{}",
        network::format_ip_address(peer->address.host),
        peer->address.port
    );

    
    if (socks5_tunnel::is_active()) {
        socks5_tunnel::disconnect();
        spdlog::info("[SOCKS5] Tunnel closed on server disconnect");
    }

    if (!player_) {
        return;
    }

    core::EventDisconnection event_disconnection{ *player_ };
    event_disconnection.from = core::EventFrom::FromServer;
    core_->get_event_dispatcher().dispatch(event_disconnection);

    delete player_;
    player_ = nullptr;

    const player::Player* to_player{ core_->get_server()->get_player() };
    if (!to_player) {
        return;
    }

    
    send_connection_error_msg(const_cast<player::Player*>(to_player));

    enet_host_flush(host_);
    to_player->disconnect_now();
    core_->get_server()->on_disconnect(to_player->get_peer());
}
}
