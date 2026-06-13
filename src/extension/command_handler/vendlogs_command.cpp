#include "vendlogs_command.hpp"
#include "../../client/client.hpp"
#include "../../server/server.hpp"
#include "../../packet/packet_variant.hpp"
#include "../../packet/packet_types.hpp"
#include "../../utils/byte_stream.hpp"
#include "../../utils/vend_logs_store.hpp"
#include <algorithm>
#include <sstream>
#include <spdlog/spdlog.h>
#include <fmt/format.h>

namespace command {

core::Core* VendLogsCommand::g_core = nullptr;

static std::string sanitize_dialog_line(std::string text) {
    std::replace(text.begin(), text.end(), '\n', ' ');
    std::replace(text.begin(), text.end(), '\r', ' ');
    std::replace(text.begin(), text.end(), '|', '/');
    return text;
}

void VendLogsCommand::set_core(core::Core* core) {
    g_core = core;
}

VendLogsCommand::VendLogsCommand() : CommandBase(
    {"vendlogs"},
    {},
    "Show captured vending purchase logs from OnConsoleMessage",
    0
) {}

std::unique_ptr<CommandBase> VendLogsCommand::clone() const {
    return std::make_unique<VendLogsCommand>(*this);
}

void VendLogsCommand::execute(client::Client* client, const std::vector<std::string>& ) {
    if (!g_core || !client || !client->get_player()) {
        spdlog::error("VendLogsCommand: No core/client/player.");
        return;
    }

    auto* server = g_core->get_server();
    if (!server || !server->get_player()) {
        spdlog::error("VendLogsCommand: No server player.");
        return;
    }

    auto logs = utils::VendLogsStore::snapshot();

    std::ostringstream dialog;
    dialog << "set_default_color|`o\n";
    dialog << "add_label_with_icon|big|`wVend Logs``|left|2978|\n";
    dialog << "add_spacer|small|\n";
    dialog << fmt::format("add_textbox|`2Captured purchases: `w{} ``|left|\n", logs.size());
    dialog << "add_spacer|small|\n";

    if (logs.empty()) {
        dialog << "add_textbox|`4No vend purchase logs captured yet.``|left|\n";
    } else {
        constexpr size_t kShowMax = 80;
        size_t shown = 0;
        for (auto it = logs.rbegin(); it != logs.rend() && shown < kShowMax; ++it, ++shown) {
            const auto msg = sanitize_dialog_line(it->message);
            dialog << fmt::format("add_smalltext|`9[{}] `w{} ``|\n", it->time_str, msg);
            dialog << "add_spacer|small|\n";
        }
        if (logs.size() > kShowMax) {
            dialog << fmt::format("add_textbox|`oShowing latest {} of {} logs.``|left|\n", kShowMax, logs.size());
        }
    }

    dialog << "add_quick_exit|\n";
    dialog << "end_dialog|vendlogs_view|Close||";

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

