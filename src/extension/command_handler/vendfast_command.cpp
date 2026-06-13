#include "vendfast_command.hpp"
#include "../../client/client.hpp"
#include "../../player/player.hpp"
#include "../../server/server.hpp"
#include "../../utils/packet_utils.hpp"
#include "../../packet/packet_variant.hpp"
#include "../../packet/packet_types.hpp"
#include "../../utils/byte_stream.hpp"
#include <spdlog/spdlog.h>
#include <sstream>

namespace command {

static core::Core* g_core = nullptr;
static bool g_empty_mode_enabled = false;
static bool g_add_mode_enabled = false;
static bool g_buy_mode_enabled = false;
static int g_buy_count = 1; 

VendFastCommand::VendFastCommand() : CommandBase(
    {"vendfast", "vf"},
    {},
    "Show vending fast actions menu (empty/add/buy)",
    0
) {}

std::unique_ptr<CommandBase> VendFastCommand::clone() const {
    return std::make_unique<VendFastCommand>(*this);
}

void VendFastCommand::set_core(core::Core* core) {
    g_core = core;
}

void VendFastCommand::toggle_empty_mode() {
    g_empty_mode_enabled = !g_empty_mode_enabled;
    spdlog::info("VendFastCommand: Empty mode {}", g_empty_mode_enabled ? "ENABLED" : "DISABLED");
}

void VendFastCommand::toggle_add_mode() {
    g_add_mode_enabled = !g_add_mode_enabled;
    spdlog::info("VendFastCommand: Add mode {}", g_add_mode_enabled ? "ENABLED" : "DISABLED");
}

void VendFastCommand::toggle_buy_mode() {
    g_buy_mode_enabled = !g_buy_mode_enabled;
    spdlog::info("VendFastCommand: Buy mode {}", g_buy_mode_enabled ? "ENABLED" : "DISABLED");
}

bool VendFastCommand::is_empty_mode_enabled() {
    return g_empty_mode_enabled;
}

bool VendFastCommand::is_add_mode_enabled() {
    return g_add_mode_enabled;
}

bool VendFastCommand::is_buy_mode_enabled() {
    return g_buy_mode_enabled;
}

void VendFastCommand::set_buy_count(int count) {
    g_buy_count = count;
    spdlog::info("VendFastCommand: Buy count set to {}", count);
}

int VendFastCommand::get_buy_count() {
    return g_buy_count;
}

void VendFastCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    if (!client || !client->get_player()) {
        spdlog::error("VendFastCommand: No client or player!");
        return;
    }

    if (!g_core) {
        spdlog::error("VendFastCommand: No core set!");
        return;
    }

    show_vendfast_gui(client);
}

void VendFastCommand::show_vendfast_gui(client::Client* client) {
    auto* server = g_core->get_server();
    if (!server || !server->get_player()) {
        spdlog::error("VendFastCommand: No server player!");
        return;
    }

    std::ostringstream dialog;
    dialog << "set_default_color|`o\n";
    dialog << "add_label_with_icon|big|`wVending Fast Actions``|left|2978|\n";
    dialog << "add_spacer|small|\n";
    
    dialog << "add_textbox|`5Select a fast vending action:``|left|\n";
    dialog << "add_spacer|small|\n";
    
    
    {
        std::string status = g_empty_mode_enabled ? "`2ENABLED``" : "`4DISABLED``";
        dialog << "add_label_with_icon|big|`wEmpty Vending``|left|2978|\n";
        dialog << "add_smalltext|`2Pull all stock from a machine when you open the update dialog``|\n";
        dialog << "add_smalltext|Status: " << status << "``|\n";
        dialog << "add_button|toggle_empty|`9Toggle Empty Mode``|\n";
        dialog << "add_spacer|small|\n";
    }
    
    
    {
        std::string status = g_add_mode_enabled ? "`2ENABLED``" : "`4DISABLED``";
        dialog << "add_label_with_icon|big|`wAdd Stock``|left|2978|\n";
        dialog << "add_smalltext|`2Automatically add the same item when update dialog opens``|\n";
        dialog << "add_smalltext|Status: " << status << "``|\n";
        dialog << "add_button|toggle_add|`9Toggle Add Mode``|\n";
        dialog << "add_spacer|small|\n";
    }
    
    
    std::string buy_status = g_buy_mode_enabled ? "`2ENABLED``" : "`4DISABLED``";
    dialog << "add_label_with_icon|big|`wFast Buy vb``|left|2978|\n";
    dialog << "add_smalltext|`2Buys maximum amount from vending (keep bgls in ur inven)``|\n";
    dialog << "add_smalltext|Status: " << buy_status << "``|\n";
    dialog << "add_button|toggle_buy|`9Toggle Buy Mode``|\n";
    dialog << "add_spacer|small|\n";
    
    dialog << "add_textbox|`4Note:Toggle the button to disable the feature``|left|\n";
    dialog << "add_spacer|small|\n";
    dialog << "add_quick_exit|\n";
    dialog << "end_dialog|vendfast|Close||\n";

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
    
    spdlog::info("VendFastCommand: Sent vend fast GUI");
}

} 
