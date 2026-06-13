#pragma once
#include "command_base.hpp"
#include "../../utils/inventory_manager.hpp"
#include "../../utils/packet_utils.hpp"
#include <thread>

namespace command {
class BankAddCommand : public CommandBase {
public:
    BankAddCommand() : CommandBase({"bankadd"}, {}, "Deposit all World Locks to storage.") {}

    void execute(client::Client* client, const std::vector<std::string>& args) override {
        if (!client || !client->get_player()) return;
        auto* player = client->get_player();
        auto& inv_mgr = utils::InventoryManager::get_instance();
        int wl = inv_mgr.get_item_count(242);
        if (wl <= 0) {
            utils::PacketUtils::send_chat_message(player, "`4No World Locks to deposit!", false);
            return;
        }
        
        std::string open_storage = "action|dialog_return\ndialog_name|popup\nnetID|" + std::to_string(player->get_peer()->connectID) + "\nbuttonClicked|open_worldlock_storage";
        packet::PacketHelper::send_raw(open_storage, *player);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        std::string deposit = "action|worldlock_storage_modify_amount\namount|" + std::to_string(wl) + "\ntype|1";
        packet::PacketHelper::send_raw(deposit, *player);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        std::string refresh = "action|worldlock_storage_getpage\npageIndex|0";
        packet::PacketHelper::send_raw(refresh, *player);
        utils::PacketUtils::send_chat_message(player, "`2Deposited all World Locks to bank!", false);
    }

    std::unique_ptr<CommandBase> clone() const override {
        return std::make_unique<BankAddCommand>(*this);
    }
};
}
