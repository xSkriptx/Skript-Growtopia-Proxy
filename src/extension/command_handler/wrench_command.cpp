#include "wrench_command.hpp"

#include "../../packet/packet_variant.hpp"
#include "../../packet/packet_types.hpp"
#include "../../server/server.hpp"
#include "../../utils/byte_stream.hpp"
#include "../../utils/packet_utils.hpp"

#include <sstream>

namespace command {

core::Core* WrenchCommand::s_core = nullptr;

WrenchCommand::WrenchCommand() : CommandBase(
    {"wrench", "awr"},
    {},
    "Open auto-wrench settings (pull/kick/ban)",
    0
) {}

std::unique_ptr<CommandBase> WrenchCommand::clone() const {
    return std::make_unique<WrenchCommand>(*this);
}

void WrenchCommand::set_core(core::Core* core) {
    s_core = core;
}

void WrenchCommand::execute(client::Client* client, const std::vector<std::string>&) {
    if (!client || !client->get_player() || !s_core) return;
    auto* server = s_core->get_server();
    if (!server || !server->get_player()) return;
    send_settings_dialog(server->get_player());
}

void WrenchCommand::send_settings_dialog(player::Player* out) {
    if (!out || !s_core) return;

    auto getb = [&](const std::string& key, bool defv = false) -> bool {
        try { return s_core->get_config().get<bool>(key); }
        catch (...) { s_core->get_config().set<bool>(key, defv); return defv; }
    };

    const int auto_pull = getb("features.wrench.auto_pull", false) ? 1 : 0;
    const int auto_kick = getb("features.wrench.auto_kick", false) ? 1 : 0;
    const int auto_ban = getb("features.wrench.auto_ban", false) ? 1 : 0;

    std::ostringstream dialog;
    dialog << "set_default_color|`o\n";
    dialog << "add_label_with_icon|big|`wAuto Wrench Settings``|left|18\n";
    dialog << "add_spacer|small\n";
    dialog << "add_checkbox|wrench_auto_pull|Auto Wrench Pull|"<< auto_pull <<"\n";
    dialog << "add_checkbox|wrench_auto_kick|Auto Wrench Kick|"<< auto_kick <<"\n";
    dialog << "add_checkbox|wrench_auto_ban|Auto Wrench Ban|"<< auto_ban <<"\n";
    dialog << "add_smalltext|Priority: Ban > Kick > Pull (when multiple enabled)|left\n";
    dialog << "end_dialog|wrench_settings|Close|Save";

    packet::Variant var{};
    var.add("OnDialogRequest");
    var.add(dialog.str());
    std::vector<std::byte> ext = var.serialize();

    packet::GameUpdatePacket pkt{};
    pkt.type = packet::PACKET_CALL_FUNCTION;
    pkt.net_id = -1;
    pkt.flags.extended = 1;
    pkt.data_size = static_cast<uint32_t>(ext.size());

    ByteStream<std::uint16_t> bs{};
    bs.write(packet::NET_MESSAGE_GAME_PACKET);
    bs.write(pkt);
    bs.write_data(ext.data(), ext.size());
    out->send_packet(bs.get_data(), 0);
}

void WrenchCommand::apply_dialog_settings(const TextParse& tp) {
    if (!s_core) return;

    auto to_bool = [](const std::string& v) -> bool {
        return v == "1" || v == "true" || v == "on";
    };

    const bool auto_pull = to_bool(tp.get("wrench_auto_pull"));
    const bool auto_kick = to_bool(tp.get("wrench_auto_kick"));
    const bool auto_ban = to_bool(tp.get("wrench_auto_ban"));

    s_core->get_config().set<bool>("features.wrench.auto_pull", auto_pull);
    s_core->get_config().set<bool>("features.wrench.auto_kick", auto_kick);
    s_core->get_config().set<bool>("features.wrench.auto_ban", auto_ban);

    if (s_core->get_server() && s_core->get_server()->get_player()) {
        utils::PacketUtils::send_chat_message(
            s_core->get_server()->get_player(),
            "`2Wrench settings updated``.",
            false
        );
    }
}

} 

