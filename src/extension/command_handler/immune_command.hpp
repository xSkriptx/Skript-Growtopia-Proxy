#pragma once
#include "command_base.hpp"
#include "../../core/core.hpp"
#include "../../client/client.hpp"
#include <spdlog/spdlog.h>
#include <memory>

namespace command {

class ImmuneCommand : public CommandBase {
    inline static core::Core* core_ = nullptr;
    
public:
    ImmuneCommand() : CommandBase(
        {"immune"},
        {},
        "Toggle immunity to fire and acid damage",
        0
    ) {}

    static void set_core(core::Core* core) {
        core_ = core;
    }

    void execute(client::Client* client, const std::vector<std::string>& args) override {
        if (!core_) {
            spdlog::error("[ImmuneCommand] Core not initialized");
            return;
        }

        bool current = core_->get_config().get<bool>("features.immune_damage");
        core_->get_config().set("features.immune_damage", !current);
        
        if (!current) {
            spdlog::info("`2[Immune]`` Immunity to fire and acid damage `2ENABLED");
        } else {
            spdlog::info("`4[Immune]`` Immunity to fire and acid damage `4DISABLED");
        }
    }
    
    std::unique_ptr<CommandBase> clone() const override {
        return std::make_unique<ImmuneCommand>(*this);
    }
};

} 
