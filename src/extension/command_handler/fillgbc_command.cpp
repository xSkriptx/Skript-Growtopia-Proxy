#include "fillgbc_command.hpp"
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

static core::Core* g_core = nullptr;
bool FillGBCCommand::active_ = false;

FillGBCCommand::FillGBCCommand() : CommandBase(
    {"fillgbc"},
    {},
    "Auto-fill Well of Love with Golden Booty Chests",
    0
) {}

std::unique_ptr<CommandBase> FillGBCCommand::clone() const {
    return std::make_unique<FillGBCCommand>(*this);
}

void FillGBCCommand::set_core(core::Core* core) {
    g_core = core;
}

bool FillGBCCommand::is_active() {
    return active_;
}

void FillGBCCommand::set_active(bool active) {
    active_ = active;
}

void FillGBCCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    if (!client || !client->get_player()) {
        spdlog::error("FillGBCCommand: No client or player available");
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
                "`2FillGBC enabled:`o will auto-fill Well of Love with Golden Booty Chests");
        } else {
            utils::PacketUtils::send_chat_message(target_player,
                "`4FillGBC disabled.`o Normal well behavior restored");
        }
    }
    
    spdlog::info("FillGBCCommand: Mode {}", active_ ? "enabled" : "disabled");
}

bool FillGBCCommand::handle_dialog(const std::string& dialog_content, uint32_t tile_x, uint32_t tile_y) {
    if (!active_ || !g_core) {
        return false;
    }

    
    if (dialog_content.find("Well of Love") == std::string::npos || 
        dialog_content.find("wishing_well") == std::string::npos) {
        return false;
    }

    spdlog::info("FillGBCCommand: Detected Well of Love dialog at tile ({}, {})", tile_x, tile_y);

    auto* client = g_core->get_client();
    if (!client || !client->get_player()) {
        spdlog::error("FillGBCCommand: No client player");
        return false;
    }

    
    int count = 5; 
    size_t count_pos = dialog_content.find("embed_data|count|");
    if (count_pos != std::string::npos) {
        size_t start = count_pos + 17; 
        size_t end = dialog_content.find_first_of("\n|", start);
        if (end != std::string::npos) {
            try {
                std::string count_str = dialog_content.substr(start, end - start);
                count = std::stoi(count_str);
            } catch (...) {
                count = 5;
            }
        }
    }

    spdlog::info("FillGBCCommand: Auto-responding with count={}, tileX={}, tileY={}", count, tile_x, tile_y);

    
    
    std::string dialog_return = fmt::format(
        "action|dialog_return\n|dialog_name|wishing_well\n"
        "count|{}\ntileX|{}\nitemID|3402\ntileY|{}\nbuttonClicked|wishing_well",
        count, tile_x, tile_y
    );

    ByteStream<std::uint16_t> bs{};
    bs.write(packet::NET_MESSAGE_GENERIC_TEXT);
    bs.write(dialog_return, false);
    
    client->get_player()->send_packet(bs.get_data(), 0);
    
    spdlog::info("FillGBCCommand: Sent dialog_return for Well of Love");
    
    return true; 
}

} 
