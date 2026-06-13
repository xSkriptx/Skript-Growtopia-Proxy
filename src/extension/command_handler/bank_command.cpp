#include "bank_command.hpp"
#include "../../server/server.hpp"
#include "../../utils/inventory_manager.hpp"
#include "../../utils/packet_utils.hpp"
#include "../../packet/packet_helper.hpp"

#include "../../packet/packet_variant.hpp"
#include <thread>

namespace command {
core::Core* BankCommand::s_core = nullptr;
BankCommand::BankCommand() : CommandBase({"bankadd"}, {}, "Deposit all World Locks to storage.") {}

void BankCommand::set_core(core::Core* core) {
    s_core = core;
}

void BankCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    if (!client || !client->get_player()) return;
    auto* player = client->get_player();
    auto& inv_mgr = utils::InventoryManager::get_instance();
    int deposit_amount = -1;
    if (args.size() > 1) {
        try {
            deposit_amount = std::stoi(args[1]);
        } catch (...) {
            utils::PacketUtils::send_chat_message(player, "`4Invalid amount! Please enter a number.", false);
            return;
        }
        if (deposit_amount <= 0) {
            utils::PacketUtils::send_chat_message(player, "`4Amount must be greater than 0!", false);
            return;
        }
    } else {
        int wl = inv_mgr.get_item_count(242);
        int dl = inv_mgr.get_item_count(1796);
        int bgl = inv_mgr.get_item_count(7188);
        deposit_amount = wl + dl * 100 + bgl * 10000;
    }
    if (deposit_amount <= 0) {
        utils::PacketUtils::send_chat_message(player, "`4No World Locks, Diamond Locks, or BGLs to deposit!", false);
        return;
    }
    
    std::string open_storage = "action|dialog_return\ndialog_name|popup\nnetID|" + std::to_string(player->get_peer()->connectID) + "\nbuttonClicked|open_worldlock_storage";
    {
        ByteStream<std::uint16_t> bs;
        bs.write(packet::NET_MESSAGE_GENERIC_TEXT);
        bs.write(open_storage, false);
        player->send_packet(bs.get_data(), 0);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    std::string deposit = "action|worldlock_storage_modify_amount\namount|" + std::to_string(deposit_amount) + "\ntype|1";
    {
        ByteStream<std::uint16_t> bs;
        bs.write(packet::NET_MESSAGE_GENERIC_TEXT);
        bs.write(deposit, false);
        player->send_packet(bs.get_data(), 0);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    std::string refresh = "action|worldlock_storage_getpage\npageIndex|0";
    {
        ByteStream<std::uint16_t> bs;
        bs.write(packet::NET_MESSAGE_GENERIC_TEXT);
        bs.write(refresh, false);
        player->send_packet(bs.get_data(), 0);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    
    if (s_core) {
        auto* server = s_core->get_server();
        if (server && server->get_player()) {
            std::string overlay = fmt::format("Adding {} ā (WL) to Bank", deposit_amount);
            packet::Variant var{};
            var.add("OnTextOverlay");
            var.add(overlay);
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
        }
    }
    utils::PacketUtils::send_chat_message(player, "`2Deposited currency to bank!", false);
}

std::unique_ptr<CommandBase> BankCommand::clone() const {
    return std::make_unique<BankCommand>(*this);
}
} 
