#include "clearclothes_command.hpp"
#include "clothes_command.hpp"
#include "../../client/client.hpp"
#include "../../player/player.hpp"
#include "../../utils/packet_utils.hpp"
#include <spdlog/spdlog.h>

namespace command {


extern std::unordered_map<int, int> g_clothing_slots;

ClearClothesCommand::ClearClothesCommand() : CommandBase(
    {"clearclothes", "clearvisual", "resetclothes"},
    {},
    "Clear all saved visual clothing items",
    0
) {}

std::unique_ptr<CommandBase> ClearClothesCommand::clone() const {
    return std::make_unique<ClearClothesCommand>(*this);
}

void ClearClothesCommand::clear_all_items() {
    g_clothing_slots.clear();
    spdlog::info("ClearClothesCommand: Cleared all saved clothing items");
}

void ClearClothesCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    if (!client || !client->get_player()) {
        spdlog::error("ClearClothesCommand: No client or player!");
        return;
    }

    int items_cleared = g_clothing_slots.size();
    
    
    clear_all_items();
    
    
    utils::PacketUtils::send_chat_message(client->get_player(), 
        fmt::format("`2Cleared {} visual clothing items! Start fresh with /find.", items_cleared));
    
    spdlog::info("ClearClothesCommand: Cleared {} items for user", items_cleared);
}

} 
