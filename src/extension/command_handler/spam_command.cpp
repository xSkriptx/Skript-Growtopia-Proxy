#include "spam_command.hpp"
#include "../../client/client.hpp"
#include "../../server/server.hpp"
#include "../../player/player.hpp"
#include "../../packet/packet_variant.hpp"
#include "../../packet/packet_types.hpp"
#include "../../utils/byte_stream.hpp"
#include "../../utils/text_parse.hpp"
#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <chrono>

namespace command {


std::string SpamCommand::s_spam_text = "Set your spam text and delay!";
int SpamCommand::s_spam_delay = 5;
std::atomic<bool> SpamCommand::s_spamming{false};
core::Core* SpamCommand::s_core = nullptr;
std::thread SpamCommand::s_spam_thread;
std::atomic<std::uint64_t> SpamCommand::s_spam_generation{0};

SpamCommand::SpamCommand() : CommandBase(
    
    
    {"spam", "/"},
    {},
    "Toggle spam on/off",
    0
) {}

std::unique_ptr<CommandBase> SpamCommand::clone() const {
    return std::make_unique<SpamCommand>();
}

void SpamCommand::set_core(core::Core* core) {
    s_core = core;
}

void SpamCommand::toggle_spam() {
    if (s_spamming) {
        stop_spam();
    } else {
        
        s_spamming = true;
        const std::uint64_t gen = ++s_spam_generation;

        s_spam_thread = std::thread([](std::uint64_t g) { spam_loop(g); }, gen);
        s_spam_thread.detach();

        spdlog::info("Spam started");
    }
}

bool SpamCommand::is_spamming() {
    return s_spamming;
}

void SpamCommand::stop_spam() {
    s_spamming = false;
    spdlog::info("Spam stopped");
}

void SpamCommand::spam_loop(std::uint64_t generation) {
    if (!s_core) {
        spdlog::error("SpamCommand: No core set!");
        s_spamming = false;
        return;
    }

    
    auto* server = s_core->get_server();
    if (!server || !server->get_player()) {
        spdlog::error("SpamCommand: No server player!");
        s_spamming = false;
        return;
    }

    
    auto* client = s_core->get_client();
    if (!client || !client->get_player()) {
        spdlog::error("SpamCommand: No client player (not connected to real server)!");
        s_spamming = false;
        return;
    }

    
    if (s_spam_delay < 2) {
        
        packet::Variant var{};
        var.add("OnConsoleMessage");
        var.add("`0[`SkriptProxy`0] `9Delay can't be less than 2!");
        
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
        
        server->get_player()->send_packet(bs.get_data(), 0);
        
        s_spamming = false;
        return;
    }

    while (s_spamming && generation == s_spam_generation.load()) {
        if (!s_core) {
            s_spamming = false;
            break;
        }

        
        client = s_core->get_client();
        if (!client || !client->get_player()) {
            spdlog::warn("SpamCommand: Lost client player - stopping spam");
            s_spamming = false;
            break;
        }

        
        
        std::string spam_packet = "action|input\ntext|" + s_spam_text;

        ByteStream<std::uint16_t> byte_stream{};
        byte_stream.write(packet::NET_MESSAGE_GENERIC_TEXT);
        byte_stream.write(spam_packet, false);

        client->get_player()->send_packet(byte_stream.get_data(), 0);

        spdlog::debug("Spam sent: {}", s_spam_text);

        
        std::this_thread::sleep_for(std::chrono::seconds(s_spam_delay));
    }
    
    spdlog::info("Spam loop ended");
}

void SpamCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    if (!client || !client->get_player()) {
        return;
    }

    if (!s_core) {
        spdlog::error("SpamCommand: No core set!");
        return;
    }

    auto* server = s_core->get_server();
    if (!server || !server->get_player()) {
        spdlog::error("SpamCommand: No server player!");
        return;
    }

    
    toggle_spam();
    
    
    packet::Variant var{};
    var.add("OnConsoleMessage");
    
    if (s_spamming) {
        var.add("`0[`bSkriptProxy`0] `9Spam is `bON");
    } else {
        var.add("`0[`bSkriptProxy`0] `9Spam is `bOFF");
    }
    
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
    
    server->get_player()->send_packet(bs.get_data(), 0);
}


SpamTextCommand::SpamTextCommand() : CommandBase(
    {"spamtext"},
    {},
    "Set spam text (usage: /spamtext <text>)",
    1
) {}

std::unique_ptr<CommandBase> SpamTextCommand::clone() const {
    return std::make_unique<SpamTextCommand>();
}

void SpamTextCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    if (!client || !client->get_player()) {
        return;
    }

    if (!SpamCommand::s_core) {
        return;
    }

    auto* server = SpamCommand::s_core->get_server();
    if (!server || !server->get_player()) {
        return;
    }

    
    std::string text;
    for (size_t i = 1; i < args.size(); i++) {
        if (i > 1) text += " ";
        text += args[i];
    }

    SpamCommand::s_spam_text = text;
    
    
    packet::Variant var{};
    var.add("OnConsoleMessage");
    var.add(fmt::format("`0[`bSkriptProxy`0] `9Spam text set to: `b{}", text));
    
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
    
    server->get_player()->send_packet(bs.get_data(), 0);
    
    spdlog::info("Spam text set to: {}", text);
}


SpamDelayCommand::SpamDelayCommand() : CommandBase(
    {"spamdelay", "sd"},
    {},
    "Set spam delay in seconds (usage: /spamdelay <seconds>)",
    1
) {}

std::unique_ptr<CommandBase> SpamDelayCommand::clone() const {
    return std::make_unique<SpamDelayCommand>();
}

void SpamDelayCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    if (!client || !client->get_player()) {
        return;
    }

    if (!SpamCommand::s_core) {
        return;
    }

    auto* server = SpamCommand::s_core->get_server();
    if (!server || !server->get_player()) {
        return;
    }

    
    int delay = 5;
    try {
        delay = std::stoi(args[1]);
    } catch (...) {
        delay = 5;
    }

    SpamCommand::s_spam_delay = delay;
    
    
    packet::Variant var{};
    var.add("OnConsoleMessage");
    var.add(fmt::format("`0[`bSkriptProxy`0] `9Spam delay set to `b{} `9seconds", delay));
    
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
    
    server->get_player()->send_packet(bs.get_data(), 0);
    
    spdlog::info("Spam delay set to: {} seconds", delay);
}

} 
