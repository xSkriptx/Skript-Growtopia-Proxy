#pragma once
#include "command_base.hpp"
#include "vending_common.hpp"
#include "../../utils/world_parser_v2.h"
#include "../../core/core.hpp"
#include <string>
#include <vector>


namespace extension::item_finder {
    class ItemDatabase;
}

namespace command {

class VendLocCommand : public CommandBase {
public:
    VendLocCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;

    
    static void save_world_vendings(const world_v2::World& world);
    static void set_core(core::Core* core);
    static void set_item_database(extension::item_finder::ItemDatabase* database);
    
    
    static void handle_dialog_click(player::Player* player, const std::string& button);
    static void handle_dialog_return(player::Player* player, const std::string& query);

private:
    void show_search_gui(player::Player* player, const std::string& query);
    void show_vending_locations(player::Player* player, uint32_t item_id, const std::string& item_name,
                                const std::vector<VendingEntry>& entries);
    
    static std::vector<VendingEntry> get_vendings_for_item(uint32_t item_id);
    static void append_to_file(const std::vector<VendingEntry>& entries);
    static void upload_to_api(const std::vector<VendingEntry>& entries);
};

class VendTPCommand : public CommandBase {
public:
    VendTPCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;

    static void set_core(core::Core* core);
    static void set_item_database(extension::item_finder::ItemDatabase* database);
};

} 
