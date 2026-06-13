#include "door_command.hpp"
#include "../../client/client.hpp"
#include "../../player/player.hpp"
#include "../../utils/packet_utils.hpp"
#include "../../utils/world_manager.hpp"
#include <fmt/format.h>

namespace command {

DoorCommand::DoorCommand() : CommandBase(
    {"door"},
    {"door_id"},
    "Join a specific door ID in the current world",
    1
) {}

std::unique_ptr<CommandBase> DoorCommand::clone() const {
    return std::make_unique<DoorCommand>(*this);
}

void DoorCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    if (!client || !client->get_player()) {
        return;
    }

    
    if (args.size() < 2) {
        utils::PacketUtils::send_chat_message(client->get_player(), "`4Usage: /door <door_id>");
        return;
    }

    auto* player = client->get_player();
    const std::string& door_id = args[1];
    
    
    std::string world_name = utils::WorldManager::get_instance().get_world_name();
    
    if (world_name.empty()) {
        utils::PacketUtils::send_chat_message(player, "`4Error: You must be in a world to use /door");
        return;
    }

    
    utils::PacketUtils::send_chat_message(player, 
        fmt::format("`0[`bSkriptProxy`0] `9Joining door id: `b{}", door_id));
    
    
    send_door_packet(client, world_name, door_id);
}

void DoorCommand::send_door_packet(client::Client* client, const std::string& world_name, const std::string& door_id) {
    
    TextParse text_parse{};
    text_parse.add("action", {"join_request"});
    text_parse.add("name", {world_name + "|" + door_id + " "});  
    text_parse.add("invitedWorld", {"0"});

    
    ByteStream<std::uint16_t> byte_stream{};
    byte_stream.write(packet::NET_MESSAGE_GAME_MESSAGE);
    byte_stream.write(text_parse.get_raw(), false);

    
    if (client->get_player()) {
        client->get_player()->send_packet(byte_stream.get_data(), 0);
    }
}

} 
