#include "bank_withdraw_command.hpp"
#include "../../server/server.hpp"
#include "../../utils/packet_utils.hpp"
#include "../../packet/packet_helper.hpp"
#include "../../packet/packet_variant.hpp"
#include <thread>

namespace command {
core::Core* BankWithdrawCommand::s_core = nullptr;
BankWithdrawCommand::BankWithdrawCommand() : CommandBase({"bankwith"}, {}, "Withdraw all World Locks from storage.") {}

void BankWithdrawCommand::set_core(core::Core* core) {
    s_core = core;
}

void BankWithdrawCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    if (!client || !client->get_player()) return;
    auto* player = client->get_player();
    
    if (args.size() <= 1) {
        utils::PacketUtils::send_chat_message(player, "`4Usage: /bankwith {amount}", false);
        return;
    }
    int withdraw_amount = -1;
    try {
        withdraw_amount = std::stoi(args[1]);
    } catch (...) {
        utils::PacketUtils::send_chat_message(player, "`4Invalid amount! Please enter a number.", false);
        return;
    }
    if (withdraw_amount <= 0) {
        utils::PacketUtils::send_chat_message(player, "`4Amount must be greater than 0!", false);
        return;
    }
    
    std::string open_storage = "action|dialog_return\ndialog_name|popup\nnetID|" + std::to_string(player->get_peer()->connectID) + "\nbuttonClicked|open_worldlock_storage";
    {
        ByteStream<std::uint16_t> bs;
        bs.write(packet::NET_MESSAGE_GENERIC_TEXT);
        bs.write(open_storage, false);
        player->send_packet(bs.get_data(), 0);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    
    
    std::string withdraw_page = "action|worldlock_storage_getpage\npageIndex|-1";
    {
        ByteStream<std::uint16_t> bs;
        bs.write(packet::NET_MESSAGE_GENERIC_TEXT);
        bs.write(withdraw_page, false);
        player->send_packet(bs.get_data(), 0);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    
    std::string withdraw = "action|worldlock_storage_modify_amount\namount|" + std::to_string(withdraw_amount) + "\ntype|-11";
    {
        ByteStream<std::uint16_t> bs;
        bs.write(packet::NET_MESSAGE_GENERIC_TEXT);
        bs.write(withdraw, false);
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
            packet::Variant var{};
            var.add("OnTextOverlay");
            std::string overlay = "Withdrew " + std::to_string(withdraw_amount) + " ā (WL) from Bank";
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
    utils::PacketUtils::send_chat_message(player, "`2Withdrew currency from bank!", false);
}

std::unique_ptr<CommandBase> BankWithdrawCommand::clone() const {
    return std::make_unique<BankWithdrawCommand>(*this);
}
} 
