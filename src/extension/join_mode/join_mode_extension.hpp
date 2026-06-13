#pragma once
#include "../extension.hpp"
#include "../../core/core.hpp"
#include "../../utils/text_parse.hpp"
#include "../command_handler/join_command.hpp"
#include <spdlog/spdlog.h>

namespace extension::join_mode {

class JoinModeExtension final : public IExtension {
    core::Core* core_;

public:
    PROVIDE_EXT_UID(0x4A4F494E); 

    explicit JoinModeExtension(core::Core* core) : core_(core) {}
    ~JoinModeExtension() override = default;

    void init() override {
        spdlog::trace("JoinModeExtension initializing...");
        
        
        core_->get_event_dispatcher().appendListener(
            core::EventType::Message,
            [this](const core::EventMessage& evt) {
                if (evt.from != core::EventFrom::FromClient) {
                    return;
                }
                handle_client_message(evt);
            }
        );
        
        spdlog::trace("JoinModeExtension initialized");
    }

    void free() override {
        delete this;
    }

private:
    void handle_client_message(const core::EventMessage& evt) {
        const auto& text_parse = evt.get_message();
        std::string action = text_parse.get("action");
        
        if (action != "dialog_return") {
            return;
        }
        
        std::string dialog_name = text_parse.get("dialog_name");
        if (dialog_name != "join_commands") {
            return;
        }
        
        std::string button = text_parse.get("buttonClicked");
        
        spdlog::info("[JOIN-EXT] Dialog return: button='{}'", button);
        
        if (button == "toggle_pull") {
            spdlog::info("[JOIN-EXT] Toggle pull mode");
            command::JoinCommand::toggle_pull_mode();
            const_cast<core::EventMessage&>(evt).canceled = true;
            
            if (core_->get_client()) {
                command::JoinCommand cmd;
                cmd.execute(core_->get_client(), {});
            }
        }
        else if (button == "toggle_kick") {
            spdlog::info("[JOIN-EXT] Toggle kick mode");
            command::JoinCommand::toggle_kick_mode();
            const_cast<core::EventMessage&>(evt).canceled = true;
            
            if (core_->get_client()) {
                command::JoinCommand cmd;
                cmd.execute(core_->get_client(), {});
            }
        }
        else if (button == "toggle_ban") {
            spdlog::info("[JOIN-EXT] Toggle ban mode");
            command::JoinCommand::toggle_ban_mode();
            const_cast<core::EventMessage&>(evt).canceled = true;
            
            if (core_->get_client()) {
                command::JoinCommand cmd;
                cmd.execute(core_->get_client(), {});
            }
        }
    }
};

} 
