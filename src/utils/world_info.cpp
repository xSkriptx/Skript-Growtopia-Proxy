#include "world_info.h"
#include "world_tile.h"
#include "extra_tile_data.h"
#include <cstdlib>
#include <cstring>
#include <spdlog/spdlog.h>

using namespace world;

WorldInfo::WorldInfo() : m_raw_data(nullptr), m_raw_data_size(0), m_curr_it(nullptr), 
                         Name(nullptr), Tiles(nullptr), TilesCount(0), 
                         ItemDrops(nullptr), ItemDropsCount(0) {}

WorldInfo::~WorldInfo() {
    reset();
}

bool WorldInfo::serialize(uint8_t* raw_data, size_t size) {
    reset();
    
    m_raw_data = (uint8_t*)malloc(size);
    if (!m_raw_data) return false;
    
    memcpy(m_raw_data, raw_data, size);
    m_raw_data_size = size;
    m_curr_it = m_raw_data;

    
    spdlog::info("WorldInfo: First 20 bytes (hex):");
    std::string hex_str;
    for (int i = 0; i < 20 && i < (int)size; i++) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%02X ", raw_data[i]);
        hex_str += buf;
    }
    spdlog::info("  {}", hex_str);
    
    
    
    
    
    
    m_curr_it = m_raw_data;
    if (try_parse_with_header()) {
        spdlog::info("WorldInfo: Successfully parsed WITH version+flags header");
        return true;
    }
    
    
    m_curr_it = m_raw_data;
    if (try_parse_without_header()) {
        spdlog::info("WorldInfo: Successfully parsed WITHOUT version+flags header");
        return true;
    }
    
    spdlog::error("WorldInfo: Both parsing methods failed!");
    return false;
}

bool WorldInfo::try_parse_with_header() {
    try {
        
        if (m_raw_data_size < 2) return false;
        uint16_t version = get_int<uint16_t>();
        spdlog::info("  Trying WITH header: version = {}", version);
        
        
        if (m_raw_data_size < 6) return false;
        uint32_t flags = get_int<uint32_t>();
        
        return parse_common();
    } catch (...) {
        return false;
    }
}

bool WorldInfo::try_parse_without_header() {
    try {
        spdlog::info("  Trying WITHOUT header (direct to name)");
        return parse_common();
    } catch (...) {
        return false;
    }
}

bool WorldInfo::parse_common() {
    try {
        
        uint16_t name_len = get_int<uint16_t>();
        spdlog::info("  Name length = {}", name_len);
        
        if (name_len == 0 || name_len > 100) return false;
        
        Name = (char*)malloc(name_len + 1);
        if (!Name) return false;
        
        for (uint16_t i = 0; i < name_len; i++) {
            Name[i] = get_int<char>();
        }
        Name[name_len] = '\0';
        
        spdlog::info("  World name = '{}'", Name);
        
        
        Width = get_int<uint32_t>();
        Height = get_int<uint32_t>();
        TotalTiles = get_int<uint32_t>();
        
        spdlog::info("  Dimensions: {}x{} = {} tiles", Width, Height, TotalTiles);
        
        
        if (TotalTiles == 0 || TotalTiles > 0xFE01) {
            spdlog::error("  Invalid tile count: {}", TotalTiles);
            return false;
        }
        if (Width == 0 || Height == 0) {
            spdlog::error("  Invalid dimensions: {}x{}", Width, Height);
            return false;
        }
        if (Width * Height != TotalTiles) {
            spdlog::error("  Dimension mismatch: {} * {} != {}", Width, Height, TotalTiles);
            return false;
        }
        
        
        m_curr_it += 5;
        
        
        Tiles = (Tile*)malloc(TotalTiles * sizeof(Tile));
        if (!Tiles) return false;
        TilesCount = TotalTiles;
        memset(Tiles, 0, TotalTiles * sizeof(Tile));

        spdlog::info("  Parsing {} tiles...", TotalTiles);
        
        
        for (uint32_t i = 0; i < TotalTiles; i++) {
            if (!get_next_tile(Tiles[i])) {
                spdlog::error("  Failed at tile {} (offset {})", i, (size_t)(m_curr_it - m_raw_data));
                return false;
            }
            
            
            if (i < 5) {
                spdlog::info("  Tile {}: Fg={}, Bg={}", i, Tiles[i].Fg, Tiles[i].Bg);
            }
        }

        spdlog::info("  All tiles parsed successfully!");
        
        
        m_curr_it += 12;
        
        
        parse_item_drops();

        
        if (m_raw_data_size - (m_curr_it - m_raw_data) >= 2) {
            BaseWeather = get_int<uint16_t>();
        }
        if (m_raw_data_size - (m_curr_it - m_raw_data) >= 2) {
            m_curr_it += 2; 
        }
        if (m_raw_data_size - (m_curr_it - m_raw_data) >= 2) {
            CurrentWeather = get_int<uint16_t>();
        }
        
        return true;
    } catch (...) {
        spdlog::error("  Exception during parsing");
        return false;
    }
}

