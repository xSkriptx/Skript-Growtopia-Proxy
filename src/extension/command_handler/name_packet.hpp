#pragma once
#include "../../packet/packet_helper.hpp"
#include "../../utils/text_parse.hpp"

namespace packet::game {

struct NameChange : NetPacket<PACKET_CALL_FUNCTION> {
    std::string new_name;

    void write(GameUpdatePacket& game_update_packet, std::vector<std::byte>& ext_data) {
        game_update_packet.net_id = -1; 
        
        Variant variant{
            "OnNameChanged",
            new_name,
            0
        };

        ext_data = variant.serialize();
    }
};

} 