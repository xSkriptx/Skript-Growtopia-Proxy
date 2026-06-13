#include "dropall_command.hpp"
#include "../../client/client.hpp"
#include "../../server/server.hpp"
#include "../../player/player.hpp"
#include "../../packet/packet_variant.hpp"
#include "../../packet/packet_types.hpp"
#include "../../utils/byte_stream.hpp"
#include "../../utils/inventory_manager.hpp"
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <thread>
#include <chrono>

namespace command {

core::Core* DropAllCommand::s_core = nullptr;
std::atomic<bool> DropAllCommand::s_running{false};
std::atomic<std::uint64_t> DropAllCommand::s_generation{0};

bool DropAllCommand::is_running() {
    return s_running.load();
}

DropAllCommand::DropAllCommand() : CommandBase(
    {"dropall"},
    {},
    "Drop all items from inventory",
    0
) {}

std::unique_ptr<CommandBase> DropAllCommand::clone() const {
    return std::make_unique<DropAllCommand>(*this);
}

void DropAllCommand::set_core(core::Core* core) {
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

static void send_generic_text(player::Player* to_server_player, const std::string& raw) {
    if (!to_server_player) return;
    ByteStream<std::uint16_t> bs{};
    
    bs.write(packet::NET_MESSAGE_GENERIC_TEXT);
    bs.write(raw, false);
    to_server_player->send_packet(bs.get_data(), 0);
}

void DropAllCommand::execute(client::Client* , const std::vector<std::string>& ) {
    spdlog::info("DropAllCommand::execute called; s_core={}, server={}", (void*)s_core, (void*)(s_core ? s_core->get_server() : nullptr));

    if (!s_core) {
        spdlog::error("DropAllCommand: No core set!");
        return;
    }

    auto* server = s_core->get_server();
    if (!server || !server->get_player()) {
        spdlog::error("DropAllCommand: No local (server) player!");
        return;
    }

    
    if (s_running.load()) {
        spdlog::info("DropAllCommand::execute - cancelling existing run");
        
        ++s_generation;
        send_console(server->get_player(), "`0[ `bSkriptProxy `0] `9dropall cancelled");
        s_running = false;
        return;
    }

    
    spdlog::info("DropAllCommand::execute - starting new run");
    s_running = true;
    const std::uint64_t gen = ++s_generation;
    send_console(server->get_player(), "`0[ `bSkriptProxy `0] `9dropping all items..");

    std::thread([gen]() {
        spdlog::info("DropAllCommand thread launched (gen={})", gen);
        run_dropall(gen);
    }).detach();
}

void DropAllCommand::run_dropall(std::uint64_t generation) {
    auto finish = []() {
        DropAllCommand::s_running = false;
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

    
    auto& inv_mgr = utils::InventoryManager::get_instance();
    const auto items = inv_mgr.get_items_snapshot();
    spdlog::info("DropAll: inventory snapshot contains {} items", items.size());

    
    
    

    std::size_t dropped_stacks = 0;
    if (items.empty()) {
        spdlog::warn("DropAll: no items to drop");
    }
    for (const auto& item : items) {
        if (!s_core || generation != s_generation.load()) {
            break;
        }

        player::Player* to_server_player = nullptr;
        if (s_core && s_core->get_client())
            to_server_player = s_core->get_client()->get_player();
        if (!to_server_player) {
            break;
        }

        if (item.id == 0 || item.amount == 0) {
            continue;
        }

        int remaining = static_cast<int>(item.amount);
        while (remaining > 0 && generation == s_generation.load()) {
            const int chunk = (remaining > 200) ? 200 : remaining;
            remaining -= chunk;

            
            send_generic_text(to_server_player, fmt::format("action|drop\n|itemID|{}|", item.id));
            std::this_thread::sleep_for(std::chrono::milliseconds(150));

            std::ostringstream confirm;
            confirm << "action|dialog_return\n"
                    << "dialog_name|drop_item\n"
                    << "itemID|" << item.id << "|\n"
                    << "count|" << chunk << "|\n"
                    << "buttonClicked|yes";
            send_generic_text(to_server_player, confirm.str());

            dropped_stacks++;
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
        }
    }

    if (s_core) {
        auto* local_player = s_core->get_server() ? s_core->get_server()->get_player() : nullptr;
        if (local_player && generation == s_generation.load()) {
            send_console(local_player,
                fmt::format("`0[ `bSkriptProxy `0] `9dropped all items (`w{}`9 stacks)", dropped_stacks));
        }
    }

    finish();
}

} 

