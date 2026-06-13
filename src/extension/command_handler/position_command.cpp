#include "position_command.hpp"
#include "utility_commands.hpp"  
#include "../../client/client.hpp"
#include "../../player/player.hpp"
#include "../../server/server.hpp"
#include "../../packet/packet_variant.hpp"
#include "../../packet/packet_types.hpp"
#include "../../utils/byte_stream.hpp"
#include "../../utils/player_tracker.hpp"
#include "../../utils/text_parse.hpp"
#include <spdlog/spdlog.h>
#include <fmt/format.h>

namespace command {

core::Core* PositionCommand::s_core = nullptr;
std::array<Position, 5> PositionCommand::s_positions = {};

core::Core* TeleportPosCommand::s_core = nullptr;
core::Core* BackCommand::s_core = nullptr;
std::string BackCommand::s_current_world{};
std::string BackCommand::s_previous_world{};

static void send_console(player::Player* player, const std::string& msg) {
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

static void send_generic_text(player::Player* player, const std::string& text) {
    if (!player) return;
    ByteStream<std::uint16_t> bs{};
    bs.write(packet::NET_MESSAGE_GENERIC_TEXT);
    bs.write(text, false);
    player->send_packet(bs.get_data(), 0);
}

PositionCommand::PositionCommand() : CommandBase(
    {"pos1", "pos2", "pos3", "pos4", "posback"},
    {},
    "Set drop/teleport positions (pos1-4 for drops, posback for return)",
    0
) {}

std::unique_ptr<CommandBase> PositionCommand::clone() const {
    return std::make_unique<PositionCommand>(*this);
}

void PositionCommand::set_core(core::Core* core) {
    s_core = core;
}

Position PositionCommand::get_position(int index) {
    if (index >= 0 && index < 5) {
        return s_positions[index];
    }
    return Position{};
}

void PositionCommand::set_position(int index, int x, int y) {
    if (index >= 0 && index < 5) {
        s_positions[index] = Position{x, y};
    }
}

void PositionCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    if (!s_core || !client || !client->get_player()) {
        spdlog::error("PositionCommand: No core or player!");
        return;
    }

    auto* server = s_core->get_server();
    if (!server || !server->get_player()) {
        spdlog::error("PositionCommand: No server player!");
        return;
    }

    
    int pos_index = -1;
    std::string pos_name;
    
    if (args.empty()) return;
    
    const std::string& cmd = args[0];
    if (cmd == "pos1") { pos_index = 0; pos_name = "1"; }
    else if (cmd == "pos2") { pos_index = 1; pos_name = "2"; }
    else if (cmd == "pos3") { pos_index = 2; pos_name = "3"; }
    else if (cmd == "pos4") { pos_index = 3; pos_name = "4"; }
    else if (cmd == "posback") { pos_index = 4; pos_name = "back"; }
    
    if (pos_index == -1) return;

    
    
    std::string pos_x_str = s_core->get_config().get<std::string>("player.position.x");
    std::string pos_y_str = s_core->get_config().get<std::string>("player.position.y");
    
    float pos_x = 0.0f;
    float pos_y = 0.0f;
    
    try {
        pos_x = std::stof(pos_x_str);
        pos_y = std::stof(pos_y_str);
    } catch (const std::exception& e) {
        send_console(server->get_player(), "`4Failed to get position - not tracked yet");
        spdlog::warn("PositionCommand: Failed to parse position from config");
        return;
    }
    
    
    int tile_x = static_cast<int>(pos_x / 32.0f);
    int tile_y = static_cast<int>(pos_y / 32.0f);
    
    spdlog::info("PositionCommand: Real-time position - pixels ({:.1f}, {:.1f}), tiles ({}, {})", 
                 pos_x, pos_y, tile_x, tile_y);
    
    s_positions[pos_index] = Position{tile_x, tile_y};
    
    std::string msg = fmt::format("`0[ `bSkriptProxy `0] `9pos {} set to `b{} `9,`b{}", 
                                   pos_name, tile_x, tile_y);
    send_console(server->get_player(), msg);
    
    
    int particle_id = (pos_index == 4) ? 356 : 354; 
    send_particle(server->get_player(), particle_id, 
                  static_cast<float>(tile_x * 32 + 16), 
                  static_cast<float>(tile_y * 32 + 16));
    
    spdlog::info("Set position {} to ({}, {})", pos_name, tile_x, tile_y);
}


TeleportPosCommand::TeleportPosCommand() : CommandBase(
    {"tp1", "tp2", "tp3", "tp4"},
    {},
    "Teleport to saved position (tp1-4)",
    0
) {}

std::unique_ptr<CommandBase> TeleportPosCommand::clone() const {
    return std::make_unique<TeleportPosCommand>(*this);
}

void TeleportPosCommand::set_core(core::Core* core) {
    s_core = core;
}

void TeleportPosCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    if (!s_core || !client || !client->get_player()) return;

