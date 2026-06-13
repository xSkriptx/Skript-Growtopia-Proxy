#pragma once
#include "command_base.hpp"
#include "../../core/core.hpp"
#include <memory>
#include <atomic>

namespace command {

class DropAtCommand : public CommandBase {
public:
    DropAtCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    static void set_core(core::Core* core);
    
private:
    static core::Core* s_core;
    static std::atomic<bool> s_running;
    
    static void run_drop_at_position(client::Client* client_ptr, int pos_index, int amount);
};

} 
