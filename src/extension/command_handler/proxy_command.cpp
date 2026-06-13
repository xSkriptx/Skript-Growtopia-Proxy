#include "proxy_command.hpp"
#include "../../client/client.hpp"
#include "../../player/player.hpp"
#include "../../server/server.hpp"
#include "../../packet/packet_variant.hpp"
#include "../../packet/packet_types.hpp"
#include "../../utils/byte_stream.hpp"
#include "../../utils/packet_utils.hpp"
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <sstream>

namespace command {

static core::Core* g_core_proxy = nullptr;
static core::Core* g_core_info = nullptr;

void ProxyCommand::set_core(core::Core* core) {
    g_core_proxy = core;
}

void InfoCommand::set_core(core::Core* core) {
    g_core_info = core;
}

ProxyCommand::ProxyCommand() : CommandBase(
    {"proxy", "commands", "help"},
    {},
    "Open comprehensive commands GUI with all available commands",
    0
) {}

std::unique_ptr<CommandBase> ProxyCommand::clone() const {
    return std::make_unique<ProxyCommand>();
}

void ProxyCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    if (!client || !client->get_player()) {
        spdlog::error("ProxyCommand: client or player is null!");
        return;
    }

    if (!g_core_proxy) {
        spdlog::error("ProxyCommand: No core set!");
        utils::PacketUtils::send_chat_message(client->get_player(), "`4Error: Core not initialized");
        return;
    }

    ProxyCommand::show_commands_gui(client->get_player(), g_core_proxy);
}

