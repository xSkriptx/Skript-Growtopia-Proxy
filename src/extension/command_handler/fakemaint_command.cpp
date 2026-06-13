#include "fakemaint_command.hpp"
#include "../../client/client.hpp"
#include "../../server/server.hpp"
#include "../../core/core.hpp"
#include "../../packet/packet_variant.hpp"
#include "../../packet/packet_types.hpp"
#include "../../utils/packet_utils.hpp"
#include "../../utils/byte_stream.hpp"
#include <spdlog/spdlog.h>

namespace command {

FakeMaintCommand::FakeMaintCommand() : CommandBase(
    {"fakemaint"},
    {"reason"},
    "Send fake maintenance system messages",
    2
) {}

std::unique_ptr<CommandBase> FakeMaintCommand::clone() const {
    return std::make_unique<FakeMaintCommand>();
}

void FakeMaintCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    if (!client || !client->get_player()) {
        spdlog::error("FakeMaintCommand: No client or player!");
        return;
    }

    spdlog::warn("FakeMaintCommand::execute called without core - this shouldn't happen");
    utils::PacketUtils::send_chat_message(client->get_player(), "`4Error: Command requires core access.");
}

void FakeMaintCommand::execute_with_core(client::Client* client, const std::vector<std::string>& args, core::Core* core) {
    if (!client || !client->get_player()) {
        spdlog::error("FakeMaintCommand: No client or player!");
        return;
    }

    auto* server = core ? core->get_server() : nullptr;
    if (!server || !server->get_player()) {
        spdlog::error("FakeMaintCommand: No server available!");
        utils::PacketUtils::send_chat_message(client->get_player(), "`4Error: Server not available.");
        return;
    }

    if (args.size() < 2) {
        utils::PacketUtils::send_chat_message(client->get_player(), "`4Usage: /fakemaint <reason>");
        return;
    }

    std::string reason;
    for (size_t i = 1; i < args.size(); ++i) {
        if (i > 1) reason += " ";
        reason += args[i];
    }

    auto send_console_line = [&](const std::string& message) {
        packet::Variant var{};
        var.add("OnConsoleMessage");
        var.add(message);

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
    };

    send_console_line("`$** Global System Message: `4Restarting server in 10 minutes. " + reason);
    send_console_line("`4Global System Message``: Restarting server for update in `410 ``minutes");
}

} 
