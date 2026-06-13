#include "mailclaim_command.hpp"
#include "../../client/client.hpp"
#include "../../player/player.hpp"
#include "../../utils/byte_stream.hpp"
#include "../../packet/packet_types.hpp"
#include <spdlog/spdlog.h>
#include <fmt/format.h>

namespace command {

MailClaimCommand::MailClaimCommand() : CommandBase(
    {"mailclaim"},
    {},
    "Claim mail reward by message ID - Usage: /mailclaim <message_id>",
    1
) {}

std::unique_ptr<CommandBase> MailClaimCommand::clone() const {
    return std::make_unique<MailClaimCommand>(*this);
}

void MailClaimCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    if (!client || !client->get_player()) {
        spdlog::error("MailClaimCommand: No player available");
        return;
    }

    if (args.empty()) {
        spdlog::warn("MailClaimCommand: Usage: /mailclaim <message_id>");
        return;
    }

    const std::string& message_id = args[0];
    
    
    std::string packet_data = fmt::format("action|mailbox_claimreward\nmessageID|{}\n", message_id);
    
    ByteStream<std::uint16_t> bs{};
    bs.write(packet::NET_MESSAGE_GENERIC_TEXT);
    bs.write(packet_data, false);

    client->get_player()->send_packet(bs.get_data(), 0);
    
    spdlog::info("[MailClaim] Claiming reward for message ID: {} (sent to game server)", message_id);
}

} 
