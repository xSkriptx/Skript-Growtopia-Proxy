#include "gsbeta_command.hpp"
#include "../../client/client.hpp"
#include "../../core/core.hpp"
#include "../../packet/packet_types.hpp"
#include "../../packet/packet_variant.hpp"
#include "../../server/server.hpp"
#include "../../utils/packet_utils.hpp"
#include "../../utils/world_manager.hpp"
#include <spdlog/spdlog.h>
#include <sstream>
#include <unordered_map>

namespace command {

GsBetaCommand::GsBetaCommand()
    : CommandBase({ "gsbeta" }, {}, "GrowScan beta (uses SEND_MAP_DATA world)", 0) {
}

std::unique_ptr<CommandBase> GsBetaCommand::clone() const {
    return std::make_unique<GsBetaCommand>();
}

void GsBetaCommand::execute(client::Client* client, const std::vector<std::string>& ) {
    if (client && client->get_player()) {
        utils::PacketUtils::send_chat_message(client->get_player(), "`4Error: Command requires core access.");
    }
}

void GsBetaCommand::execute_with_core(client::Client* client, const std::vector<std::string>& , core::Core* core) {
    if (!client || !client->get_player()) {
        return;
    }

    auto* server = core ? core->get_server() : nullptr;
    if (!server || !server->get_player()) {
        utils::PacketUtils::send_chat_message(client->get_player(), "`4Error: Server not available.");
        return;
    }

    show_menu(server->get_player());
}

void GsBetaCommand::handle_button_click(player::Player* player, const std::string& button) {
    if (!player) return;

    if (button == "beta_scan_blocks") {
        if (!utils::WorldManager::get_instance().has_world()) {
            utils::PacketUtils::send_chat_message(player, "`4No world data yet. Re-enter world and try again.");
            return;
        }

        std::string data = scan_tiles_from_send_map_data();
        if (data.empty()) {
            utils::PacketUtils::send_chat_message(player, "`4No blocks found in world.");
            return;
        }
        show_results(player, data, "blocks");
    }
    else if (button == "beta_scan_items") {
        if (!utils::WorldManager::get_instance().has_world()) {
            utils::PacketUtils::send_chat_message(player, "`4No world data yet. Re-enter world and try again.");
            return;
        }

        std::string data = scan_floating_from_send_map_data();
        if (data.empty()) {
            utils::PacketUtils::send_chat_message(player, "`4No floating items found.");
            return;
        }
        show_results(player, data, "items");
    }
    else if (button == "beta_back_to_menu") {
        show_menu(player);
    }
}

void GsBetaCommand::show_menu(player::Player* player) {
    if (!player) return;

    std::string dialog =
        "set_default_color|`o\n"
        "add_label_with_icon|big|`wGrowScan Beta``|left|6016|\n"
        "add_spacer|small|\n"
        "add_textbox|`wUses SEND_MAP_DATA`` (world_v2) for scanning.|left|\n"
        "add_spacer|small|\n"
        "add_button|beta_scan_blocks|`2World Blocks``|noflags|0|0|\n"
        "add_button|beta_scan_items|`6Floating Items``|noflags|0|0|\n"
        "add_spacer|small|\n"
        "end_dialog|gsbeta_menu|Cancel||";

    packet::Variant variant{};
    variant.add("OnDialogRequest");
    variant.add(dialog);

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
    player->send_packet(byte_stream.get_data(), 0);
}

void GsBetaCommand::show_results(player::Player* player, const std::string& item_data, const std::string& scan_type) {
    if (!player) return;

    const std::string title = (scan_type == "blocks") ? "World Blocks" : "Floating Items";
    const int unique = count_items_in_data(item_data);

    std::string dialog =
        "set_default_color|`o\n"
        "add_label_with_icon|big|`wGrowScan Beta``|left|6016|\n"
        "add_spacer|small|\n"
        "add_textbox|`w" + title + "``  (`2" + std::to_string(unique) + "`` types)|left|\n"
        "add_spacer|small|\n"
        "add_label_with_icon_button_list|small|`w%s : %s``|left|findTile_|itemIDseed2tree_itemAmount|" + item_data + "\n"
        "add_spacer|small|\n"
        "add_button|beta_back_to_menu|`5Back``|noflags|0|0|\n"
        "embed_data|DialogDwi|0\n"
        "end_dialog|gsbeta_results|Cancel||";

    packet::Variant variant{};
    variant.add("OnDialogRequest");
    variant.add(dialog);

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
    player->send_packet(byte_stream.get_data(), 0);
}

std::string GsBetaCommand::scan_tiles_from_send_map_data() {
    auto& wm = utils::WorldManager::get_instance();
    if (!wm.has_world() || wm.get_tiles().empty()) {
        return "";
    }

    std::unordered_map<uint16_t, uint64_t> counts;
    for (const auto& tile : wm.get_tiles()) {
        if (tile.Fg != 0 && tile.Fg != 256) counts[tile.Fg]++;
        if (tile.Bg != 0 && tile.Bg != 256) counts[tile.Bg]++;
    }

    std::ostringstream out;
    for (const auto& [id, c] : counts) {
        out << id << "," << c << ",";
    }
    return out.str();
}

std::string GsBetaCommand::scan_floating_from_send_map_data() {
    auto& wm = utils::WorldManager::get_instance();
    if (!wm.has_world()) {
        return "";
    }

    
    std::unordered_map<uint16_t, uint64_t> counts;
    for (const auto& d : wm.get_items()) {
        counts[d.ItemId] += d.Amount;
    }
    for (const auto& d : wm.get_live_objects()) {
        counts[d.ItemId] += d.Amount;
    }

    std::ostringstream out;
    for (const auto& [id, c] : counts) {
        out << id << "," << c << ",";
    }
    return out.str();
}

int GsBetaCommand::count_items_in_data(const std::string& data) {
    if (data.empty()) return 0;
    int commas = 0;
    for (char c : data) if (c == ',') ++commas;
    return commas / 2;
}

} 
