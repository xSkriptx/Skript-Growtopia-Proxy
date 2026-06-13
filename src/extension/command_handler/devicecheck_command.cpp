#include "devicecheck_command.hpp"
#include "banall_command.hpp"
#include "../../client/client.hpp"
#include "../../server/server.hpp"
#include "../../packet/packet_variant.hpp"
#include "../../packet/packet_types.hpp"
#include "../../utils/byte_stream.hpp"
#include "../../utils/player_tracker.hpp"
#include "../../utils/packet_utils.hpp"
#include <algorithm>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace command {

core::Core* DeviceCheckCommand::g_core = nullptr;

static void send_console(player::Player* player, const std::string& msg) {
    if (!player) return;

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
    player->send_packet(bs.get_data(), 0);
}

void DeviceCheckCommand::set_core(core::Core* core) {
    g_core = core;
}

DeviceCheckCommand::DeviceCheckCommand() : CommandBase(
    {"devicecheck", "devices"},
    {},
    "Show tracked spawned players and their device type",
    0
) {}

std::unique_ptr<CommandBase> DeviceCheckCommand::clone() const {
    return std::make_unique<DeviceCheckCommand>(*this);
}

void DeviceCheckCommand::execute(client::Client* client, const std::vector<std::string>& args) {
    (void)args;

    if (!client || !client->get_player()) {
        spdlog::error("DeviceCheckCommand: No client/player.");
        return;
    }

    player::Player* target_player = client->get_player();
    if (g_core) {
        if (auto* server = g_core->get_server(); server && server->get_player()) {
            target_player = server->get_player();
        }
    }

    auto players_map = utils::PlayerTracker::get_instance().get_all_players();
    std::vector<utils::PlayerTracker::PlayerInfo> others{};
    others.reserve(players_map.size());

    for (const auto& entry : players_map) {
        const auto& info = entry.second;
        if (!info.is_local && info.netID != 0) {
            others.push_back(info);
        }
    }

    std::sort(others.begin(), others.end(), [](const auto& a, const auto& b) {
        return a.netID < b.netID;
    });

    spdlog::info("[DeviceCheck] Command executed. tracked_players={}", players_map.size());
    send_console(target_player, fmt::format("`2[DeviceCheck] tracked={} non_local={}", players_map.size(), others.size()));

    if (others.empty()) {
        const auto spawned_names = BanallCommand::get_spawned_players();
        if (!spawned_names.empty()) {
            send_console(target_player, fmt::format("`2[DeviceCheck] banall_spawned={}", spawned_names.size()));
            for (const auto& name : spawned_names) {
                send_console(target_player, fmt::format("`w{} `o-> `wUnknown `o(platformID:Unknown)", name));
            }
            return;
        }

        auto local = utils::PlayerTracker::get_instance().get_local_player();
        const std::string local_device = local.device_type.empty()
            ? utils::PlayerTracker::get_device_from_platform_id(local.platform_id)
            : local.device_type;
        const std::string local_platform = local.platform_id.empty() ? "Unknown" : local.platform_id;
        const std::string local_name = local.name.empty() ? "LocalPlayer" : local.name;
        send_console(target_player, fmt::format("`w{} `o(local) `2-> `w{} `o(platformID:{})",
            local_name, local_device, local_platform));
        send_console(target_player, "`4No spawned non-local players tracked yet.");
        return;
    }

    for (const auto& info : others) {
        const std::string device = info.device_type.empty()
            ? utils::PlayerTracker::get_device_from_platform_id(info.platform_id)
            : info.device_type;
        const std::string platform = info.platform_id.empty() ? "Unknown" : info.platform_id;
        const std::string player_name = info.name.empty() ? "Unknown" : info.name;

        send_console(target_player, fmt::format("`w{} `o(netID:{}) `2-> `w{} `o(platformID:{})",
            player_name, info.netID, device, platform));
    }
}

} 
