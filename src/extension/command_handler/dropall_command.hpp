#pragma once
#include "command_base.hpp"
#include "../../core/core.hpp"
#include <atomic>
#include <cstdint>

namespace command {

class DropAllCommand : public CommandBase {
public:
    DropAllCommand();
    std::unique_ptr<CommandBase> clone() const override;
    void execute(client::Client* client, const std::vector<std::string>& args) override;

    static void set_core(core::Core* core);
    
    static bool is_running();

private:
    static core::Core* s_core;
    static std::atomic<bool> s_running;
    static std::atomic<std::uint64_t> s_generation;

    static void run_dropall(std::uint64_t generation);
};

} 