void ProxyCommand::show_commands_gui(player::Player* player, core::Core* core) {
    if (!player) {
        spdlog::error("ProxyCommand: player is null!");
        return;
    }

    auto* server = core->get_server();
    if (!server || !server->get_player()) {
        spdlog::error("ProxyCommand: No server available!");
        return;
    }

    try {
        std::ostringstream dialog;
        
        
        dialog << "set_default_color|`o\n";
        dialog << "add_label_with_icon|big|`wGrowtopia Proxy Commands``|left|32|\n";
        dialog << "add_spacer|small|\n";
        dialog << "add_textbox|`9Version 2.43.0 - Complete Command List``|left|\n";
        dialog << "add_spacer|small|\n";
        
        
        
        
        dialog << "add_textbox|`#════════════════════════════════════════``|\n";
        dialog << "add_label_with_icon|medium|`2World & Navigation``|left|242|\n";
        dialog << "add_textbox|`#════════════════════════════════════════``|\n";
        dialog << "add_spacer|small|\n";

        dialog << "add_textbox|`2/warp`` `9[world]``|left|\n";
        dialog << "add_textbox|`2/back`` - `9Warp to previous world|left|\n";
        dialog << "add_textbox|`2/exit`` - `9Exit world|left|\n";
        dialog << "add_textbox|`2/join`` `9 - Open join mode GUI [autoban/pull/kick]|left|\n";
        dialog << "add_textbox|`2/growscan`` - Scan world for items|left|\n";
        dialog << "add_textbox|`2/door`` `9[door_id]`` - Enter specific door|left|\n";
        dialog << "add_textbox|`2/doorbf`` - Bruteforce door IDs|left|\n";
        dialog << "add_textbox|`2/eventmenu`` - Open clash event menu (even if not live)|left|\n";
        dialog << "add_textbox|`2/remo`` - `9Unaccess fast from world|left|\n";
        dialog << "add_textbox|`2/autocollect`` - Auto-collect items|left|\n";
        dialog << "add_textbox|`2/path`` `9[x] [y]`` - Find path and walk to coordinates|left|\n";
        dialog << "add_spacer|small|\n";
        
        
        
        
        dialog << "add_textbox|`#════════════════════════════════════════``|\n";
        dialog << "add_label_with_icon|medium|`eVending & Trading``|left|2978|\n";
        dialog << "add_textbox|`#════════════════════════════════════════``|\n";
        dialog << "add_spacer|small|\n";

        dialog << "add_textbox|`e/vendloc`` - Find vending machines|left|\n";
        dialog << "add_textbox|`e/vendfind`` `9[item]`` - Search local vendings|left|\n";
        dialog << "add_textbox|`e/vendfast`` - Quick setup|left|\n";
        dialog << "add_textbox|`e/vendsafe`` - Buy everything you can afford from vending machines|left|\n";
        dialog << "add_textbox|`e/vendlogs`` - View vending logs|left|\n";
        dialog << "add_textbox|`e/vendtp [item_name]`` - Highlight the item in the world|left|\n";
        dialog << "add_textbox|`e/buy`` - Open buy dialogs|left|\n";
        dialog << "add_textbox|`e/fillgbc`` - Auto-fill GBC into well|left|\n";
        dialog << "add_textbox|`e/mailclaim`` `9[message_id]`` - Claim mailbox message|left|\n";
        dialog << "add_textbox|`e/dbox`` - Enable fast donation|left|\n";
        dialog << "add_textbox|`e/setdb`` `9[amount]`` - Set fast donation amount|left|\n";
        dialog << "add_spacer|small|\n";
        
        
        
        
        dialog << "add_textbox|`#════════════════════════════════════════``|\n";
        dialog << "add_label_with_icon|medium|`6Inventory & Items``|left|6878|\n";
        dialog << "add_textbox|`#════════════════════════════════════════``|\n";
        dialog << "add_spacer|small|\n";

        dialog << "add_textbox|`6/inventory`` - View your inventory|left|\n";
        dialog << "add_textbox|`6/find`` `9[item]`` - Find items in world|left|\n";
        dialog << "add_textbox|`6/dropfast`` - Fast drop items|left|\n";
        dialog << "add_textbox|`6/dropall`` - Drop all items|left|\n";
        dialog << "add_textbox|`6/trashfast`` - Fast trash items|left|\n";
        dialog << "add_textbox|`6/bankadd`` `9[amount]`` - Deposit to world lock bank|left|\n";
        dialog << "add_textbox|`6/bankwith`` `9[amount]`` - Withdraw from world lock bank|left|\n";
        dialog << "add_spacer|small|\n";
        
        
        
        
        dialog << "add_textbox|`#════════════════════════════════════════``|\n";
        dialog << "add_label_with_icon|medium|`bPlayer Customization``|left|1784|\n";
        dialog << "add_textbox|`#════════════════════════════════════════``|\n";
        dialog << "add_spacer|small|\n";

        dialog << "add_textbox|`b/name`` `9[color] [name]`` - Change name|left|\n";
        dialog << "add_textbox|`b/title`` - Open title GUI|left|\n";
        dialog << "add_textbox|`b/cleartitle`` - Remove title|left|\n";
        dialog << "add_textbox|`b/clothes`` `9[items]`` - Change clothes|left|\n";
        dialog << "add_textbox|`b/clearclothes`` - Remove clothes|left|\n";
        dialog << "add_textbox|`b/flag`` `9[flag_id]`` - Change flag (us,tr,lb)|left|\n";
        dialog << "add_textbox|`b/invis`` - Toggle invisibility|left|\n";
        dialog << "add_textbox|`b/sm`` - Super Supporter + Rejoin World For Long Punch|left|\n";
        dialog << "add_textbox|`b/players`` - List all players you have met + netIDs|left|\n";
        dialog << "add_spacer|small|\n";
        
        
        
        
        dialog << "add_textbox|`#════════════════════════════════════════``|\n";
        dialog << "add_label_with_icon|medium|`4Admin & Moderation & Casinos``|left|758|\n";
        dialog << "add_textbox|`#════════════════════════════════════════``|\n";
        dialog << "add_spacer|small|\n";
        
        dialog << "add_textbox|`4/admin`` - Check admin uids + count|left|\n";
        dialog << "add_textbox|`4/warn`` - Warn player|left|\n";
        dialog << "add_textbox|`4/mentor`` - Make player mentor|left|\n";
        dialog << "add_textbox|`4/banall`` - Ban all in world|left|\n";
        dialog << "add_textbox|`4/pullall`` - Pull all players|left|\n";
        dialog << "add_textbox|`4/banfire`` - Auto-ban with fire effect|left|\n";
        dialog << "add_textbox|`4/fakemaint `4{reason}`` - Fake restart message|left|\n";
        dialog << "add_textbox|`4/antigravity`` - Enables antigravity effect|left|\n";
        dialog << "add_textbox|`4/antipunch`` - Enables antipunch like punch jammer|left|\n";
        dialog << "add_textbox|`4/vision`` - See in dark|left|\n";
        dialog << "add_textbox|`4/chest`` - See what's in chest|left|\n";
        dialog << "add_textbox|`4/host`` - Open hoster settings dialog|left|\n";
        dialog << "add_textbox|`4/dwl`` [amount] - Drop specific amount of wls|left|\n";
        dialog << "add_textbox|`4/ddl`` [amount] - Drop specific amount of dls|left|\n";
        dialog << "add_textbox|`4/dbgl`` [amount] - Drop specific amount of bgls|left|\n";
        dialog << "add_textbox|`4/moddetect`` - Enable/Disable mod spawn detection|left|\n";
        dialog << "add_textbox|`4/run`` - Warp multiple worlds to escape from mods|left|\n";
        dialog << "add_textbox|`4/ignorecsn`` - Will ignore every csn promoting msg|left|\n";
        dialog << "add_textbox|`4/ignorecsnchat`` - Will ignore every csn promoting chat|left|\n";
        dialog << "add_spacer|small|\n";
        
        
        
        
        dialog << "add_textbox|`#════════════════════════════════════════``|\n";
        dialog << "add_label_with_icon|medium|`1World Effects & Utilities``|left|780|\n";
        dialog << "add_textbox|`#════════════════════════════════════════``|\n";
        dialog << "add_spacer|small|\n";
        
        dialog << "add_textbox|`1/weather`` `9[value]`` - Change weather|left|\n";
        dialog << "add_textbox|`1/dat`` - Reveal ext tiles data|left|\n";
        dialog << "add_textbox|`1/freeze`` - Freeze client (fake DC)|left|\n";
        dialog << "add_textbox|`1/ping`` - Display GT server ping|left|\n";
        dialog << "add_textbox|`1/lockefind`` - Search for locke|left|\n";
        dialog << "add_spacer|small|\n";
        
        
        
        
        dialog << "add_textbox|`#════════════════════════════════════════``|\n";
        dialog << "add_label_with_icon|medium|`3Spam & Automation``|left|3410|\n";
        dialog << "add_textbox|`#════════════════════════════════════════``|\n";
        dialog << "add_spacer|small|\n";
        
        dialog << "add_textbox|`3/spamtext`` `9[text]`` - Set spam text|left|\n";
        dialog << "add_textbox|`3/spamdelay`` `9[seconds]`` - Set delay|left|\n";
        dialog << "add_textbox|`3/spam`` - Start/stop spam|left|\n";
        dialog << "add_textbox|`3/autosurg`` - Start/stop Auto-Surgery|left|\n";
        dialog << "add_textbox|`3/autocrime`` - Start/stop Auto-Crime|left|\n";
        dialog << "add_textbox|`3/wrench`` - Auto-wrench pull/kick/ban|left|\n";
        dialog << "add_spacer|small|\n";
        
        
        dialog << "add_textbox|`#════════════════════════════════════════``|\n";
        dialog << "add_spacer|small|\n";
        dialog << "add_textbox|`w💡 Tip: `oType commands with space if they don't work!``|left|\n";
        dialog << "add_textbox|`9Auto-save enabled for titles & features!``|left|\n";
        dialog << "add_spacer|small|\n";
        dialog << "add_quick_exit|\n";
        dialog << "end_dialog|proxy_commands_gui|Close||";
        
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
        
        spdlog::info("ProxyCommand: GUI sent successfully");
        
    } catch (const std::exception& e) {
        spdlog::error("ProxyCommand: Failed to send GUI: {}", e.what());
    }
}


InfoCommand::InfoCommand() : CommandBase(
    {"info", "information"},
    {},
    "Open comprehensive commands GUI with all available commands",
    0
) {}

std::unique_ptr<CommandBase> InfoCommand::clone() const {
    return std::make_unique<InfoCommand>();
}

void InfoCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    if (!client || !client->get_player()) {
        spdlog::error("InfoCommand: client or player is null!");
        return;
    }

    if (!g_core_info) {
        spdlog::error("InfoCommand: No core set!");
        utils::PacketUtils::send_chat_message(client->get_player(), "`4Error: Core not initialized");
        return;
    }

    
    ProxyCommand::show_commands_gui(client->get_player(), g_core_info);
}

} 
