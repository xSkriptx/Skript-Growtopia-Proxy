#include "eventmenu_command.hpp"
#include "../../client/client.hpp"
#include "../../player/player.hpp"
#include "../../utils/byte_stream.hpp"
#include "../../packet/packet_types.hpp"
#include <spdlog/spdlog.h>

namespace command {

EventMenuCommand::EventMenuCommand() : CommandBase(
    {"eventmenu"},
    {},
    "Opens the event menu",
    0
) {}

std::unique_ptr<CommandBase> EventMenuCommand::clone() const {
    return std::make_unique<EventMenuCommand>(*this);
}

void EventMenuCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    if (!client || !client->get_player()) {
        spdlog::error("EventMenuCommand: No player available");
        return;
    }

    
    std::string packet_data = "action|eventmenu\n";
    
    ByteStream<std::uint16_t> bs{};
    bs.write(packet::NET_MESSAGE_GENERIC_TEXT);
    bs.write(packet_data, false);

    client->get_player()->send_packet(bs.get_data(), 0);
    
    spdlog::info("[EventMenu] Sent eventmenu action to game server");
}

} 
