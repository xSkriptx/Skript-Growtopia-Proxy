#include "warp_command.hpp"
#include "../../client/client.hpp"
#include "../../player/player.hpp"
#include <fmt/format.h>

namespace command {

WarpCommand::WarpCommand() : CommandBase(
    {"warp", "exit"},
    {"world_name"},
    "Warp into the world specified by world_name (/exit warps to EXIT)",
    1
) {}

std::unique_ptr<CommandBase> WarpCommand::clone() const {
    return std::make_unique<WarpCommand>(*this);
}

void WarpCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    
    std::string command_name;
    if (!args.empty()) {
        command_name = args[0];
    }

    if (command_name == "exit") {
        const std::string world_name = "EXIT";
        send_warp_packet(client, world_name);

        packet::message::Log success_msg{};
        success_msg.msg = "`2Warping to world: `5EXIT";
        if (client->get_player()) {
            packet::PacketHelper::send(success_msg, *client->get_player());
        }
        return;
    }

    if (args.size() < 2) {
        
        packet::message::Log error_msg{};
        error_msg.msg = "`4Usage: /warp <world_name>";
        if (client->get_player()) {
            packet::PacketHelper::send(error_msg, *client->get_player());
        }
        return;
    }

    const std::string& world_name = args[1];
    send_warp_packet(client, world_name);
    
    
    packet::message::Log success_msg{};
    success_msg.msg = fmt::format("`2Warping to world: `5{}", world_name);
    if (client->get_player()) {
        packet::PacketHelper::send(success_msg, *client->get_player());
    }
}

void WarpCommand::send_warp_packet(client::Client* client, const std::string& world_name) {
    
    TextParse text_parse{};
    text_parse.add("action", {"join_request"});
    text_parse.add("name", {world_name});
    text_parse.add("invitedWorld", {"0"});

    
    ByteStream<std::uint16_t> byte_stream{};
    byte_stream.write(packet::NET_MESSAGE_GAME_MESSAGE);
    byte_stream.write(text_parse.get_raw(), false);

    
    if (client->get_player()) {
        client->get_player()->send_packet(byte_stream.get_data(), 0);
    }
}

} 
