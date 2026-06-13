#pragma once
#include "command_base.hpp"
#include "../../utils/world_parser_v2.h"
#include "../../core/core.hpp"
#include <string>
#include <vector>

namespace command {

struct VendingEntry {
    std::string world_name;
    std::string timestamp;
    uint32_t x, y;
    uint16_t tile_id;  
    uint32_t item_id;
    int32_t price;
    bool is_wl_price;
    
    std::string get_type() const {
        return tile_id == 9268 ? "Digivend" : "Vending";
    }
};

class VendFindCommand : public CommandBase {
public:
    VendFindCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;

    
    static void save_world_vendings(const world_v2::World& world);
    static void set_core(core::Core* core);

private:
    void show_search_results(player::Player* player, const std::string& query, const std::vector<VendingEntry>& results);
    static std::vector<VendingEntry> search_vendings(const std::string& query);
    static void append_to_file(const std::vector<VendingEntry>& entries);
    static std::string get_item_name(uint32_t item_id);
};

} 
