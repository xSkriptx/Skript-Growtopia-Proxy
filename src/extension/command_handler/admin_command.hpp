#pragma once
#include "command_base.hpp"
#include "../../utils/world_parser_v2.h"
#include "../../core/core.hpp"
#include <string>
#include <vector>

namespace command {

class AdminCommand : public CommandBase {
public:
    AdminCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;

    
    static void set_current_world(world_v2::World* world);
    static void set_core(core::Core* core);

private:
    void show_admin_gui(player::Player* player, const std::vector<const world_v2::Tile*>& locks);
    bool is_lock_item(uint16_t item_id);
    const char* get_lock_name(uint16_t item_id);
};

} 
