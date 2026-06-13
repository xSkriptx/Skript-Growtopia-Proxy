#include "run_command.hpp"

#include "../../packet/packet_types.hpp"
#include "../../packet/packet_variant.hpp"
#include "../../server/server.hpp"
#include "../../utils/byte_stream.hpp"
#include "../../utils/text_parse.hpp"
#include "../../utils/world_manager.hpp"
#include "../../utils/packet_utils.hpp"

#include <chrono>
#include <random>
#include <string>
#include <thread>

namespace command {

core::Core* RunCommand::s_core = nullptr;

namespace {

constexpr int kJoinCount = 12;
constexpr int kDelayMs = 400;
constexpr int kMinLen = 9;
constexpr int kMaxLen = 12;
constexpr char kCharset[] =
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789";
std::atomic<bool> g_run_active = false;
std::atomic<bool> g_run_stop_requested = false;

void send_overlay(core::Core* core, const std::string& text) {
    if (!core || !core->get_server() || !core->get_server()->get_player()) {
        return;
    }

    packet::Variant var{};
    var.add("OnTextOverlay");
    var.add(text);
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
    core->get_server()->get_player()->send_packet(bs.get_data(), 0);
}

void send_join_request(core::Core* core, const std::string& world_name) {
    if (!core || !core->get_client() || !core->get_client()->get_player()) {
        return;
    }

    TextParse text_parse{};
    text_parse.add("action", {"join_request"});
    text_parse.add("name", {world_name});
    text_parse.add("invitedWorld", {"0"});

    ByteStream<std::uint16_t> byte_stream{};
    byte_stream.write(packet::NET_MESSAGE_GAME_MESSAGE);
    byte_stream.write(text_parse.get_raw(), false);
    core->get_client()->get_player()->send_packet(byte_stream.get_data(), 0);
}

std::string random_world_name(std::mt19937& rng) {
    std::uniform_int_distribution<int> len_dist(kMinLen, kMaxLen);
    std::uniform_int_distribution<int> char_dist(0, static_cast<int>(sizeof(kCharset) - 2));

    const int len = len_dist(rng);
    std::string out;
    out.reserve(static_cast<size_t>(len));

    for (int i = 0; i < len; ++i) {
        out.push_back(kCharset[char_dist(rng)]);
    }
    return out;
}

} 

RunCommand::RunCommand() : CommandBase(
    {"run"},
    {},
    "Join 12 random worlds (9-12 alphanumeric chars) with delay",
    0
) {}

std::unique_ptr<CommandBase> RunCommand::clone() const {
    return std::make_unique<RunCommand>(*this);
}

void RunCommand::set_core(core::Core* core) {
    s_core = core;
}

void RunCommand::execute(client::Client* client, const std::vector<std::string>& ) {
    if (!client || !client->get_player() || !s_core) {
        return;
    }

    auto* out = client->get_player();
    if (s_core->get_server() && s_core->get_server()->get_player()) {
        out = s_core->get_server()->get_player();
    }

    if (g_run_active.load()) {
        g_run_stop_requested = true;
        utils::PacketUtils::send_chat_message(out, "`6Run:`o stopped.");
        return;
    }

    utils::PacketUtils::send_chat_message(out, "`2Run:`o joining 12 random worlds...");

    g_run_active = true;
    g_run_stop_requested = false;

    core::Core* core = s_core;
    std::thread([core] {
        std::random_device rd;
        std::mt19937 rng(rd());

        for (int i = 0; i < kJoinCount; ++i) {
            if (g_run_stop_requested.load()) {
                break;
            }
            if (!core || !core->get_client() || !core->get_client()->get_player()) {
                break;
            }

            send_overlay(core, "Please don't move escaping from mods");
            send_join_request(core, random_world_name(rng));
            std::this_thread::sleep_for(std::chrono::milliseconds(kDelayMs));
        }
        g_run_active = false;
        g_run_stop_requested = false;
    }).detach();
}

} 