void WorldInfo::reset() {
    free(m_raw_data);
    m_raw_data = nullptr;
    m_raw_data_size = 0;
    m_curr_it = nullptr;
    
    free(Name);
    Name = nullptr;
    
    free(Tiles);
    Tiles = nullptr;
    TilesCount = 0;
    
    free(ItemDrops);
    ItemDrops = nullptr;
    ItemDropsCount = 0;
    
    Width = Height = TotalTiles = 0;
    BaseWeather = CurrentWeather = 0;
}

template <typename T>
T WorldInfo::get_int() {
    T value = *((T*)m_curr_it);
    m_curr_it += sizeof(T);
    return value;
}

const char* WorldInfo::get_str() {
    uint16_t length = get_int<uint16_t>();
    char* str = (char*)malloc(length + 1);
    if (!str) return "";
    
    for (uint16_t i = 0; i < length; i++) {
        str[i] = get_int<char>();
    }
    str[length] = '\0';
    return str;
}

bool WorldInfo::get_next_tile(Tile& out) {
    if (m_curr_it - m_raw_data + 8 > m_raw_data_size) return false;
    
    
    out.Fg = get_int<uint16_t>();
    out.Bg = get_int<uint16_t>();
    out.ParentTileIndex = get_int<uint16_t>();  
    
    uint16_t flags_value = get_int<uint16_t>();
    out.Flags.Value = static_cast<world::TileFlag>(flags_value);

    
    out.LockIndex = 0;
    out.ExtraTileDataType = eExtraTileDataType::None;

    
    if (out.Flags.bLocked) {  
        if (m_curr_it - m_raw_data + 2 > m_raw_data_size) return false;
        
        out.LockIndex = get_int<uint16_t>();
    }

    
    if (out.Flags.bHasTileExtra) {
        if (m_curr_it - m_raw_data + 1 > m_raw_data_size) return false;
        out.ExtraTileDataType = static_cast<eExtraTileDataType>(get_int<uint8_t>());
        
        
        if (!parse_tile_extra(out)) {
            uint8_t raw_type = static_cast<uint8_t>(out.ExtraTileDataType);
            spdlog::error("  Failed to parse extra type: {} at offset {}", raw_type, (size_t)(m_curr_it - m_raw_data));
            return false;
        }
    }

    return true;
}


