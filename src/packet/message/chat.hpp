#pragma once
#include <format>

#include "../packet_types.hpp"
#include "../packet_helper.hpp"

namespace packet::message {
struct Log : NetMessage<NetMessageType::NET_MESSAGE_GAME_MESSAGE> {
    std::string msg;

    void write(ByteStream<std::uint16_t>& byte_stream)
    {
        TextParse text_parse{};
        text_parse.add("action", { "log" });
        text_parse.add("msg", { msg });
        byte_stream.write(text_parse.get_raw(), false);
    }
};

struct Chat : NetMessage<NetMessageType::NET_MESSAGE_GAME_MESSAGE> {
    std::string message;

    void write(ByteStream<std::uint16_t>& byte_stream)
    {
        TextParse text_parse{};
        text_parse.add("action", { "input" });
        text_parse.add("text", { message });
        byte_stream.write(text_parse.get_raw(), false);
    }
};
}