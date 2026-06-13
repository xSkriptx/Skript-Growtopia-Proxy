#pragma once
#include "command_base.hpp"
#include "../../core/core.hpp"
#include <string>
#include <vector>

namespace command {

class DboxCommand : public CommandBase {
public:
    DboxCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;

    static void set_core(core::Core* core);
    static bool is_active();
    static void set_active(bool active);

private:
    static bool active_;
    
    static int custom_amount_;
public:
    static void set_custom_amount(int amount);
    static int get_custom_amount();
};

} 
