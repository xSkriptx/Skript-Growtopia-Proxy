#pragma once
#include "command_base.hpp"
#include "../../core/core.hpp"
#include <atomic>
#include <cstdint>

namespace command {

class AutoCollectCommand : public CommandBase {
public:
    AutoCollectCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    static void set_core(core::Core* core);
    static void run_autocollect(std::uint64_t generation);
    static void send_collect_packet(player::Player* to_server, uint32_t uid, float x, float y);
    static void stop();
    static bool is_enabled() { return s_running.load(); }

    
    
    
    static void notify_item_drop(float x, float y);
    static core::Core* s_core;

private:
    static std::atomic<bool> s_running;
    static std::atomic<std::uint64_t> s_generation;
};

} 
