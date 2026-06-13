#pragma once
#include "command_base.hpp"
#include "../../utils/world_parser_v2.h"
#include "../../core/core.hpp"
#include <vector>

namespace command {

class DoordatCommand : public CommandBase {
public:
    DoordatCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    
    static void set_current_world(world_v2::World* world);
    static void set_core(core::Core* core);

private:
    static bool is_door_item(uint16_t item_id);
    static const char* get_door_name(uint16_t item_id);
    static void show_door_gui(player::Player* player, const std::vector<const world_v2::Tile*>& doors);
};

} 
