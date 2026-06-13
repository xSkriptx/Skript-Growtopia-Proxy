#pragma once
#include "command_base.hpp"
#include "../../core/core.hpp"
#include <atomic>
#include <cstdint>
#include <chrono>

namespace command {

class AutoCompCommand : public CommandBase {
public:
    AutoCompCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    static void set_core(core::Core* core);
    static void run_autocomp(std::uint64_t generation);

private:
    static core::Core* s_core;
    static std::atomic<bool> s_running;
    static std::atomic<std::uint64_t> s_generation;
    static std::chrono::steady_clock::time_point s_last_use;
};

} 
