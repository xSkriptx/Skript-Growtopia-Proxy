#include "doorbf_command.hpp"
#include "doorbf_tree.hpp"
#include "../../client/client.hpp"
#include "../../player/player.hpp"
#include "../../server/server.hpp"
#include "../../utils/world_manager.hpp"
#include "../../utils/player_tracker.hpp"
#include "../../utils/packet_utils.hpp"
#include "../../utils/text_parse.hpp"
#include "../../utils/byte_stream.hpp"
#include "../../packet/packet_types.hpp"
#include "../../packet/packet_variant.hpp"
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace command {

core::Core* DoorBFCommand::s_core = nullptr;

namespace {

constexpr int kDefaultMaxAttempts = 2500;
constexpr int kMinDelayMs = 150;
constexpr int kBaseProbeDelayMs = 900;
constexpr int kSuccessPollWindowMs = 1800;
constexpr int kSuccessPollStepMs = 100;
std::atomic<bool> g_doorbf_running = false;
std::atomic<bool> g_doorbf_stop_requested = false;

void send_door_packet(core::Core* core, const std::string& world_name, const std::string& door_id) {
    if (!core || !core->get_client() || !core->get_client()->get_player()) {
        return;
    }

    TextParse text_parse{};
    text_parse.add("action", {"join_request"});
    text_parse.add("name", {world_name + "|" + door_id + " "});
    text_parse.add("invitedWorld", {"0"});

    ByteStream<std::uint16_t> byte_stream{};
    byte_stream.write(packet::NET_MESSAGE_GAME_MESSAGE);
    byte_stream.write(text_parse.get_raw(), false);
    core->get_client()->get_player()->send_packet(byte_stream.get_data(), 0);
}

bool read_player_tile(core::Core* core, int& tile_x, int& tile_y) {
    if (!core) return false;

    
    
    auto& tracker = utils::PlayerTracker::get_instance();
    auto local = tracker.get_local_player();
    if (local.netID > 0) {
        auto pos = tracker.get_player_position(local.netID);
        tile_x = static_cast<int>(pos.x / 32.0f);
        tile_y = static_cast<int>(pos.y / 32.0f);
        return true;
    }

    try {
        const std::string pos_x_str = core->get_config().get<std::string>("player.position.x");
        const std::string pos_y_str = core->get_config().get<std::string>("player.position.y");
        const float pos_x = std::stof(pos_x_str);
        const float pos_y = std::stof(pos_y_str);
        tile_x = static_cast<int>(pos_x / 32.0f);
        tile_y = static_cast<int>(pos_y / 32.0f);
        return true;
    } catch (...) {
        return false;
    }
}

bool wait_for_player_tile(core::Core* core, int& tile_x, int& tile_y, int timeout_ms, int step_ms = 100) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        if (read_player_tile(core, tile_x, tile_y)) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(step_ms));
    }
    return false;
}

void send_overlay(core::Core* core, const std::string& text) {
    if (!core || !core->get_server() || !core->get_server()->get_player()) return;

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

std::vector<std::string> build_candidates(int max_attempts) {
    std::vector<std::string> out;
    out.reserve(static_cast<size_t>(max_attempts) + 512);
    std::unordered_set<std::string> seen;

    auto push_unique = [&](const std::string& id) {
        if (id.empty()) return;
        if (static_cast<int>(out.size()) >= max_attempts) return;
        if (seen.insert(id).second) out.push_back(id);
    };

    const std::vector<std::string> words = {
        "me","in","out","go","lol","ok","poor","nub","pro","noob","door","main","home",
        "spawn","farm","vend","shop","buy","sell","top","bot","left","right","up","down",
        "one","two","three","four","five","six","seven","eight","nine","ten"
    };

    for (const auto& w : words) push_unique(w);
    for (int i = 0; i <= 999 && static_cast<int>(out.size()) < max_attempts; ++i) {
        push_unique(std::to_string(i));
    }

    for (const auto& w : words) {
        for (int i = 0; i <= 199 && static_cast<int>(out.size()) < max_attempts; ++i) {
            push_unique(w + std::to_string(i));
            push_unique(std::to_string(i) + w);
        }
    }

    for (size_t i = 0; i < words.size() && static_cast<int>(out.size()) < max_attempts; ++i) {
        for (size_t j = 0; j < words.size() && static_cast<int>(out.size()) < max_attempts; ++j) {
            push_unique(words[i] + words[j]);
            push_unique(words[i] + "_" + words[j]);
        }
    }

    return out;
}

} 

