#include "join_command.hpp"
#include "../../client/client.hpp"
#include "../../player/player.hpp"
#include "../../server/server.hpp"
#include "../../utils/packet_utils.hpp"
#include "../../utils/text_parse.hpp"
#include "../../packet/packet_variant.hpp"
#include "../../packet/packet_types.hpp"
#include "../../utils/byte_stream.hpp"
#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace command {

static core::Core* g_core = nullptr;
static bool g_pull_mode_enabled = false;
static bool g_kick_mode_enabled = false;
static bool g_ban_mode_enabled = false;

JoinCommand::JoinCommand() : CommandBase(
    {"join", "j"},
    {},
    "Show join mode menu (pull/kick/ban)",
    0
) {}

std::unique_ptr<CommandBase> JoinCommand::clone() const {
    return std::make_unique<JoinCommand>(*this);
}

void JoinCommand::set_core(core::Core* core) {
    g_core = core;
}

void JoinCommand::toggle_pull_mode() {
    g_pull_mode_enabled = !g_pull_mode_enabled;
    spdlog::info("JoinCommand: Pull mode {}", g_pull_mode_enabled ? "ENABLED" : "DISABLED");
}

void JoinCommand::toggle_kick_mode() {
    g_kick_mode_enabled = !g_kick_mode_enabled;
    spdlog::info("JoinCommand: Kick mode {}", g_kick_mode_enabled ? "ENABLED" : "DISABLED");
}

void JoinCommand::toggle_ban_mode() {
    g_ban_mode_enabled = !g_ban_mode_enabled;
    spdlog::info("JoinCommand: Ban mode {}", g_ban_mode_enabled ? "ENABLED" : "DISABLED");
}

bool JoinCommand::is_pull_mode_enabled() {
    return g_pull_mode_enabled;
}

bool JoinCommand::is_kick_mode_enabled() {
    return g_kick_mode_enabled;
}

bool JoinCommand::is_ban_mode_enabled() {
    return g_ban_mode_enabled;
}

std::string JoinCommand::get_current_mode() {
    if (g_pull_mode_enabled) return "pull";
    if (g_kick_mode_enabled) return "kick";
    if (g_ban_mode_enabled) return "ban";
    return "off";
}

void JoinCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    if (!client || !client->get_player()) {
        spdlog::error("JoinCommand: No client or player!");
        return;
    }

    if (!g_core) {
        spdlog::error("JoinCommand: No core set!");
        return;
    }

    show_join_gui(client);
}

void JoinCommand::show_join_gui(client::Client* client) {
    auto* server = g_core->get_server();
    if (!server || !server->get_player()) {
        spdlog::error("JoinCommand: No server player!");
        return;
    }

    std::ostringstream dialog;
    dialog << "set_default_color|`o\n";
    dialog << "add_label_with_icon|big|`wJoin Mode Menu``|left|6|\n";
    dialog << "add_spacer|small|\n";
    
    dialog << "add_textbox|`5Select a join mode:``|left|\n";
    dialog << "add_spacer|small|\n";
    
    
    std::string pull_status = g_pull_mode_enabled ? "`2ENABLED``" : "`4DISABLED``";
    dialog << "add_label_with_icon|big|`2Join Pull``|left|6|\n";
    dialog << "add_smalltext|`oAuto-pull players when they join``|\n";
    dialog << "add_smalltext|Status: " << pull_status << "``|\n";
    dialog << "add_button|toggle_pull|`2Toggle Pull``|\n";
    dialog << "add_spacer|small|\n";
    
    
    std::string kick_status = g_kick_mode_enabled ? "`3ENABLED``" : "`4DISABLED``";
    dialog << "add_label_with_icon|big|`3Join Kick``|left|6|\n";
    dialog << "add_smalltext|`oAuto-kick players when they join``|\n";
    dialog << "add_smalltext|Status: " << kick_status << "``|\n";
    dialog << "add_button|toggle_kick|`3Toggle Kick``|\n";
    dialog << "add_spacer|small|\n";
    
    
    std::string ban_status = g_ban_mode_enabled ? "`4ENABLED``" : "`4DISABLED``";
    dialog << "add_label_with_icon|big|`4Join Ban``|left|6|\n";
    dialog << "add_smalltext|`oAuto-ban players when they join``|\n";
    dialog << "add_smalltext|Status: " << ban_status << "``|\n";
    dialog << "add_button|toggle_ban|`4Toggle Ban``|\n";
    dialog << "add_spacer|small|\n";
    
    dialog << "add_textbox|`4Note: Toggle modes to enable/disable the feature``|left|\n";
    dialog << "add_spacer|small|\n";
    dialog << "add_quick_exit|\n";
    dialog << "end_dialog|join_commands|Close||\n";

    std::string dialog_data = dialog.str();

    packet::Variant variant{};
    variant.add("OnDialogRequest");
    variant.add(dialog_data);

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
    
    spdlog::info("JoinCommand: Sent join mode GUI");
}

void JoinCommand::handle_dialog_click(player::Player* player, const std::string& button_clicked) {
    
    
    spdlog::info("[JOIN] handle_dialog_click called (should be handled by extension)");
}

void JoinCommand::handle_spawn_packet(player::Player* player, const std::string& player_name) {
    if (!player || !g_core) {
        return;
    }

    
    auto client = g_core->get_client();
    if (!client || !client->get_player()) {
        spdlog::error("[JOIN] No client player available to send command");
        return;
    }

    std::string mode = get_current_mode();
    if (mode == "off") {
        return;
    }

    
    std::string clean_name = player_name;
    
    
    clean_name.erase(0, clean_name.find_first_not_of(" \t\r\n"));
    
    clean_name.erase(clean_name.find_last_not_of(" \t\r\n") + 1);
    
    
    size_t pos = 0;
    while ((pos = clean_name.find('`')) != std::string::npos) {
        if (pos + 1 < clean_name.length()) {
            clean_name.erase(pos, 2);
        } else {
            clean_name.erase(pos, 1);
            break;
        }
    }
    
    
    clean_name.erase(std::remove(clean_name.begin(), clean_name.end(), '\''), clean_name.end());
    clean_name.erase(std::remove(clean_name.begin(), clean_name.end(), '"'), clean_name.end());
    
    
    std::transform(clean_name.begin(), clean_name.end(), clean_name.begin(), ::tolower);
    
    spdlog::info("[JOIN] Spawned player '{}' (final cleaned: '{}') - executing: /{} {}", 
                 player_name, clean_name, mode, clean_name);
    
    
    std::string command_packet = fmt::format("action|input\ntext|/{} {}", mode, clean_name);
    
    spdlog::info("[JOIN] Packet format: '{}' ({} bytes)", command_packet, command_packet.length());
    
    
    ByteStream<std::uint16_t> byte_stream{};
    byte_stream.write(packet::NET_MESSAGE_GENERIC_TEXT);
    byte_stream.write(command_packet, false);
    
    auto packet_data = byte_stream.get_data();
    spdlog::info("[JOIN] Final packet size: {} bytes", packet_data.size());
    
    
    client->get_player()->send_packet(packet_data, 0);
    
    spdlog::info("[JOIN] Sent packet - mode: '{}', player: '{}'", mode, clean_name);
}

} 

