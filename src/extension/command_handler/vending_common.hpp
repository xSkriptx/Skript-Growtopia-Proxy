#pragma once
#include <string>
#include <cstdint>

namespace command {

struct VendingEntry {
    std::string world_name;
    std::string timestamp;
    uint32_t x, y;
    uint16_t tile_id;  
    uint32_t item_id;
    int32_t price;
    bool is_wl_price;
    
    std::string get_type() const {
        return tile_id == 9268 ? "Digivend" : "Vending";
    }
};

} 
