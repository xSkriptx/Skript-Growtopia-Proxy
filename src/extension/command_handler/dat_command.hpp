#pragma once
#include "command_base.hpp"
#include "../../utils/world_parser_v2.h"
#include "../../core/core.hpp"
#include <vector>
#include <string>

namespace command {

class DatCommand : public CommandBase {
public:
    DatCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    
    static void set_current_world(world_v2::World* world);
    static void set_core(core::Core* core);
    static void ingest_donation_dialog(const std::string& dialog_content);

private:
    static bool is_special_item(uint16_t item_id);
    static const char* get_item_type_name(uint16_t item_id, uint8_t extra_type);
    static void show_data_gui(player::Player* player, const std::vector<const world_v2::Tile*>& tiles);
};

} 
