#include "drop_currency_command.hpp"
#include "../../client/client.hpp"
#include "../../server/server.hpp"
#include "../../player/player.hpp"
#include "../../utils/packet_utils.hpp"
#include "../../packet/packet_variant.hpp"
#include "../../packet/packet_types.hpp"
#include "../../packet/tank_packet.hpp"
#include "../../utils/byte_stream.hpp"
#include "../../utils/inventory_manager.hpp"
#include "../../utils/player_tracker.hpp"
#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <thread>
#include <chrono>

namespace command {

std::atomic<bool> DropCurrencyState::s_dropping{false};

core::Core* DropWLCommand::s_core  = nullptr;
core::Core* DropDLCommand::s_core  = nullptr;
core::Core* DropBGLCommand::s_core = nullptr;


core::Core* VisualDropCommand::s_core = nullptr;

static void send_generic(player::Player* p, const std::string& raw) {
    if (!p) return;
    ByteStream<std::uint16_t> bs{};
    bs.write(packet::NET_MESSAGE_GENERIC_TEXT);
    bs.write(raw, false);
    p->send_packet(bs.get_data(), 0);
}

static void send_drop_overlay(core::Core* core, const std::string& text) {
    if (!core || !core->get_server() || !core->get_server()->get_player()) return;
    packet::Variant var{};
    var.add("OnTextOverlay");
    var.add(text);
    std::vector<std::byte> ext = var.serialize();
    packet::GameUpdatePacket pkt{};
    pkt.type = packet::PACKET_CALL_FUNCTION;
    pkt.net_id = -1;
    pkt.flags.extended = 1;
    pkt.data_size = static_cast<uint32_t>(ext.size());
    ByteStream<std::uint16_t> bs{};
    bs.write(packet::NET_MESSAGE_GAME_PACKET);
    bs.write(pkt);
    bs.write_data(ext.data(), ext.size());
    core->get_server()->get_player()->send_packet(bs.get_data(), 0);
}

static void do_drop(core::Core* core, player::Player* console_player,
                    int item_id, int amount, const std::string& display,
                    int wl_value, const std::string& glyph)
{
    if (!core || !core->get_client() || !core->get_client()->get_player()) {
        if (console_player)
            utils::PacketUtils::send_chat_message(console_player,
                fmt::format("`4Drop{}: not connected!", display));
        return;
    }

    auto& inv = utils::InventoryManager::get_instance();
    int have = inv.get_item_count(item_id);
    if (have <= 0) {
        if (console_player)
            utils::PacketUtils::send_chat_message(console_player,
                fmt::format("`4Drop{}: you have 0 {}!", display, display));
        return;
    }
    if (amount > have) amount = have;

    player::Player* client_player = core->get_client()->get_player();

    
    DropCurrencyState::s_dropping = true;

    
    send_generic(client_player, fmt::format("action|drop\nitemID|{}|", item_id));
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    
    send_generic(client_player,
        fmt::format("action|dialog_return\ndialog_name|drop_item\nitemID|{}|\ncount|{}|\nbuttonClicked|yes",
            item_id, amount));

    
    std::thread([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
        DropCurrencyState::s_dropping = false;
    }).detach();

    
    int total_wl = amount * wl_value;
    std::string overlay = fmt::format("Dropped {} {} ({} WL equivalent)", glyph, amount, total_wl);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    send_drop_overlay(core, overlay);

    spdlog::info("Drop{}: dropped {} x itemID={} ({} WL)", display, amount, item_id, total_wl);
}




DropWLCommand::DropWLCommand() : CommandBase(
    {"dwl"}, {"<amount>"}, "Drop World Locks. /dwl <amount>", 1) {}

std::unique_ptr<CommandBase> DropWLCommand::clone() const {
    return std::make_unique<DropWLCommand>(*this);
}

void DropWLCommand::set_core(core::Core* core) { s_core = core; }

void DropWLCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    if (!client || !client->get_player()) return;
    auto* player = client->get_player();
    if (args.size() < 2) {
        utils::PacketUtils::send_chat_message(player, "`4Usage: /dwl <amount>");
        return;
    }
    int amount = 0;
    try { amount = std::stoi(args[1]); } catch (...) {}
    if (amount <= 0) {
        utils::PacketUtils::send_chat_message(player, "`4Invalid amount!");
        return;
    }
    do_drop(s_core, player, 242, amount, "WL", 1, "ā");
}



