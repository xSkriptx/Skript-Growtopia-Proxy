#include "dbox_command.hpp"
#include "../../client/client.hpp"
#include "../../server/server.hpp"
#include "../../player/player.hpp"
#include "../../utils/text_parse.hpp"
#include "../../utils/byte_stream.hpp"
#include "../../packet/packet_types.hpp"
#include "../../utils/packet_utils.hpp"
#include <spdlog/spdlog.h>
#include <fmt/format.h>

namespace command {

bool DboxCommand::active_ = false;
int DboxCommand::custom_amount_ = -1;
static core::Core* g_core = nullptr;

DboxCommand::DboxCommand() : CommandBase(
    {"dbox", "setdb"},
    {},
    "Auto-donate items to Donation Box with max inventory count",
    0
) {}

std::unique_ptr<CommandBase> DboxCommand::clone() const {
    return std::make_unique<DboxCommand>(*this);
}

void DboxCommand::set_core(core::Core* core) {
    g_core = core;
}

bool DboxCommand::is_active() {
    return active_;
}

void DboxCommand::set_active(bool active) {
    active_ = active;
}

void DboxCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    if (!client || !client->get_player()) {
        spdlog::error("DboxCommand: No client or player available");
        return;
    }

    
    if (!args.empty() && args[0] == "setdb") {
        if (args.size() >= 2) {
            try {
                int amount = std::stoi(args[1]);
                if (amount > 0) {
                    custom_amount_ = amount;
                    player::Player* target_player = nullptr;
                    if (g_core) {
                        if (auto* server = g_core->get_server(); server && server->get_player()) {
                            target_player = server->get_player();
                        } else {
                            target_player = client->get_player();
                        }
                    } else {
                        target_player = client->get_player();
                    }
                    if (target_player) {
                        utils::PacketUtils::send_chat_message(target_player,
                            fmt::format("`2Dbox set to donate {} items next time.", amount));
                    }
                    spdlog::info("DboxCommand: Custom donation amount set to {}", amount);
                } else {
                    spdlog::warn("DboxCommand: Invalid donation amount (must be > 0)");
                }
            } catch (...) {
                spdlog::warn("DboxCommand: Failed to parse donation amount");
            }
        } else {
            spdlog::warn("DboxCommand: Usage: /setdb <amount>");
        }
        return;
    }

    
    active_ = !active_;
    player::Player* target_player = nullptr;
    if (g_core) {
        if (auto* server = g_core->get_server(); server && server->get_player()) {
            target_player = server->get_player();
        } else {
            target_player = client->get_player();
        }
    } else {
        target_player = client->get_player();
    }
    if (target_player) {
        if (active_) {
            utils::PacketUtils::send_chat_message(target_player,
                "`2Dbox enabled:`o will auto-donate max inventory count to Donation Boxes if error generated please use /setdb [amount] first!");
        } else {
            utils::PacketUtils::send_chat_message(target_player,
                "`4Dbox disabled.`o Normal donation box behavior restored");
        }
    }
    spdlog::info("DboxCommand: Mode {}", active_ ? "enabled" : "disabled");




} 

void DboxCommand::set_custom_amount(int amount) {
    custom_amount_ = amount;
}

int DboxCommand::get_custom_amount() {
    return custom_amount_;
}
}
