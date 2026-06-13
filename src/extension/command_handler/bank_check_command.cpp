#include "bank_check_command.hpp"
#include "../../server/server.hpp"
#include "../../utils/packet_utils.hpp"
#include "../../packet/packet_helper.hpp"
#include <thread>
#include <memory>
#include "../../packet/packet_variant.hpp"
#include <fmt/format.h>

#include "../parser/parser_impl.hpp" 
#include <spdlog/spdlog.h>



namespace command {

BankCheckCommand::BankCheckCommand() : CommandBase({"bankcheck"}, {}, "Check World Lock bank balance.") {}

std::unique_ptr<CommandBase> BankCheckCommand::clone() const {
    return std::make_unique<BankCheckCommand>(*this);
}


void BankCheckCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    if (!client || !client->get_player()) return;
    auto* player = client->get_player();

    
    static int last_bank_balance = -1;

    
    auto parse_bank_balance = [](const std::string& message) -> int {
        size_t pos = message.find("Bank Balance: ");
        if (pos != std::string::npos) {
            size_t start = pos + std::string("Bank Balance: ").size();
            size_t end = message.find(" ā", start);
            if (end != std::string::npos) {
                std::string balance_str = message.substr(start, end - start);
                try {
                    return std::stoi(balance_str);
                } catch (...) {}
            }
        }
        return -1;
    };

    
    
    for (int poll = 0; poll < 10; ++poll) {
        
        
        
        
        
        
        
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
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
    
    std::string fake_withdraw = "action|worldlock_storage_modify_amount\namount|-\ntype|-11";
    {
        ByteStream<std::uint16_t> bs;
        bs.write(packet::NET_MESSAGE_GENERIC_TEXT);
        bs.write(fake_withdraw, false);
        player->send_packet(bs.get_data(), 0);
    }
    utils::PacketUtils::send_chat_message(player, "`2Checking World Lock bank balance...", false);

    
    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    
    if (last_bank_balance >= 0) {
        std::string msg = fmt::format("Bank Balance: {} ā (WL)", last_bank_balance);
        packet::Variant var;
        var.add("OnConsoleMessage");
        var.add(msg);
        std::vector<std::byte> ext_data = var.serialize();
        packet::GameUpdatePacket pkt;
        pkt.type = packet::PACKET_CALL_FUNCTION;
        pkt.net_id = -1;
        pkt.flags.extended = 1;
        pkt.data_size = static_cast<uint32_t>(ext_data.size());
        ByteStream<std::uint16_t> bs;
        bs.write(packet::NET_MESSAGE_GAME_PACKET);
        bs.write(pkt);
        bs.write_data(ext_data.data(), ext_data.size());
        player->send_packet(bs.get_data(), 0);
    } else {
        utils::PacketUtils::send_chat_message(player, "`4Bank balance not received from server.", false);
    }
}

} 