    auto* server = s_core->get_server();
    if (!server || !server->get_player()) return;

    if (args.empty()) return;
    
    int pos_index = -1;
    const std::string& cmd = args[0];
    if (cmd == "tp1") pos_index = 0;
    else if (cmd == "tp2") pos_index = 1;
    else if (cmd == "tp3") pos_index = 2;
    else if (cmd == "tp4") pos_index = 3;
    
    if (pos_index == -1) return;

    Position pos = PositionCommand::get_position(pos_index);
    if (!pos.is_set()) {
        send_console(server->get_player(), "`4Pos Not Set");
        return;
    }

    
    
    uint32_t cur_tx = 0, cur_ty = 0;
    auto& tracker = utils::PlayerTracker::get_instance();
    auto local = tracker.get_local_player();
    if (local.netID != 0) {
        cur_tx = static_cast<uint32_t>(local.position.x / 32.0f);
        cur_ty = static_cast<uint32_t>(local.position.y / 32.0f);
        
        s_core->get_config().set<std::string>("player.position.x", std::to_string(local.position.x));
        s_core->get_config().set<std::string>("player.position.y", std::to_string(local.position.y));
        s_core->get_config().save();
    } else {
        
        spdlog::debug("TeleportPos: local player not tracked yet, using config position");
    }

    if (cur_tx == static_cast<uint32_t>(pos.x) && cur_ty == static_cast<uint32_t>(pos.y)) {
        send_console(server->get_player(), "`6Already at that saved position");
        return;
    }

    
    int steps = command::FindPathCommand::run_path(client, pos.x, pos.y, true, 1);
    if (steps > 0) {
        
        
        send_overlay(server->get_player(),
            fmt::format("`2Pathfind:`w {} steps to (`9{},{} `w) `o(fast)", steps, pos.x, pos.y));

        std::string msg = fmt::format("`0[ `bSkriptProxy `0] `9Pathing to pos{} at `b{} `9,`b{} (`9{} steps`9)",
                                       pos_index + 1, pos.x, pos.y, steps);
        send_console(server->get_player(), msg);
        spdlog::info("TeleportPos: Pathing to pos{} at ({}, {}), {} steps", pos_index + 1, pos.x, pos.y, steps);
    } else {
        send_console(server->get_player(), "`4Failed to path to position");
        spdlog::warn("TeleportPos: path failed for pos{} at ({}, {})", pos_index + 1, pos.x, pos.y);
    }
}


BackCommand::BackCommand() : CommandBase(
    {"back"},
    {},
    "Warp to the previously entered world",
    0
) {}

std::unique_ptr<CommandBase> BackCommand::clone() const {
    return std::make_unique<BackCommand>(*this);
}

void BackCommand::set_core(core::Core* core) {
    s_core = core;
}

void BackCommand::note_join_request_target(const std::string& raw_name) {
    if (raw_name.empty()) return;

    std::string world = raw_name;
    const size_t pipe_pos = world.find('|');
    if (pipe_pos != std::string::npos) {
        world = world.substr(0, pipe_pos);
    }

    const size_t space_pos = world.find(' ');
    if (space_pos != std::string::npos) {
        world = world.substr(0, space_pos);
    }

    if (world.empty()) return;
    if (!s_current_world.empty() && s_current_world == world) return;

    s_previous_world = s_current_world;
    s_current_world = world;
}

void BackCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    if (!s_core || !client || !client->get_player()) return;

    auto* server = s_core->get_server();
    if (!server || !server->get_player()) return;

    if (s_previous_world.empty()) {
        send_console(server->get_player(), "`4No previous world tracked yet.");
        return;
    }

    TextParse text_parse{};
    text_parse.add("action", {"join_request"});
    text_parse.add("name", {s_previous_world});
    text_parse.add("invitedWorld", {"0"});

    ByteStream<std::uint16_t> bs{};
    bs.write(packet::NET_MESSAGE_GAME_MESSAGE);
    bs.write(text_parse.get_raw(), false);
    client->get_player()->send_packet(bs.get_data(), 0);
    send_console(server->get_player(), fmt::format("`2Warping back to world: `5{}", s_previous_world));
    spdlog::info("BackCommand: warping to previous world '{}'", s_previous_world);
}

} 
