#include "dropat_command.hpp"
#include "position_command.hpp"
#include "../../client/client.hpp"
#include "../../player/player.hpp"
#include "../../server/server.hpp"
#include "../../packet/packet_variant.hpp"
#include "../../packet/packet_types.hpp"
#include "../../utils/byte_stream.hpp"
#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <thread>
#include <chrono>

namespace command {

core::Core* DropAtCommand::s_core = nullptr;
std::atomic<bool> DropAtCommand::s_running{false};

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

DropAtCommand::DropAtCommand() : CommandBase(
    {"w1", "w2", "w3", "w4"},
    {"<amount>"},
    "Drop WLs/DLs at saved position (e.g., /w1 150 drops 1dl 50wl at pos1)",
    1
) {}

std::unique_ptr<CommandBase> DropAtCommand::clone() const {
    return std::make_unique<DropAtCommand>(*this);
}

void DropAtCommand::set_core(core::Core* core) {
    s_core = core;
}

void DropAtCommand::execute(client::Client* client_ptr, const std::vector<std::string>& args) {
    if (!s_core || !client_ptr || !client_ptr->get_player()) {
        spdlog::error("DropAtCommand: No core or player!");
        return;
    }

    auto* server = s_core->get_server();
    if (!server || !server->get_player()) {
        spdlog::error("DropAtCommand: No server player!");
        return;
    }

    if (args.size() < 2) {
        send_console(server->get_player(), "`4Usage: /w1 <amount>");
        return;
    }

    
    int pos_index = -1;
    const std::string& cmd = args[0];
    if (cmd == "w1") pos_index = 0;
    else if (cmd == "w2") pos_index = 1;
    else if (cmd == "w3") pos_index = 2;
    else if (cmd == "w4") pos_index = 3;
    
    if (pos_index == -1) return;

    
    Position pos = PositionCommand::get_position(pos_index);
    if (!pos.is_set()) {
        send_console(server->get_player(), "`4Pos Not Set");
        return;
    }

    
    int amount = 0;
    try {
        amount = std::stoi(args[1]);
    } catch (...) {
        send_console(server->get_player(), "`4Invalid amount");
        return;
    }

    if (amount <= 0) {
        send_console(server->get_player(), "`4Amount must be positive");
        return;
    }

    if (s_running.exchange(true)) {
        send_console(server->get_player(), "`4Already dropping at a position");
        return;
    }

    
    std::thread([client_ptr, pos_index, amount]() {
        run_drop_at_position(client_ptr, pos_index, amount);
    }).detach();
}

void DropAtCommand::run_drop_at_position(client::Client* client_ptr, int pos_index, int amount) {
    auto finish = []() {
        DropAtCommand::s_running = false;
    };

    if (!s_core) {
        finish();
        return;
    }

    auto* server = s_core->get_server();
    if (!server || !server->get_player()) {
        finish();
        return;
    }

    Position pos = PositionCommand::get_position(pos_index);
    Position back_pos = PositionCommand::get_position(4); 

    
    send_particle(server->get_player(), 88, 
                  static_cast<float>(pos.x * 32 + 16), 
                  static_cast<float>(pos.y * 32 + 16));

    
    std::string move_packet = fmt::format("action|move\nx|{}\ny|{}", pos.x, pos.y);
    
    ByteStream<std::uint16_t> move_bs{};
    move_bs.write(packet::NET_MESSAGE_GENERIC_TEXT);
    move_bs.write(move_packet, false);

    
    client_ptr->get_player()->send_packet(move_bs.get_data(), 0);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    
    int dl_to_drop = amount / 100;
    int wl_to_drop = amount % 100;

    
    if (dl_to_drop > 0) {
        std::string drop_dl = fmt::format("action|drop\n|itemID|1796");
        send_generic_text(server->get_player(), drop_dl);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        std::string drop_count = fmt::format("action|dialog_return\ndialog_name|drop_item\nitemID|1796|\ncount|{}", dl_to_drop);
        send_generic_text(server->get_player(), drop_count);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    
    if (wl_to_drop > 0) {
        std::string drop_wl = fmt::format("action|drop\n|itemID|242");
        send_generic_text(server->get_player(), drop_wl);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        std::string drop_count = fmt::format("action|dialog_return\ndialog_name|drop_item\nitemID|242|\ncount|{}", wl_to_drop);
        send_generic_text(server->get_player(), drop_count);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    std::string msg = fmt::format("`0[ `bSkriptProxy `0] `9Dropping `b{}`9wls and `b{}`9dls", 
                                   wl_to_drop, dl_to_drop);
    send_console(server->get_player(), msg);

    
    if (back_pos.is_set()) {
        std::string back_packet = fmt::format("action|move\nx|{}\ny|{}", back_pos.x, back_pos.y);
        
        ByteStream<std::uint16_t> back_bs{};
        back_bs.write(packet::NET_MESSAGE_GENERIC_TEXT);
        back_bs.write(back_packet, false);

        
        client_ptr->get_player()->send_packet(back_bs.get_data(), 0);
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
    finish();
}

} 
