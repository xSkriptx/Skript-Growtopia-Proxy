#pragma once
#include "command_base.hpp"
#include <string>

namespace core {
class Core;
}

namespace player {
class Player;
}

namespace command {

class GsBetaCommand : public CommandBase {
public:
    GsBetaCommand();

    void execute(client::Client* client, const std::vector<std::string>& args) override;
    void execute_with_core(client::Client* client, const std::vector<std::string>& args, core::Core* core);
    std::unique_ptr<CommandBase> clone() const override;

    void handle_button_click(player::Player* player, const std::string& button);

private:
    void show_menu(player::Player* player);
    void show_results(player::Player* player, const std::string& item_data, const std::string& scan_type);

    static std::string scan_tiles_from_send_map_data();
    static std::string scan_floating_from_send_map_data();
    static int count_items_in_data(const std::string& data);
};

} 