DoorBFCommand::DoorBFCommand() : CommandBase(
    {"doorbf"},
    {},
    "Bruteforce door IDs using white-door position change detection",
    0
) {}

std::unique_ptr<CommandBase> DoorBFCommand::clone() const {
    return std::make_unique<DoorBFCommand>(*this);
}

void DoorBFCommand::set_core(core::Core* core) {
    s_core = core;
}

void DoorBFCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    if (!client || !client->get_player() || !s_core) {
        return;
    }

    if (g_doorbf_running.load()) {
        g_doorbf_stop_requested = true;
        utils::PacketUtils::send_chat_message(client->get_player(), "`6DoorBF:`o stop requested.");
        return;
    }

    std::string world_name = utils::WorldManager::get_instance().get_world_name();
    if (world_name.empty()) {
        utils::PacketUtils::send_chat_message(client->get_player(), "`4Error: You must be in a world to use /doorbf");
        return;
    }

    const auto input = doorbf_tree::parse_input(args, kDefaultMaxAttempts);
    const int max_attempts = input.max_attempts;

    if (!input.error.empty()) {
        g_doorbf_running = false;
        utils::PacketUtils::send_chat_message(client->get_player(), fmt::format("`4{}", input.error));
        return;
    }

    if (input.show_help) {
        g_doorbf_running = false;
        utils::PacketUtils::send_chat_message(client->get_player(), "`2DoorBF help:");
        utils::PacketUtils::send_chat_message(client->get_player(), "`9/doorbf`w -> run with saved list (or default list if none)");
        utils::PacketUtils::send_chat_message(client->get_player(), "`9/doorbf 1-100 good lol pep`w -> save + run custom list");
        utils::PacketUtils::send_chat_message(client->get_player(), "`9/doorbf ?list`w -> show all saved ids");
        utils::PacketUtils::send_chat_message(client->get_player(), "`9/doorbf ?clearlist`w -> clear saved custom list");
        utils::PacketUtils::send_chat_message(client->get_player(), "`9/doorbf 1-300 out go`w -> save + run custom list");
        return;
    }

    if (input.show_list) {
        g_doorbf_running = false;
        const auto ids = doorbf_tree::load_custom_ids(0);
        if (ids.empty()) {
            utils::PacketUtils::send_chat_message(client->get_player(), "`4DoorBF: no saved ids.");
            return;
        }
        utils::PacketUtils::send_chat_message(client->get_player(),
            fmt::format("`2DoorBF saved ids ({})", ids.size()));
        for (const auto& id : ids) {
            utils::PacketUtils::send_chat_message(client->get_player(), fmt::format("`9- `w{}", id));
        }
        return;
    }

    if (input.clear_list) {
        g_doorbf_running = false;
        doorbf_tree::clear_custom_ids();
        utils::PacketUtils::send_chat_message(client->get_player(), "`2DoorBF: saved custom bruteforce list cleared.");
        return;
    }

    player::Player* out = client->get_player();
    if (s_core->get_server() && s_core->get_server()->get_player()) {
        out = s_core->get_server()->get_player();
    }

    std::vector<std::string> run_candidates;
    if (input.has_custom_ids) {
        run_candidates = input.custom_ids;
        doorbf_tree::save_custom_ids(run_candidates);
        utils::PacketUtils::send_chat_message(
            out,
            fmt::format("`2DoorBF:`o saved {} custom ids and starting bruteforce.", run_candidates.size())
        );
    } else {
        if (doorbf_tree::saved_custom_count() > 0) {
            run_candidates = doorbf_tree::load_custom_ids(max_attempts);
            utils::PacketUtils::send_chat_message(
                out,
                fmt::format("`2DoorBF:`o using saved custom list ({} ids).", run_candidates.size())
            );
        } else {
            utils::PacketUtils::send_chat_message(
                out,
                "`4No ID has been registered yet in Dev Console. Please add the IDs first."
            );
            g_doorbf_running = false;
            return;
        }
    }

    g_doorbf_running = true;
    g_doorbf_stop_requested = false;

    std::thread([world_name, run_candidates = std::move(run_candidates)]() mutable {

        core::Core* core = DoorBFCommand::s_core;
        if (!core) {
            g_doorbf_running = false;
            return;
        }

        if (run_candidates.empty()) {
            if (core->get_server() && core->get_server()->get_player()) {
                utils::PacketUtils::send_chat_message(core->get_server()->get_player(),
                    "`4DoorBF: no IDs available to test.");
            }
            g_doorbf_running = false;
            return;
        }

        
        send_door_packet(core, world_name, "");
        std::this_thread::sleep_for(std::chrono::milliseconds(kBaseProbeDelayMs));
        std::this_thread::sleep_for(std::chrono::milliseconds(300));

        int base_x = 0, base_y = 0;
        bool has_baseline_pos = wait_for_player_tile(core, base_x, base_y, 3000, 120);
        if (!has_baseline_pos) {
            if (core->get_server() && core->get_server()->get_player()) {
                utils::PacketUtils::send_chat_message(core->get_server()->get_player(),
                    "`6DoorBF: position tracker is delayed; continuing with world-change checks.");
            }
        }

        auto& candidates = run_candidates;
        spdlog::info("DoorBF: baseline tile=({}, {}), has_pos={}, candidates={}",
            base_x, base_y, has_baseline_pos, candidates.size());

        for (size_t i = 0; i < candidates.size(); ++i) {
            if (g_doorbf_stop_requested.load()) {
                if (core->get_server() && core->get_server()->get_player()) {
                    utils::PacketUtils::send_chat_message(core->get_server()->get_player(), "`6DoorBF:`o stopped.");
                }
                g_doorbf_running = false;
                return;
            }

            const std::string& id = candidates[i];
            send_overlay(core, fmt::format("`9Checking door id: `w{} `8(please do not move)", id));
            send_door_packet(core, world_name, id);
            std::this_thread::sleep_for(std::chrono::milliseconds(kMinDelayMs));

            bool found = false;
            int found_x = base_x;
            int found_y = base_y;
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(kSuccessPollWindowMs);
            while (std::chrono::steady_clock::now() < deadline) {
                if (g_doorbf_stop_requested.load()) {
                    if (core->get_server() && core->get_server()->get_player()) {
                        utils::PacketUtils::send_chat_message(core->get_server()->get_player(), "`6DoorBF:`o stopped.");
                    }
                    g_doorbf_running = false;
                    return;
                }

                std::string cur_world = utils::WorldManager::get_instance().get_world_name();
                if (!cur_world.empty() && cur_world != world_name) {
                    found = true;
                    break;
                }

                int cur_x = 0, cur_y = 0;
                if (read_player_tile(core, cur_x, cur_y)) {
                    if (!has_baseline_pos) {
                        
                        base_x = cur_x;
                        base_y = cur_y;
                        has_baseline_pos = true;
                    } else if (cur_x != base_x || cur_y != base_y) {
                        found = true;
                        found_x = cur_x;
                        found_y = cur_y;
                        break;
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(kSuccessPollStepMs));
            }

            if (found) {
                const std::string found_msg = fmt::format("`2DoorBF Found ID:`w {} ", id);
                send_overlay(core, found_msg);
                if (core->get_server() && core->get_server()->get_player()) {
                    utils::PacketUtils::send_chat_message(core->get_server()->get_player(), found_msg);
                }
                spdlog::info("DoorBF: success id='{}' pos=({}, {}) -> ({}, {}) after {} attempts",
                    id, base_x, base_y, found_x, found_y, i + 1);
                g_doorbf_running = false;
                return;
            }
        }

        if (core->get_server() && core->get_server()->get_player()) {
            utils::PacketUtils::send_chat_message(core->get_server()->get_player(),
                fmt::format("`4DoorBF:`o no valid door id found in {} attempts.", candidates.size()));
        }
        g_doorbf_running = false;
    }).detach();
}

} 
