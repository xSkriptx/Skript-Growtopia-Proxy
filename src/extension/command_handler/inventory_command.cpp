#include "inventory_command.hpp"
#include "../../client/client.hpp"
#include "../../player/player.hpp"
#include "../../utils/packet_utils.hpp"
#include "../../utils/inventory_manager.hpp"
#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace command {

InventoryCommand::InventoryCommand() : CommandBase(
    {"inv", "inventory"},
    {"[item_id]"},
    "Show inventory info\nUsage: /inv [item_id]",
    0  
) {}

std::unique_ptr<CommandBase> InventoryCommand::clone() const {
    return std::make_unique<InventoryCommand>();
}

void InventoryCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    if (!client || !client->get_player()) {
        spdlog::error("InventoryCommand: No client or player!");
        return;
    }

    spdlog::info("=== INVENTORY COMMAND EXECUTED ===");
    auto& inv_mgr = utils::InventoryManager::get_instance();
    
    
    if (!args.empty()) {
        try {
            uint16_t item_id = static_cast<uint16_t>(std::stoul(args[0]));
            uint8_t count = inv_mgr.get_item_count(item_id);
            
            spdlog::info("Item {} count: {}", item_id, count);
            utils::PacketUtils::send_chat_message(client->get_player(),
                fmt::format("`2Item {}: `w{} `2in inventory", item_id, count));
        } catch (...) {
            utils::PacketUtils::send_chat_message(client->get_player(),
                "`4Invalid item ID");
        }
        return;
    }
    
    
    uint32_t inv_size = inv_mgr.get_inventory_size();
    
    
    size_t item_count = 0;
    std::string item_list = "`2Items: ";
    int display_count = 0;
    
    
    try {
        const auto& items = inv_mgr.get_items();
        item_count = items.size();
        
        for (const auto& [id, item] : items) {
            if (display_count >= 10) {
                item_list += "...";
                break;
            }
            item_list += fmt::format("`w{}x{} ", item.amount, id);
            display_count++;
        }
    } catch (const std::exception& e) {
        spdlog::error("Error accessing inventory: {}", e.what());
    }
    
    spdlog::info("Inventory summary: {} items, size {}", item_count, inv_size);
    
    utils::PacketUtils::send_chat_message(client->get_player(),
        fmt::format("`2Inventory: `w{} `2items, size: `w{}", item_count, inv_size));
    
    if (item_count > 0) {
        utils::PacketUtils::send_chat_message(client->get_player(), item_list);
    }
}

} 
