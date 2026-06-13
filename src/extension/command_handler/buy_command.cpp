#include "buy_command.hpp"
#include "../../client/client.hpp"
#include "../../server/server.hpp"
#include "../../player/player.hpp"
#include "../../utils/text_parse.hpp"
#include "../../utils/byte_stream.hpp"
#include "../../packet/packet_types.hpp"
#include "../../packet/packet_variant.hpp"
#include "../../core/core.hpp"
#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <thread>
#include <chrono>

namespace command {

static core::Core* g_core = nullptr;


static const std::unordered_map<std::string, std::string> STORE_ID_MAP = {
    
    {"vcs", "voucher_pack_card_battle"},
    {"vouchercardbattle", "voucher_pack_card_battle"},
    {"cardvoucher", "voucher_pack_card_battle"},
    
    
    
};

BuyCommand::BuyCommand() : CommandBase(
    {"buy"},
    {},
    "Purchase an item from the store (opens GUI to enter store_id)",
    0
) {}

std::unique_ptr<CommandBase> BuyCommand::clone() const {
    return std::make_unique<BuyCommand>(*this);
}

void BuyCommand::set_core(core::Core* core) {
    g_core = core;
}

void BuyCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    if (!client->get_player()) {
        spdlog::error("BuyCommand: No player available");
        return;
    }

    if (!g_core) {
        spdlog::error("BuyCommand: Core not set!");
        return;
    }

    auto* server = g_core->get_server();
    if (!server || !server->get_player()) {
        spdlog::error("BuyCommand: No server player!");
        return;
    }

    
    std::string dialog = 
        "set_default_color|`o\n"
        "add_label_with_icon|big|`wPurchase from Store``|left|1796|\n"
        "add_spacer|small|\n"
        "add_textbox|Enter the Store ID of the item you want to purchase:|\n"
        "add_text_input|store_id|||30|\n"
        "add_spacer|small|\n"
        "add_textbox|Enter the amount to buy (1-200):|\n"
        "add_text_input|count||1|3|\n"
        "add_spacer|small|\n"
        "add_smalltext|`4Note: Enter the exact store_id (e.g., voucher_pack_card_battle)``|\n"
        "add_spacer|small|\n"
        "end_dialog|buy_dialog|Cancel|Purchase|";

    packet::Variant var{};
    var.add("OnDialogRequest");
    var.add(dialog);

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

    server->get_player()->send_packet(bs.get_data(), 0);
    
    spdlog::info("BuyCommand: Displayed purchase dialog to server player");
}

void BuyCommand::send_buy_packet(client::Client* client, const std::string& store_id, int count) {
    if (!client->get_player()) {
        spdlog::error("BuyCommand: No player available");
        return;
    }
    
    
    if (count < 1) count = 1;
    if (count > 200) count = 200;
    
    spdlog::info("BuyCommand: Purchasing '{}' x{} times", store_id, count);
    
    
    for (int i = 0; i < count; i++) {
        
        std::string buy_packet = fmt::format("action|buy\n|item|{} ", store_id);
        
        
        if (i == 0) {
            spdlog::info("BuyCommand: Sending buy packet: '{}'", buy_packet);
        }

        
        ByteStream<std::uint16_t> byte_stream{};
        byte_stream.write(packet::NET_MESSAGE_GENERIC_TEXT); 
        byte_stream.write(buy_packet, false);

        
        client->get_player()->send_packet(byte_stream.get_data(), 0);
        
        
        if (i % 50 == 0 || i == count - 1) {
            spdlog::info("BuyCommand: Sent buy packet {}/{} for store_id '{}'", i + 1, count, store_id);
        }
        
        
        if (i < count - 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    spdlog::info("BuyCommand: Completed sending {} buy packets for '{}'", count, store_id);
}

std::string BuyCommand::get_store_id(const std::string& item_name) {
    
    std::string lower_name = item_name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
    
    
    auto it = STORE_ID_MAP.find(lower_name);
    if (it != STORE_ID_MAP.end()) {
        return it->second;
    }
    
    
    return item_name;
}

} 
