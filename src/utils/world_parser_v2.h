#pragma once

#include <cstdint>
#include <vector>
#include <string>

namespace world_v2 {


struct LockData {
    uint32_t owner_uid = 0;
    std::vector<uint32_t> access_list;
    uint8_t minimum_level = 0;
    bool is_public = false;
    
    bool has_data() const { return owner_uid != 0; }
};


struct VendingData {
    uint32_t item_id = 0;
    int32_t price = 0;  
    
    bool has_data() const { return item_id != 0; }
    bool is_world_lock_price() const { return price < 0; }
    int32_t get_price() const { return price < 0 ? -price : price; }
};


struct DoorData {
    std::string destination;  
    std::string label;        
    
    bool has_data() const { return !destination.empty(); }
};


struct DonationData {
    std::string entry1;
    std::string entry2;
    std::string entry3;
    uint8_t tail_flag = 0;
    std::vector<uint8_t> raw_extra_bytes;

    bool has_data() const {
        return !entry1.empty() || !entry2.empty() || !entry3.empty() || !raw_extra_bytes.empty();
    }
};


struct DroppedItem {
    uint16_t id = 0;
    float x = 0.0f;
    float y = 0.0f;
    uint32_t count = 0;
    uint8_t flags = 0;
    uint32_t uid = 0;
};


struct Tile {
    uint16_t fg = 0;
    uint16_t bg = 0;
    uint16_t parent_index = 0;
    uint16_t flags = 0;
    uint8_t extra_type = 0;
    
    
    uint32_t x = 0;
    uint32_t y = 0;
    
    
    LockData lock_data;
    
    
    VendingData vending_data;
    
    
    DoorData door_data;

    
    DonationData donation_data;
    
    
    bool has_extra() const { return (flags & 0x01) != 0; }
    bool has_parent() const { return (flags & 0x02) != 0; }
    bool is_lock() const { return extra_type == 3; }
    bool is_vending() const { return extra_type == 24; }
    bool is_door() const { return extra_type == 2; }
};


struct World {
    
    uint16_t version = 0;
    uint32_t flags = 0;
    std::string name;
    
    
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t tile_count = 0;
    
    
    std::vector<Tile> tiles;

    
    uint32_t tiles_parsed_from_data = 0;
    uint32_t tiles_filled_as_empty = 0;
    bool hit_eof_early = false;
    
    
    std::vector<DroppedItem> dropped_items;
    uint32_t dropped_items_count = 0;
    uint32_t last_dropped_item_uid = 0;
    
    
    bool is_valid = false;
    bool is_error = false;
    std::string error_message;

    
    bool had_warnings = false;
    
    
    void reset();
    bool parse(const uint8_t* data, size_t size);
    Tile* get_tile(uint32_t x, uint32_t y);
};

} 
