#pragma once
#include "command_base.hpp"
#include "../../utils/text_parse.hpp"
#include "../../packet/message/chat.hpp"


namespace core {
    class Core;
}

namespace command {

class TitleCommand : public CommandBase {
public:
    TitleCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    void execute_with_core(client::Client* client, const std::vector<std::string>& args, core::Core* core);
    std::unique_ptr<CommandBase> clone() const override;

private:
    static void send_title_gui(client::Client* client, core::Core* core);
};

class G4GCommand : public CommandBase {
public:
    G4GCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    void execute_with_core(client::Client* client, const std::vector<std::string>& args, core::Core* core);
    std::unique_ptr<CommandBase> clone() const override;
};

class MaxLevelCommand : public CommandBase {
public:
    MaxLevelCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    void execute_with_core(client::Client* client, const std::vector<std::string>& args, core::Core* core);
    std::unique_ptr<CommandBase> clone() const override;
};

class DrCommand : public CommandBase {
public:
    DrCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    void execute_with_core(client::Client* client, const std::vector<std::string>& args, core::Core* core);
    std::unique_ptr<CommandBase> clone() const override;
};

} 