DropDLCommand::DropDLCommand() : CommandBase(
    {"ddl"}, {"<amount>"}, "Drop Diamond Locks. /ddl <amount>", 1) {}

std::unique_ptr<CommandBase> DropDLCommand::clone() const {
    return std::make_unique<DropDLCommand>(*this);
}

void DropDLCommand::set_core(core::Core* core) { s_core = core; }

void DropDLCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    if (!client || !client->get_player()) return;
    auto* player = client->get_player();
    if (args.size() < 2) {
        utils::PacketUtils::send_chat_message(player, "`4Usage: /ddl <amount>");
        return;
    }
    int amount = 0;
    try { amount = std::stoi(args[1]); } catch (...) {}
    if (amount <= 0) {
        utils::PacketUtils::send_chat_message(player, "`4Invalid amount!");
        return;
    }
    do_drop(s_core, player, 1796, amount, "DL", 100, "ā");
}



DropBGLCommand::DropBGLCommand() : CommandBase(
    {"dbgl"}, {"<amount>"}, "Drop Blue Gem Locks. /dbgl <amount>", 1) {}

std::unique_ptr<CommandBase> DropBGLCommand::clone() const {
    return std::make_unique<DropBGLCommand>(*this);
}

void DropBGLCommand::set_core(core::Core* core) { s_core = core; }

void DropBGLCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    if (!client || !client->get_player()) return;
    auto* player = client->get_player();
    if (args.size() < 2) {
        utils::PacketUtils::send_chat_message(player, "`4Usage: /dbgl <amount>");
        return;
    }
    int amount = 0;
    try { amount = std::stoi(args[1]); } catch (...) {}
    if (amount <= 0) {
        utils::PacketUtils::send_chat_message(player, "`4Invalid amount!");
        return;
    }
    do_drop(s_core, player, 7188, amount, "BGL", 10000, "ā");
}



VisualDropCommand::VisualDropCommand() : CommandBase(
    {"dbglvis"}, {"<amount>"}, "Visual drop Blue Gem Locks (no inventory loss). /dbglvis <amount>", 1) {}

std::unique_ptr<CommandBase> VisualDropCommand::clone() const {
    return std::make_unique<VisualDropCommand>(*this);
}

void VisualDropCommand::set_core(core::Core* core) { s_core = core; }

void VisualDropCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    if (!client || !client->get_player()) return;
    auto *player = client->get_player();
    if (args.size() < 2) {
        utils::PacketUtils::send_chat_message(player, "`4Usage: /dbglvis <amount>");
        return;
    }
    int amount = 0;
    try { amount = std::stoi(args[1]); } catch (...) {}
    if (amount <= 0) {
        utils::PacketUtils::send_chat_message(player, "`4Invalid amount!");
        return;
    }

    if (!s_core || !s_core->get_client() || !s_core->get_client()->get_player()) {
        utils::PacketUtils::send_chat_message(player, "`4VisualDrop: not connected!");
        return;
    }
    player::Player* client_player = s_core->get_client()->get_player();

    
    uint32_t netid = 0;
    {
        auto& tracker = utils::PlayerTracker::get_instance();
        auto local = tracker.get_local_player();
        if (local.netID > 0) {
            netid = local.netID;
        } else if (player->get_peer()) {
            
            netid = player->get_peer()->connectID;
        }
    }

    
    DropCurrencyState::s_dropping = true;
    std::thread([client_player, amount]() {
        for (int i = 0; i < amount; ++i) {
            send_generic(client_player,
                fmt::format("action|drop\n|itemID|{}|", 7188));
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        DropCurrencyState::s_dropping = false;
    }).detach();
    spdlog::info("VisualDrop: sent {} visual drops of itemID=7188", amount);
}

} 