bool WorldInfo::parse_tile_extra(Tile& tile) {
    if (!tile.Flags.bHasTileExtra) {
        return true;
    }
    
    
    uint8_t type = static_cast<uint8_t>(tile.ExtraTileDataType);
    
    try {
        switch (type) {
            case 0:  
                break;
                
            case 1:  
                {
                    uint16_t str_len = get_int<uint16_t>();
                    m_curr_it += str_len;
                    m_curr_it += 4; 
                }
                break;
                
            case 2:  
                {
                    uint16_t str_len = get_int<uint16_t>();
                    m_curr_it += str_len;
                    m_curr_it += 1; 
                }
                break;
                
            case 3:  
                m_curr_it += 1; 
                m_curr_it += 4; 
                {
                    uint32_t access_count = get_int<uint32_t>();
                    m_curr_it += access_count * 4;
                }
                m_curr_it += 1; 
                m_curr_it += 7; 
                break;
                
            case 4:  
                m_curr_it += 4; 
                m_curr_it += 1; 
                break;
                
            case 6:  
            case 7:  
            case 12: 
                
                for (int i = 0; i < 3; i++) {
                    uint16_t str_len = get_int<uint16_t>();
                    m_curr_it += str_len;
                }
                m_curr_it += 1;
                break;
                
            case 8:  
                m_curr_it += 1;
                break;
                
            case 9:  
                m_curr_it += 4;
                break;
                
            case 10: 
                m_curr_it += 4;
                m_curr_it += 1;
                break;
                
            case 11: 
                m_curr_it += 4;
                {
                    uint16_t str_len = get_int<uint16_t>();
                    m_curr_it += str_len;
                }
                break;
                
            case 14: 
                {
                    uint16_t str_len = get_int<uint16_t>();
                    m_curr_it += str_len;
                    m_curr_it += 1;
                    m_curr_it += 4;
                    m_curr_it += 2 * 9;
                }
                break;
                
            case 15: 
                m_curr_it += 4;
                break;
                
            case 16: 
                m_curr_it += 1;
                break;
                
            case 17: 
                break;
                
            case 18: 
                m_curr_it += 1;
                m_curr_it += 4;
                break;
                
            case 19: 
                m_curr_it += 2 * 9;
                break;
                
            case 20: 
                break;
                
            case 21: 
                {
                    uint16_t str_len = get_int<uint16_t>();
                    m_curr_it += str_len;
                }
                break;
                
            case 22: 
                {
                    uint16_t str_len = get_int<uint16_t>();
                    m_curr_it += str_len;
                    m_curr_it += 4;
                    m_curr_it += 1;
                }
                break;
                
            case 23: 
                m_curr_it += 4;
                break;
                
            case 24: 
                m_curr_it += 4;
                m_curr_it += 4;
                break;
                
            case 25: 
                {
                    m_curr_it += 1;
                    uint32_t fish_count = get_int<uint32_t>();
                    m_curr_it += fish_count * 8;
                }
                break;
                
            case 26: 
                m_curr_it += 5;
                break;
                
            case 27: 
                m_curr_it += 4;
                break;
                
            case 28: 
                m_curr_it += 2;
                m_curr_it += 4;
                break;
                
            case 29: 
                m_curr_it += 1;
                m_curr_it += 4;
                break;
                
            case 30: 
                m_curr_it += 1;
                {
                    uint16_t str_len = get_int<uint16_t>();
                    m_curr_it += str_len;
                }
                m_curr_it += 16;
                break;
                
            case 32: 
                {
                    uint32_t bolt_count = get_int<uint32_t>();
                    m_curr_it += bolt_count * 4;
                }
                break;
                
            case 33: 
                {
                    uint16_t str_len = get_int<uint16_t>();
                    m_curr_it += str_len;
                }
                break;
                
            case 34: 
                break;
                
            case 35: 
                m_curr_it += 4;
                {
                    uint16_t str_len = get_int<uint16_t>();
                    m_curr_it += str_len;
                }
                break;
                
            case 40: 
                m_curr_it += 4;
                break;
                
            case 42: 
                break;
                
            case 43: 
                m_curr_it += 16;
                break;
                
            case 44: 
                m_curr_it += 1;
                m_curr_it += 4;
                {
                    uint32_t count = get_int<uint32_t>();
                    m_curr_it += count * 4;
                }
                break;
                
            case 45: 
                break;
                
            case 46: 
                {
                    uint16_t str_len = get_int<uint16_t>();
                    m_curr_it += str_len;
                }
                m_curr_it += 4;
                m_curr_it += 1;
                break;
                
            case 48: 
                {
                    uint16_t str_len = get_int<uint16_t>();
                    m_curr_it += str_len;
                }
                m_curr_it += 28;
                break;
                
            case 49: 
                m_curr_it += 4;
                m_curr_it += 4;
                m_curr_it += 1;
                break;
                
            case 50: 
                m_curr_it += 4;
                break;
                
            case 51: 
                break;
                
            case 52: 
                break;
                
            case 53: 
                m_curr_it += 8;
                break;
                
            case 54: 
                {
                    uint32_t item_count = get_int<uint32_t>();
                    m_curr_it += item_count * 8;
                }
                break;
                
            case 55: 
                {
                    m_curr_it += 4;
                    uint32_t ing_count = get_int<uint32_t>();
                    m_curr_it += ing_count * 8;
                    m_curr_it += 12;
                }
                break;
                
            case 56: 
                {
                    uint16_t str_len = get_int<uint16_t>();
                    m_curr_it += str_len;
                }
                m_curr_it += 4;
                break;
                
            case 57: 
                m_curr_it += 4;
                break;
                
            case 58: 
                break;
                
            case 59: 
                break;
                
            case 60: 
                m_curr_it += 4;
                m_curr_it += 1;
                break;
                
            case 61: 
                m_curr_it += 35;
                break;
                
            case 62: 
                m_curr_it += 14;
                break;
                
            case 63: 
                {
                    m_curr_it += 4; 
                    m_curr_it += 4; 
                    uint32_t cmd_count = get_int<uint32_t>();
                    
                    for (uint32_t i = 0; i < cmd_count; i++) {
                        m_curr_it += 4; 
                        m_curr_it += 4; 
                        m_curr_it += 7; 
                    }
                }
                break;
                
            case 65: 
                m_curr_it += 17;
                break;
                
            case 66: 
                m_curr_it += 1;
                break;
                
            case 80: 
                m_curr_it += 8;
                break;
                
            case 81: 
                m_curr_it += 8;
                break;
                
            default:
                
                break;
        }
        
        return true;
    } catch (...) {
        spdlog::error("Exception parsing tile extra type {}", type);
        return false;
    }
}

void WorldInfo::parse_item_drops() {
    
    if (m_curr_it - m_raw_data + 12 > m_raw_data_size) return;
    m_curr_it += 12;
}
