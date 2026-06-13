#pragma once
#include "command_base.hpp"
#include "../../core/core.hpp"
#include <string>
#include <vector>

namespace command {

class FillGBCCommand : public CommandBase {
public:
    FillGBCCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;

    static void set_core(core::Core* core);
    static bool is_active();
    static void set_active(bool active);
    
    
    static bool handle_dialog(const std::string& dialog_content, uint32_t tile_x, uint32_t tile_y);

private:
    static bool active_;
};

} 
