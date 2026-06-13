#include "locketest001_command.hpp"

#include "../../client/client.hpp"
#include "../../server/server.hpp"
#include "../../packet/packet_variant.hpp"
#include "../../packet/packet_types.hpp"
#include "../../utils/byte_stream.hpp"
#include "../../utils/packet_utils.hpp"
#include "../../utils/api_client.hpp"

namespace command {

core::Core* LockeTest001Command::g_core = nullptr;

LockeTest001Command::LockeTest001Command() : CommandBase(
    {"locketest001"},
    {},
    "Send a test Locke OnConsoleMessage and upload a test sighting",
    0
) {}

std::unique_ptr<CommandBase> LockeTest001Command::clone() const {
    return std::make_unique<LockeTest001Command>(*this);
}

void LockeTest001Command::set_core(core::Core* core) {
    g_core = core;
}

void LockeTest001Command::execute(client::Client* client, const std::vector<std::string>& ) {
    if (!g_core || !client || !client->get_player()) {
        return;
    }
    auto* server = g_core->get_server();
    if (!server || !server->get_player()) {
        return;
    }

    constexpr const char* kWorld = "RTYW";
    const std::string test_msg = "`9Locke has stopped by the world `#" + std::string(kWorld) + "``! Go buy stuff from him!``";

    packet::Variant var{};
    var.add("OnConsoleMessage");
    var.add(test_msg);

    std::vector<std::byte> ext_data = var.serialize();
    packet::GameUpdatePacket game_packet{};
    game_packet.type = packet::PACKET_CALL_FUNCTION;
    game_packet.net_id = -1;
    game_packet.flags.extended = 1;
    game_packet.data_size = static_cast<uint32_t>(ext_data.size());

    ByteStream<std::uint16_t> bs{};
    bs.write(packet::NET_MESSAGE_GAME_PACKET);
    bs.write(game_packet);
    bs.write_data(ext_data.data(), ext_data.size());
    server->get_player()->send_packet(bs.get_data(), 0);

    const bool uploaded = utils::APIClient::upload_locke_data(kWorld);
    utils::PacketUtils::send_chat_message(
        server->get_player(),
        uploaded ? "`2Locke test sent + uploaded (`wRTYW`2)."
                 : "`4Locke test sent but upload failed.",
        false
    );
}

} 

