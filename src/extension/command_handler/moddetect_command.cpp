#include "moddetect_command.hpp"

#include "../../server/server.hpp"
#include "../../utils/text_parse.hpp"
#include "../../utils/packet_utils.hpp"
#include "../../packet/packet_variant.hpp"
#include "../../utils/byte_stream.hpp"

#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <unordered_map>
#include <algorithm>
#include <chrono>

namespace command {

core::Core* ModDetectCommand::s_core = nullptr;

namespace {
const std::unordered_map<std::string, std::string> kModerators = {
    {"30835728", "NekoRei"}, {"36310143", "Pangloss"}, {"41539504", "Hufflewitz"},
    {"41538133", "Pharaohboi"}, {"36709249", "vvCephei"}, {"36559671", "GadnokBOW"},
    {"25374", "Anulot"}, {"553625", "Jenuine"}, {"73346", "Aimster"},
    {"629331", "BlueDwarf"}, {"536707", "Misthios"}, {"35869182", "Ottowo"},
    {"29160268", "Sabaei"}, {"15006163", "Bleulabel"}, {"16966321", "Fournos"},
    {"32036726", "Kailyx"}, {"22353525", "Ubidev"}, {"22242821", "VectorCat"},
    {"25181947", "ThePsyborg"}, {"24233063", "nPlus1"}, {"24969470", "Qadevone"},
    {"36713808", "Gatello"}, {"308143", "Tharapita"}, {"41553179", "Phainomai"},
    {"41552957", "Santackles"}, {"3804202", "Explorate"}, {"41208310", "Akrius"},
    {"7275489", "Elbanna"}, {"43432233", "Serenite"}, {"38753466", "Lunatary"},
    {"46919291", "Cynced"}, {"42705852", "WindyPlay"}, {"41263973", "Trinculo"},
    {"47093010", "HollowDragon"}, {"47094621", "Sadilus"}, {"47091700", "Kintsugin"},
    {"47119248", "MrKeter"}, {"47120399", "Circulatum"}, {"44250099", "Caitriona"}
};

std::unordered_map<std::string, std::chrono::steady_clock::time_point> g_last_alert;

std::string clean_name(std::string name) {
    name.erase(std::remove(name.begin(), name.end(), '\''), name.end());
    name.erase(std::remove(name.begin(), name.end(), '"'), name.end());
    size_t pos = 0;
    while ((pos = name.find('`')) != std::string::npos) {
        if (pos + 1 < name.length()) name.erase(pos, 2);
        else {
            name.erase(pos, 1);
            break;
        }
    }
    if (!name.empty()) {
        const auto first = name.find_first_not_of(" \t\r\n");
        const auto last = name.find_last_not_of(" \t\r\n");
        if (first == std::string::npos) return {};
        name = name.substr(first, last - first + 1);
    }
    return name;
}

void send_mod_notification(core::Core* core, const std::string& mod_name, const std::string& uid) {
    if (!core || !core->get_server() || !core->get_server()->get_player()) return;
    auto* player = core->get_server()->get_player();

    packet::Variant var{};
    var.add("OnAddNotification");
    var.add("interface/atomic_button.rttex");
    var.add(fmt::format("`4Moderator Spawned``: `w{} `` (/run to escape)", mod_name));
    var.add("audio/hub_open.wav");
    var.add(0);

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
    player->send_packet(bs.get_data(), 0);

    utils::PacketUtils::send_chat_message(
        player,
        fmt::format("`4[MODDETECT]`` Moderator Spawned: `w{} ``(UID: {})", mod_name, uid),
        false
    );
}

void send_console_message(core::Core* core, const std::string& message) {
    if (!core || !core->get_server() || !core->get_server()->get_player()) return;
    auto* player = core->get_server()->get_player();

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
    player->send_packet(bs.get_data(), 0);
}
} 

ModDetectCommand::ModDetectCommand() : CommandBase(
    {"moddetect"},
    {},
    "Toggle moderator spawn detection",
    0
) {}

std::unique_ptr<CommandBase> ModDetectCommand::clone() const {
    return std::make_unique<ModDetectCommand>(*this);
}

void ModDetectCommand::set_core(core::Core* core) {
    s_core = core;
}

void ModDetectCommand::execute(client::Client* client, const std::vector<std::string>&) {
    if (!client || !client->get_player() || !s_core) return;

    
    bool enabled = true;
    try {
        enabled = s_core->get_config().get<bool>("features.moddetect");
    } catch (...) {
        enabled = true;
    }

    enabled = !enabled;
    s_core->get_config().set<bool>("features.moddetect", enabled);

    send_console_message(
        s_core,
        enabled ? "`2ModDetect enabled`` - watching moderator spawns."
                : "`4ModDetect disabled``."
    );
}

bool ModDetectCommand::is_enabled() {
    if (!s_core) return false;
    try {
        return s_core->get_config().get<bool>("features.moddetect");
    } catch (...) {
        return false;
    }
}

void ModDetectCommand::toggle() {
    
    ModDetectCommand cmd;
    
    cmd.execute(nullptr, {});
}

void ModDetectCommand::handle_spawn_packet(const std::string& spawn_data) {
    if (!s_core) return;

    bool enabled = true;
    try {
        enabled = s_core->get_config().get<bool>("features.moddetect");
    } catch (...) {
        
        enabled = true;
        s_core->get_config().set<bool>("features.moddetect", true);
    }
    if (!enabled) return;

    TextParse text_parse{ spawn_data };
    const std::string spawn_type = text_parse.get("type");
    if (spawn_type == "local") return;

    std::string uid = text_parse.get("userID");
    if (uid.empty()) uid = text_parse.get("userid");
    if (uid.empty()) return;

    auto it = kModerators.find(uid);
    if (it == kModerators.end()) return;

    const auto now = std::chrono::steady_clock::now();
    auto last_it = g_last_alert.find(uid);
    if (last_it != g_last_alert.end()) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_it->second).count();
        if (elapsed < 15) return;
    }
    g_last_alert[uid] = now;

    std::string spawn_name = clean_name(text_parse.get("name"));
    const std::string& known_name = it->second;
    const std::string final_name = spawn_name.empty() ? known_name : spawn_name;

    spdlog::warn("[MODDETECT] Moderator spawn detected: {} (uid={})", final_name, uid);
    send_mod_notification(s_core, final_name, uid);
}

} 
