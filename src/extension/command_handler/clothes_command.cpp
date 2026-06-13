#include "clothes_command.hpp"
#include "../../client/client.hpp"
#include "../../player/player.hpp"
#include "../../server/server.hpp"
#include "../../utils/text_parse.hpp"
#include "../../utils/byte_stream.hpp"
#include "../../utils/player_tracker.hpp"
#include "../../packet/packet_variant.hpp"
#include "../../packet/packet_types.hpp"
#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>
#include <unordered_map>

namespace command {


std::unordered_map<int, int> g_clothing_slots; 
static core::Core* g_core = nullptr;

ClothesCommand::ClothesCommand() : CommandBase(
    {"clothes", "wear", "visual"},
    {},
    "Apply the selected visual clothing items",
    0
) {}

std::unique_ptr<CommandBase> ClothesCommand::clone() const {
    return std::make_unique<ClothesCommand>(*this);
}

void ClothesCommand::set_core(core::Core* core) {
    g_core = core;
}

void ClothesCommand::set_pending_item(int item_id, int clothing_type) {
    
    if (g_clothing_slots.find(clothing_type) != g_clothing_slots.end()) {
        int old_item = g_clothing_slots[clothing_type];
        g_clothing_slots[clothing_type] = item_id;
        spdlog::info("ClothesCommand: Replaced item {} with {} in slot {}", old_item, item_id, clothing_type);
    } else {
        g_clothing_slots[clothing_type] = item_id;
        spdlog::info("ClothesCommand: Added item {} to slot {}", item_id, clothing_type);
    }
    
    
    spdlog::info("ClothesCommand: Current outfit has {} items", g_clothing_slots.size());
}

void ClothesCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    if (!client || !client->get_player()) {
        spdlog::error("ClothesCommand: No client or player!");
        return;
    }

    if (!g_core) {
        spdlog::error("ClothesCommand: No core set!");
        return;
    }

    if (g_clothing_slots.empty()) {
        spdlog::warn("ClothesCommand: No items selected! Use /find to select items first.");
        return;
    }

    spdlog::info("ClothesCommand: Applying visual clothing for {} items", g_clothing_slots.size());
    send_clothing_change(client);
}

void ClothesCommand::send_clothing_change(client::Client* client) {
    
    auto& player_tracker = utils::PlayerTracker::get_instance();
    auto player_info = player_tracker.get_local_player();
    
    if (player_info.netID == 0) {
        spdlog::warn("ClothesCommand: player netID not found. Try after spawning.");
        return;
    }
    
    
    TextParse skin_parse{};
    skin_parse.add("action", {"setSkin"});
    skin_parse.add("color", {"3033464831"});
    
    ByteStream<std::uint16_t> skin_stream{};
    skin_stream.write(packet::NET_MESSAGE_GENERIC_TEXT);
    skin_stream.write(skin_parse.get_raw(), false);
    
    if (g_core->get_client() && g_core->get_client()->get_player()) {
        g_core->get_client()->get_player()->send_packet(skin_stream.get_data(), 0);
        spdlog::info("ClothesCommand: Sent setSkin packet");
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    
    
    int hat = 0, shirt = 0, pants = 0, shoes = 0, face = 0, hand = 0, back = 0, hair = 0, neck = 0, ances = 0;
    
    
    for (const auto& [slot_type, item_id] : g_clothing_slots) {
        switch (slot_type) {
            case 0: hat = item_id; break;
            case 1: shirt = item_id; break;
            case 2: pants = item_id; break;
            case 3: shoes = item_id; break;
            case 4: face = item_id; break;
            case 5: hand = item_id; break;
            case 6: back = item_id; break;
            case 7: hair = item_id; break;
            case 8: neck = item_id; break;
            case 9: ances = item_id; break;
            default:
                spdlog::warn("ClothesCommand: Unknown clothing_type: {}", slot_type);
                break;
        }
    }
    
    spdlog::info("ClothesCommand: Outfit - Hat:{} Shirt:{} Pants:{} Shoes:{} Face:{} Hand:{} Back:{} Hair:{} Neck:{} Ances:{}", 
                 hat, shirt, pants, shoes, face, hand, back, hair, neck, ances);
    
    packet::Variant clothing_variant{};
    clothing_variant.add("OnSetClothing");
    
    
    clothing_variant.add(glm::vec3((float)hat, (float)shirt, (float)pants));
    
    
    clothing_variant.add(glm::vec3((float)shoes, (float)face, (float)hand));
    
    
    clothing_variant.add(glm::vec3((float)back, (float)hair, (float)neck));
    
    
    clothing_variant.add((uint32_t)3033464831);
    
    
    clothing_variant.add(glm::vec3((float)ances, 0.0f, 0.0f));
    
    std::vector<std::byte> clothing_data = clothing_variant.serialize();
    
    packet::GameUpdatePacket clothing_packet{};
    clothing_packet.type = packet::PACKET_CALL_FUNCTION;
    clothing_packet.net_id = player_info.netID;
    clothing_packet.flags.extended = 1;
    clothing_packet.data_size = static_cast<uint32_t>(clothing_data.size());
    
    ByteStream<std::uint16_t> clothing_stream{};
    clothing_stream.write(packet::NET_MESSAGE_GAME_PACKET);
    clothing_stream.write(clothing_packet);
    clothing_stream.write_data(clothing_data.data(), clothing_data.size());
    
    if (g_core->get_server() && g_core->get_server()->get_player()) {
        g_core->get_server()->get_player()->send_packet(clothing_stream.get_data(), 0);
        spdlog::info("ClothesCommand: Sent OnSetClothing with netid: {}", player_info.netID);
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    
    
    packet::Variant particle_variant{};
    particle_variant.add("OnParticleEffect");
    particle_variant.add((uint32_t)73); 
    particle_variant.add(glm::vec2(0.0f, 0.0f)); 
    particle_variant.add((uint32_t)0);
    particle_variant.add((uint32_t)0);
    
    std::vector<std::byte> particle_data = particle_variant.serialize();
    
    packet::GameUpdatePacket particle_packet{};
    particle_packet.type = packet::PACKET_CALL_FUNCTION;
    particle_packet.net_id = -1; 
    particle_packet.flags.extended = 1;
    particle_packet.data_size = static_cast<uint32_t>(particle_data.size());
    
    ByteStream<std::uint16_t> particle_stream{};
    particle_stream.write(packet::NET_MESSAGE_GAME_PACKET);
    particle_stream.write(particle_packet);
    particle_stream.write_data(particle_data.data(), particle_data.size());
    
    if (g_core->get_server() && g_core->get_server()->get_player()) {
        g_core->get_server()->get_player()->send_packet(particle_stream.get_data(), 0);
        spdlog::info("ClothesCommand: Sent particle effect");
    }
    
    spdlog::info("ClothesCommand: Clothing change completed for {} items", g_clothing_slots.size());
}

} 
