#pragma once
#include "command_base.hpp"
#include "../../core/core.hpp"
#include <atomic>

namespace command {



struct DropCurrencyState {
    static std::atomic<bool> s_dropping;
};

class DropWLCommand : public CommandBase {
public:
    DropWLCommand();
    std::unique_ptr<CommandBase> clone() const override;
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    static void set_core(core::Core* core);
    static bool is_dropping() { return DropCurrencyState::s_dropping.load(); }
private:
    static core::Core* s_core;
};

class DropDLCommand : public CommandBase {
public:
    DropDLCommand();
    std::unique_ptr<CommandBase> clone() const override;
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    static void set_core(core::Core* core);
private:
    static core::Core* s_core;
};

class DropBGLCommand : public CommandBase {
public:
    DropBGLCommand();
    std::unique_ptr<CommandBase> clone() const override;
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    static void set_core(core::Core* core);
private:
    static core::Core* s_core;
};



class VisualDropCommand : public CommandBase {
public:
    VisualDropCommand();
    std::unique_ptr<CommandBase> clone() const override;
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    static void set_core(core::Core* core);
private:
    static core::Core* s_core;
};

} 
