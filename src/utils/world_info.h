#pragma once



#include <cstdint>
#include <cstring>

namespace world {


struct Tile;
struct DroppedItemInfo;

class WorldInfo {
public:
    WorldInfo();
    ~WorldInfo();

    bool serialize(uint8_t* raw_data, size_t size);
    void reset();

    char* Name; 
    uint32_t Width, Height, TotalTiles;
    Tile* Tiles; 
    size_t TilesCount;
    DroppedItemInfo* ItemDrops; 
    size_t ItemDropsCount;
    uint16_t BaseWeather, CurrentWeather;

private:
    uint8_t* m_raw_data;
    size_t m_raw_data_size;
    uint8_t* m_curr_it;

    template <typename T> T get_int();
    uint8_t* get_byte_array(size_t sz, uint8_t* dest);
    void copy_raw_byte_array(void* dest, size_t sz);
    const char* get_str(); 
    bool get_next_tile(Tile& out);
    bool parse_tile_extra(Tile& tile);
    void parse_item_drops();
    
    
    bool try_parse_with_header();
    bool try_parse_without_header();
    bool parse_common();
};

struct DroppedItemInfo {
    uint16_t ItemId;
    float X, Y;
    uint32_t Amount;
    uint8_t Flag;
    uint32_t Uid;
};

}
