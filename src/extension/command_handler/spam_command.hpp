#pragma once
#include "command_base.hpp"
#include "../../core/core.hpp"
#include <string>
#include <thread>
#include <atomic>

namespace command {

class SpamCommand : public CommandBase {
public:
    SpamCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    
    static void set_core(core::Core* core);
    static void toggle_spam();
    static bool is_spamming();
    static void stop_spam();

    
    static std::string s_spam_text;
    static int s_spam_delay;  
    static std::atomic<bool> s_spamming;
    static core::Core* s_core;

private:
    static std::thread s_spam_thread;

    
    static std::atomic<std::uint64_t> s_spam_generation;

    static void spam_loop(std::uint64_t generation);
};

class SpamTextCommand : public CommandBase {
public:
    SpamTextCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
};

class SpamDelayCommand : public CommandBase {
public:
    SpamDelayCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
};

} 
