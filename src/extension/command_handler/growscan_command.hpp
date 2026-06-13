#pragma once
#include "command_base.hpp"
#include <string>
#include <unordered_map>


namespace core {
    class Core;
}

namespace player {
    class Player;
}

namespace world {
    struct WorldInfo;
}

namespace command {

class GrowScanCommand : public CommandBase {
public:
    GrowScanCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    void execute_with_core(client::Client* client, const std::vector<std::string>& args, core::Core* core);
    std::unique_ptr<CommandBase> clone() const override;
    
    
    void handle_button_click(player::Player* player, const std::string& button, core::Core* core);
    
private:
    void show_growscan_menu(player::Player* player);
    void show_scan_results(player::Player* player, const std::string& item_data, const std::string& scan_type);
    
    
    std::string scan_world_tiles(const world::WorldInfo* world_info);
    std::string scan_floating_objects(const world::WorldInfo* world_info);
    
    
    std::string scan_world_tiles_from_manager();
    std::string scan_floating_objects_from_manager();
    int count_items_in_data(const std::string& data);
};

}
