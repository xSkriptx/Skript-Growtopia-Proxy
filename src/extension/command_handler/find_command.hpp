#pragma once
#include "command_base.hpp"
#include "vending_common.hpp"
#include "../../utils/world_parser_v2.h"
#include "../../core/core.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace command {

class FindCommand : public CommandBase {
public:
    FindCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;

    
    static void save_world_vendings(const world_v2::World& world);
    static void set_core(core::Core* core);
    
    
    static void handle_dialog_click(player::Player* player, const std::string& button);

private:
    void show_item_search_results(player::Player* player, const std::string& query, 
                                  const std::unordered_map<uint32_t, std::vector<VendingEntry>>& grouped);
    void show_vending_locations(player::Player* player, uint32_t item_id, 
                                const std::vector<VendingEntry>& entries);
    static std::unordered_map<uint32_t, std::vector<VendingEntry>> search_and_group_by_item(const std::string& query);
    static void append_to_file(const std::vector<VendingEntry>& entries);
};

} 
