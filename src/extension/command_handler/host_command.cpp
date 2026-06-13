#include "host_command.hpp"

#include "../../utils/packet_utils.hpp"
#include "../../server/server.hpp"
#include "../../packet/packet_variant.hpp"
#include "../../utils/byte_stream.hpp"

#include <sstream>

namespace command {

core::Core* HostCommand::s_core = nullptr;

HostCommand::HostCommand() : CommandBase(
    {"host"},
    {},
    "Open casino hoster settings",
    0
) {}

std::unique_ptr<CommandBase> HostCommand::clone() const {
    return std::make_unique<HostCommand>(*this);
}

void HostCommand::set_core(core::Core* core) {
    s_core = core;
}

void HostCommand::execute(client::Client* client, const std::vector<std::string>&) {
    if (!client || !client->get_player() || !s_core) return;
    auto* server = s_core->get_server();
    if (!server || !server->get_player()) return;
    send_settings_dialog(server->get_player());
}

void HostCommand::send_settings_dialog(player::Player* out) {
    if (!out || !s_core) return;

    auto getb = [&](const std::string& key, bool defv = false) -> bool {
        try { return s_core->get_config().get<bool>(key); }
        catch (...) { s_core->get_config().set<bool>(key, defv); return defv; }
    };

    const int show_real = getb("features.host.show_real_spin", false) ? 1 : 0;
    const int show_qq = getb("features.host.show_qq_number", false) ? 1 : 0;
    const int show_reme = getb("features.host.show_reme_spin", false) ? 1 : 0;
    const int instant = getb("features.host.instant_roulette_spin", false) ? 1 : 0;
    const int show_last = getb("features.host.show_last_spin", false) ? 1 : 0;

    std::ostringstream dialog;
    dialog << "set_default_color|`o\n";
    dialog << "add_label_with_icon|big|`wCasino Hoster Settings``|left|1366\n";
    dialog << "add_spacer|small\n";
    dialog << "add_checkbox|host_show_real|Show Real Spin|"<< show_real <<"\n";
    dialog << "add_checkbox|host_show_qq|Show QQ Number|"<< show_qq <<"\n";
    dialog << "add_checkbox|host_show_reme|Show REME Spin|"<< show_reme <<"\n";
    dialog << "add_checkbox|host_instant|Show Instant Roulette Spin|"<< instant <<"\n";
    dialog << "add_checkbox|host_show_last|Show Last Spin|"<< show_last <<"\n";
    dialog << "end_dialog|host_settings|Close|Save";

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

void HostCommand::apply_dialog_settings(const TextParse& tp) {
    if (!s_core) return;

    auto to_bool = [&](const std::string& v) -> bool {
        return v == "1" || v == "true" || v == "on";
    };

    const bool show_real = to_bool(tp.get("host_show_real"));
    const bool show_qq = to_bool(tp.get("host_show_qq"));
    const bool show_reme = to_bool(tp.get("host_show_reme"));
    const bool instant = to_bool(tp.get("host_instant"));
    const bool show_last = to_bool(tp.get("host_show_last"));

    s_core->get_config().set<bool>("features.host.show_real_spin", show_real);
    s_core->get_config().set<bool>("features.host.show_qq_number", show_qq);
    s_core->get_config().set<bool>("features.host.show_reme_spin", show_reme);
    s_core->get_config().set<bool>("features.host.instant_roulette_spin", instant);
    s_core->get_config().set<bool>("features.host.show_last_spin", show_last);

    if (s_core->get_server() && s_core->get_server()->get_player()) {
        utils::PacketUtils::send_chat_message(
            s_core->get_server()->get_player(),
            "`2Host settings updated``.",
            false
        );
    }
}

} 
