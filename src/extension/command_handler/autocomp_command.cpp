#include "autocomp_command.hpp"
#include "../../client/client.hpp"
#include "../../server/server.hpp"
#include "../../player/player.hpp"
#include "../../packet/packet_variant.hpp"
#include "../../packet/packet_types.hpp"
#include "../../utils/byte_stream.hpp"
#include "../../utils/inventory_manager.hpp"
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>

namespace command {

core::Core* AutoCompCommand::s_core = nullptr;
std::atomic<bool> AutoCompCommand::s_running{false};
std::atomic<std::uint64_t> AutoCompCommand::s_generation{0};
std::chrono::steady_clock::time_point AutoCompCommand::s_last_use = std::chrono::steady_clock::time_point{};

AutoCompCommand::AutoCompCommand() : CommandBase(
    {"autocomp", "ac"},
    {},
    "Compress 100 WLs to 1 DL (run multiple times to compress more)",
    0
) {}

std::unique_ptr<CommandBase> AutoCompCommand::clone() const {
    return std::make_unique<AutoCompCommand>(*this);
}

void AutoCompCommand::set_core(core::Core* core) {
    s_core = core;
}

static void send_console(player::Player* local_player, const std::string& msg) {
    if (!local_player) return;
    packet::Variant var{};
    var.add("OnConsoleMessage");
    var.add(msg);

    std::vector<std::byte> ext_data = var.serialize();
    packet::GameUpdatePacket pkt{};
    pkt.type = packet::PACKET_CALL_FUNCTION;
    pkt.net_id = -1;
    pkt.flags.extended = 1;
    pkt.data_size = static_cast<uint32_t>(ext_data.size());

    ByteStream<std::uint16_t> bs{};
    bs.write(packet::NET_MESSAGE_GAME_PACKET);
    bs.write(pkt);
    bs.write_data(ext_data.data(), ext_data.size());

    local_player->send_packet(bs.get_data(), 0);
}

static void send_compress_packet(player::Player* to_server_player, uint16_t item_id) {
    if (!to_server_player) return;
    
    
    packet::GameUpdatePacket pkt{};
    pkt.type = packet::PACKET_ITEM_ACTIVATE_REQUEST;  
    pkt.net_id = -1;
    pkt.flags.value = packet::PACKET_FLAG_NONE;
    pkt.data_size = 0;
    pkt.decompressed_data_size = 0;
    
    
    ByteStream<std::uint16_t> bs{};
    bs.write(packet::NET_MESSAGE_GAME_PACKET);
    bs.write(pkt);
    bs.write(static_cast<int32_t>(item_id));  
    
    to_server_player->send_packet(bs.get_data(), 0);
    spdlog::info("Sent PACKET_ITEM_ACTIVATE_REQUEST for item ID: {}", item_id);
}

void AutoCompCommand::execute(client::Client* , const std::vector<std::string>& ) {
    if (!s_core) {
        spdlog::error("AutoCompCommand: No core set!");
        return;
    }

    auto* server = s_core->get_server();
    if (!server || !server->get_player()) {
        spdlog::error("AutoCompCommand: No local (server) player!");
        return;
    }

    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - s_last_use).count();
    
    if (elapsed < 5) {
        send_console(server->get_player(), 
            fmt::format("`0[ `bSkriptProxy `0] `4Cooldown! Wait {} more seconds", 5 - elapsed));
        return;
    }

    if (s_running.exchange(true)) {
        send_console(server->get_player(), "`0[ `bSkriptProxy `0] `4AutoComp already running!");
        return;
    }

    
    s_last_use = now;
    
    const std::uint64_t gen = ++s_generation;
    send_console(server->get_player(), "`0[ `bSkriptProxy `0] `9Starting auto-compress...");

    std::thread([gen]() {
        run_autocomp(gen);
    }).detach();
}

void AutoCompCommand::run_autocomp(std::uint64_t generation) {
    auto finish = []() {
        AutoCompCommand::s_running = false;
    };

    if (!s_core) {
        finish();
        return;
    }

    auto* server = s_core->get_server();
    auto* client = s_core->get_client();
    if (!server || !server->get_player() || !client || !client->get_player()) {
        finish();
        return;
    }

    const uint16_t WL_ID = 242;    
    const int WL_PER_DL = 100;

    auto& inv_mgr = utils::InventoryManager::get_instance();

    
    uint8_t wl_count_raw = inv_mgr.get_item_count(WL_ID);
    int wl_count = static_cast<int>(wl_count_raw);
    
    spdlog::info("Current WL count: {}", wl_count);
    
    
    auto items_snapshot = inv_mgr.get_items_snapshot();
    spdlog::info("Total items in inventory: {}", items_snapshot.size());
    for (const auto& item : items_snapshot) {
        if (item.id == WL_ID) {
            spdlog::info("  Found WL! id={}, amount={}, flags={}", item.id, item.amount, item.flags);
        }
    }
    
    if (wl_count < WL_PER_DL) {
        
        send_console(server->get_player(), 
            fmt::format("`0[ `bSkriptProxy `0] `4Not enough WLs! (have: {}, need: {})", 
            wl_count, WL_PER_DL));
        finish();
        return;
    }

    auto* to_server_player = s_core->get_client() ? s_core->get_client()->get_player() : nullptr;
    if (!to_server_player) {
        finish();
        return;
    }

    
    send_console(server->get_player(), 
        fmt::format("`9Compressing 1 DL... (WLs: {})", wl_count));
    
    send_compress_packet(to_server_player, WL_ID);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    
    
    int new_wl_count = static_cast<int>(inv_mgr.get_item_count(WL_ID));
    spdlog::info("After compression - WL count: {} -> {}", wl_count, new_wl_count);
    
    
    if (new_wl_count == wl_count) {
        send_console(server->get_player(), 
            "`0[ `bSkriptProxy `0] `4Item locked by server! Wait 10-30 seconds before trying again.");
        
        s_last_use = std::chrono::steady_clock::time_point{};
        finish();
        return;
    }
    
    
    int compressed = (wl_count - new_wl_count) / WL_PER_DL;
    send_console(server->get_player(),
        fmt::format("`0[ `bSkriptProxy `0] `2Compressed {} DL(s) successfully!", compressed));

    finish();
}

} 
