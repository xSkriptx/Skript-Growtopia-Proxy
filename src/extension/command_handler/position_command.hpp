#pragma once
#include "command_base.hpp"
#include "../../core/core.hpp"
#include <memory>
#include <array>

namespace command {

struct Position {
    int x = -1;
    int y = -1;
    bool is_set() const { return x != -1 && y != -1; }
};

class PositionCommand : public CommandBase {
public:
    PositionCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    static void set_core(core::Core* core);
    static Position get_position(int index); 
    static void set_position(int index, int x, int y);
    
private:
    static core::Core* s_core;
    static std::array<Position, 5> s_positions; 
};

class TeleportPosCommand : public CommandBase {
public:
    TeleportPosCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    static void set_core(core::Core* core);
    
private:
    static core::Core* s_core;
};

class BackCommand : public CommandBase {
public:
    BackCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    static void set_core(core::Core* core);
    static void note_join_request_target(const std::string& raw_name);
    
private:
    static core::Core* s_core;
    static std::string s_current_world;
    static std::string s_previous_world;
};

} 
