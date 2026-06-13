#pragma once
#include "../packet/message/chat.hpp"
#include "../packet/packet_helper.hpp"
#include "../player/player.hpp"
#include "../core/logger.hpp"
#include <thread>
#include <chrono>

namespace utils {
class PacketUtils {
public:
    static void send_chat_message(player::Player* player, const std::string& message, bool add_prefix = true) {
        if (!player || !player->is_connected()) {
            spdlog::error("Cannot send message: player is null or not connected.");
            return;
        }

        packet::message::Log message_packet{};
        if (add_prefix) {
            message_packet.msg = "`^[SkriptProxy]`o " + message;
        } else {
            message_packet.msg = message;
        }

        if (!packet::PacketHelper::send(message_packet, *player)) {
            spdlog::error("Failed to send chat message packet to player.");
        } else {
            spdlog::debug("Chat message sent: {}", message);
        }
    }

    static void send_delayed_chat_message(player::Player* player, const std::string& message, int delay_ms = 100) {
        std::thread([player, message, delay_ms]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            send_chat_message(player, message, false);
        }).detach();
    }
};
}