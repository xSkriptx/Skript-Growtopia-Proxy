#include "lockefind_command.hpp"

#include "../../client/client.hpp"
#include "../../server/server.hpp"
#include "../../packet/packet_variant.hpp"
#include "../../packet/packet_types.hpp"
#include "../../utils/byte_stream.hpp"
#include "../../utils/api_client.hpp"
#include <sstream>
#include <algorithm>

namespace command {

core::Core* LockeFindCommand::g_core = nullptr;

static std::string sanitize_dialog_line(std::string text) {
    std::replace(text.begin(), text.end(), '\n', ' ');
    std::replace(text.begin(), text.end(), '\r', ' ');
    std::replace(text.begin(), text.end(), '|', '/');
    return text;
}

LockeFindCommand::LockeFindCommand() : CommandBase(
    {"lockefind"},
    {},
    "Show latest worlds where Locke stopped by",
    0
) {}

std::unique_ptr<CommandBase> LockeFindCommand::clone() const {
    return std::make_unique<LockeFindCommand>(*this);
}

void LockeFindCommand::set_core(core::Core* core) {
    g_core = core;
}

void LockeFindCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    if (!g_core || !client || !client->get_player()) {
        return;
    }

    auto* server = g_core->get_server();
    if (!server || !server->get_player()) {
        return;
    }

    std::string world_query;
    if (args.size() > 1) {
        for (size_t i = 1; i < args.size(); ++i) {
            if (!world_query.empty()) world_query += " ";
            world_query += args[i];
        }
    }

    auto rows = utils::APIClient::search_locke_data(world_query);

    std::ostringstream dialog;
    dialog << "set_default_color|`o\n";
    dialog << "add_label_with_icon|big|`wLocke Tracker``|left|2398|\n";
    dialog << "add_spacer|small|\n";
    dialog << "add_textbox|`2Tracked Locke sightings: `w" << rows.size() << "``|left|\n";
    dialog << "add_spacer|small|\n";

    if (rows.empty()) {
        dialog << "add_textbox|`4No Locke sightings found yet.``|left|\n";
    } else {
        constexpr size_t kShowMax = 100;
        size_t shown = 0;
        for (const auto& row : rows) {
            if (shown++ >= kShowMax) break;
            dialog << "add_smalltext|`6Locke in `w"
                   << sanitize_dialog_line(row.world_name)
                   << "`` `9(" << sanitize_dialog_line(row.time_str) << ")``|left|\n";
            dialog << "add_spacer|small|\n";
        }
    }

    dialog << "add_quick_exit|\n";
    dialog << "end_dialog|lockefind_view|Close||";

    packet::Variant variant{};
    variant.add("OnDialogRequest");
    variant.add(dialog.str());

    std::vector<std::byte> ext_data = variant.serialize();
    packet::GameUpdatePacket game_packet{};
    game_packet.type = packet::PACKET_CALL_FUNCTION;
    game_packet.net_id = -1;
    game_packet.flags.extended = 1;
    game_packet.data_size = static_cast<uint32_t>(ext_data.size());

    ByteStream<std::uint16_t> byte_stream{};
    byte_stream.write(packet::NET_MESSAGE_GAME_PACKET);
    byte_stream.write(game_packet);
    byte_stream.write_data(ext_data.data(), ext_data.size());
    server->get_player()->send_packet(byte_stream.get_data(), 0);
}

} 
