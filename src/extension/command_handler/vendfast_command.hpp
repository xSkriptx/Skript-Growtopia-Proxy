#pragma once
#include "command_base.hpp"
#include "../../core/core.hpp"
#include <string>
#include <vector>

namespace command {

class VendFastCommand : public CommandBase {
public:
    VendFastCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;

    static void set_core(core::Core* core);
    
    
    static void toggle_empty_mode();
    static void toggle_add_mode();
    static void toggle_buy_mode();
    
    
    static bool is_empty_mode_enabled();
    static bool is_add_mode_enabled();
    static bool is_buy_mode_enabled();
    
    
    static void set_buy_count(int count);
    static int get_buy_count();

private:
    static void show_vendfast_gui(client::Client* client);
};

} 
