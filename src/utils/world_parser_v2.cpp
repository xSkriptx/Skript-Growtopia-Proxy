#include "world_parser_v2.h"
#include <spdlog/spdlog.h>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace world_v2 {


class BinaryReader {
public:
    
    
    BinaryReader(const uint8_t* data, size_t size, bool* warn_flag = nullptr) 
        : m_data(data), m_size(size), m_pos(0), m_warn_flag(warn_flag) {}
    
    template<typename T>
    T read() {
        if (m_pos + sizeof(T) > m_size) {
            if (m_warn_flag) *m_warn_flag = true;
            throw std::runtime_error("Read past end of data");
        }
        T value;
        memcpy(&value, m_data + m_pos, sizeof(T));
        m_pos += sizeof(T);
        
        
        
        if constexpr (std::is_same_v<T, uint32_t> || std::is_same_v<T, int32_t>) {
            
            
            if (value > 4290000000) {
                spdlog::debug("Impossible value read: {} at position {}", value, m_pos - sizeof(T));
                if (m_warn_flag) *m_warn_flag = true;
            }
        }
        
        return value;
    }
    
    void skip(size_t bytes) {
        
        if (bytes > m_size) {
            spdlog::error("Attempting to skip more than file size: {} bytes at position {} (size: {})",
                bytes, m_pos, m_size);
            if (m_warn_flag) *m_warn_flag = true;
            throw std::runtime_error("Skip amount larger than file");
        }
        
        if (m_pos + bytes > m_size) {
            spdlog::error("Skip past end of data: trying to skip {} bytes at position {} (size: {})", 
                bytes, m_pos, m_size);
            if (m_warn_flag) *m_warn_flag = true;
            throw std::runtime_error("Skip past end of data");
        }
        m_pos += bytes;
    }

    void rewind(size_t bytes) {
        if (bytes > m_pos) {
            throw std::runtime_error("Rewind past beginning of data");
        }
        m_pos -= bytes;
    }
    
    std::string read_string(uint16_t length) {
        
        if (length > 65000) {
            spdlog::warn("Large string length: {} at position {}", length, m_pos);
            if (m_warn_flag) *m_warn_flag = true;
        }
        
        if (m_pos + length > m_size) {
            throw std::runtime_error("String read past end of data");
        }
        std::string str((const char*)(m_data + m_pos), length);
        m_pos += length;
        return str;
    }
    
    size_t position() const { return m_pos; }
    
    void seek(size_t pos) {
        if (pos > m_size) {
            spdlog::error("Attempting to seek past file size: {} (limit: {})", pos, m_size);
            if (m_warn_flag) *m_warn_flag = true;
            throw std::runtime_error("Seek past end of data");
        }
        m_pos = pos;
    }
    
private:
    const uint8_t* m_data;
    size_t m_size;
    size_t m_pos;
    bool* m_warn_flag; 
};

void World::reset() {
    version = 0;
    flags = 0;
    name.clear();
    width = height = tile_count = 0;
    tiles.clear();
    is_valid = false;
    is_error = false;
    error_message.clear();
    had_warnings = false;
}

bool World::parse(const uint8_t* data, size_t size) {
    reset();
    
    try {
        
        BinaryReader reader(data, size, &had_warnings);
        
        
        version = reader.read<uint16_t>();
        if (version < 0x19) {
            is_error = true;
            error_message = "Unsupported version";
            had_warnings = true;
            return false;
        }
        
        flags = reader.read<uint32_t>();
        
        uint16_t name_len = reader.read<uint16_t>();
        name = reader.read_string(name_len);
        
        width = reader.read<uint32_t>();
        height = reader.read<uint32_t>();
        
        if (width == 0 || height == 0 || width > 200 || height > 200) {
            throw std::runtime_error("Invalid world dimensions: " + std::to_string(width) + "x" + std::to_string(height));
        }
        tile_count = reader.read<uint32_t>();
        
        
        reader.skip(5);
        
        spdlog::info("Parsing world: '{}' ({}x{}, {} tiles)", name, width, height, tile_count);
        spdlog::info("World data size: {} bytes ({:.2f} bytes per tile average)", 
            size, (double)size / tile_count);
        
        if (tile_count > 0xFE01) {
            is_error = true;
            error_message = "Tile count too large";
            had_warnings = true;
            return false;
        }
        
        tiles.reserve(tile_count);
        
        uint32_t tiles_parsed_from_data = 0;
        uint32_t tiles_filled_as_empty = 0;
        
        
        for (uint32_t count = 0; count < tile_count; count++) {
            try {
                
                if (reader.position() + 8 > size) {
                    break;
                }
                
                
                
                
                uint16_t test_fg = 0;
                uint16_t test_bg = 0;
                size_t resync_attempts = 0;
                while (resync_attempts < 1024 && reader.position() + 8 <= size) {
                    size_t p = reader.position();
                    test_fg = *reinterpret_cast<const uint16_t*>(data + p);
                    test_bg = *reinterpret_cast<const uint16_t*>(data + p + 2);
                    
                    
                    
                    if (test_fg < 18000 && test_bg < 18000) break;
                    
                    reader.skip(1);
                    resync_attempts++;
                }
                if (resync_attempts > 0) {
                    spdlog::debug("Tile {} resynced after skipping {} bytes", count, resync_attempts);
                    had_warnings = true;
                }
                
                tiles_parsed_from_data++;
                
                Tile tile;
                tile.x = count % width;
                tile.y = count / width;
                
                
                tile.fg = reader.read<uint16_t>();
                tile.bg = reader.read<uint16_t>();
                tile.parent_index = reader.read<uint16_t>();
                tile.flags = reader.read<uint16_t>();
            
            
            if (tile.flags & 0x02) {
                reader.read<uint16_t>(); 
            }
            
            
            if (tile.flags & 0x01) {
                tile.extra_type = reader.read<uint8_t>();
                
                
                if (count >= 3845 && count <= 3880) {
                    spdlog::debug("Tile {}: fg={}, bg={}, flags=0x{:04x}, extra_type={}, pos=({},{}), reader_pos={}", 
                        count, tile.fg, tile.bg, tile.flags, (int)tile.extra_type, tile.x, tile.y, reader.position());
                }
                
                
                bool extra_parsed = false;
                
                
                switch (tile.extra_type) {
                    case 0:
                        extra_parsed = true;
                        
                        break;
                    case 1: 
                        extra_parsed = true;
                        {
                            uint16_t str_len = reader.read<uint16_t>();
                            reader.skip(str_len); 
                            reader.skip(1); 
                        }
                        break;
                        
                    case 2: 
                        extra_parsed = true;
                        {
                            uint16_t str_len = reader.read<uint16_t>();
                            reader.skip(str_len); 
                            reader.skip(4); 
                            
                            if (tile.flags & 0x04) reader.skip(1); 
                        }
                        break;
                        
                    case 3: 
                        extra_parsed = true;
                        {
                            uint8_t settings = reader.read<uint8_t>();
                            tile.lock_data.owner_uid = reader.read<uint32_t>();
                            
                            uint32_t access_count = reader.read<uint32_t>();
                            
                            if (access_count > 100000) {
                                spdlog::warn("Lock admin count too large at ({}, {}): {} (clamping)",
                                    tile.x, tile.y, access_count);
                                had_warnings = true;
                                access_count = 100000;
                            }
                            
                            tile.lock_data.access_list.reserve(access_count);
                            for (uint32_t i = 0; i < access_count; i++) {
                                uint32_t admin_uid = reader.read<uint32_t>();
                                tile.lock_data.access_list.push_back(admin_uid);
                            }
                            
                            tile.lock_data.minimum_level = reader.read<uint8_t>();
                            tile.lock_data.is_public = (settings & 0x01) != 0;
                            
                            reader.skip(7); 
                            
                            
                            if (tile.fg == 5814) {
                                reader.skip(16);
                            }
                            
                            
                            spdlog::info("LOCK FOUND at tile {} ({}, {}): fg={}, owner={}, admins={}, public={}", 
                                count, tile.x, tile.y, tile.fg, tile.lock_data.owner_uid, 
                                tile.lock_data.access_list.size(), tile.lock_data.is_public);
                        }
                        break;
                        
                    case 4: 
                        reader.skip(4); 
                        reader.skip(1); 
                        break;
                        
                    case 6: 
                    case 7: 
                    case 12: 
                        
                        for (int i = 0; i < 3; i++) {
                            uint16_t str_len = reader.read<uint16_t>();
                            reader.skip(str_len);
                        }
                        reader.skip(1);
                        break;
                        
                    case 8: 
                        reader.skip(1);
                        break;
                        
                    case 9: 
                        reader.skip(4);
                        break;
                        
                    case 10: 
                        reader.skip(5);
                        break;
                        
                    case 11: 
                        {
                            reader.skip(4);
                            uint16_t str_len = reader.read<uint16_t>();
                            reader.skip(str_len);
                        }
                        break;
                        
                    case 14: 
                        {
                            uint16_t str_len = reader.read<uint16_t>();
                            reader.skip(str_len);
                            reader.skip(1);
                            reader.skip(4);
                            reader.skip(18); 
                        }
                        break;
                        
                    case 15: 
                        reader.skip(4);
                        break;
                        
                    case 16: 
                        reader.skip(1);
                        break;
                        
                    case 17: 
                        
                        break;
                        
                    case 18: 
                        reader.skip(5);
                        break;
                        
                    case 19: 
                        reader.skip(18); 
                        break;
                        
                    case 20: 
                        
                        break;
                        
                    case 21: 
                        {
                            uint16_t str_len = reader.read<uint16_t>();
                            reader.skip(str_len);
                        }
                        break;
                        
                    case 22: 
                        {
                            uint16_t str_len = reader.read<uint16_t>();
                            reader.skip(str_len);
                            reader.skip(5);
                        }
                        break;
                        
                    case 23: 
                        reader.skip(4);
                        break;
                        
                    case 24: 
                        {
                            extra_parsed = true;
                            tile.vending_data.item_id = reader.read<uint32_t>();
                            tile.vending_data.price = reader.read<int32_t>();
                            
                            
                            
                            
                        }
                        break;
                        
                    case 25: 
                        {
                            reader.skip(1);
                            uint32_t fish_count = reader.read<uint32_t>();
                            if (fish_count > 50000) {
                                spdlog::warn("FishTankPort count too large at ({}, {}): {} (clamping)",
                                    tile.x, tile.y, fish_count);
                                had_warnings = true;
                                fish_count = 50000;
                            }
                            reader.skip(fish_count * 8);
                        }
                        break;
                        
                    case 26: 
                        reader.skip(5);
                        break;
                        
                    case 27: 
                        reader.skip(4);
                        break;
                        
                    case 28: 
                        reader.skip(6);
                        break;
                        
                    case 29: 
                        reader.skip(5);
                        break;
                        
                    case 30: 
                        {
                            reader.skip(1);
                            uint16_t str_len = reader.read<uint16_t>();
                            reader.skip(str_len);
                            reader.skip(16);
                        }
                        break;
                        
                    case 32: 
                        {
                            uint32_t bolt_count = reader.read<uint32_t>();
                            if (bolt_count > 100000) {
                                spdlog::warn("SewingMachine bolt count too large at ({}, {}): {} (clamping)",
                                    tile.x, tile.y, bolt_count);
                                had_warnings = true;
                                bolt_count = 100000;
                            }
                            reader.skip(bolt_count * 4);
                        }
                        break;
                        
                    case 33: 
                        {
                            uint16_t str_len = reader.read<uint16_t>();
                            reader.skip(str_len);
                        }
                        break;
                        
                    case 34: 
                        
                        break;
                        
                    case 35: 
                        {
                            reader.skip(4);
                            uint16_t str_len = reader.read<uint16_t>();
                            reader.skip(str_len);
                        }
                        break;
                        
                    case 40: 
                        reader.skip(4);
                        break;
                        
                    case 42: 
                        
                        break;
                        
                    case 43: 
                        reader.skip(16);
                        break;
                        
                    case 44: 
                        {
                            reader.skip(5);
                            uint32_t count = reader.read<uint32_t>();
                            if (count > 100000) {
                                spdlog::warn("VipEntrance count too large at ({}, {}): {} (clamping)",
                                    tile.x, tile.y, count);
                                had_warnings = true;
                                count = 100000;
                            }
                            reader.skip(count * 4);
                        }
                        break;
                        
                    case 45: 
                        
                        break;
                        
                    case 46: 
                        {
                            uint16_t str_len = reader.read<uint16_t>();
                            reader.skip(str_len);
                            reader.skip(5);
                        }
                        break;
                        
                    case 48: 
                        {
                            uint16_t str_len = reader.read<uint16_t>();
                            reader.skip(str_len);
                            reader.skip(28);
                        }
                        break;
                        
                    case 49: 
                        reader.skip(9);
                        break;
                        
                    case 50: 
                        reader.skip(4);
                        break;
                        
                    case 51: 
                        
                        break;
                        
                    case 52: 
                        
                        break;
                        
                    case 53: 
                        reader.skip(8);
                        break;
                        
                    case 54: 
                        {
                            uint32_t item_count = reader.read<uint32_t>();
                            if (item_count > 200000) {
                                spdlog::warn("StorageBlock item count too large at ({}, {}): {} (clamping)",
                                    tile.x, tile.y, item_count);
                                had_warnings = true;
                                item_count = 200000;
                            }
                            reader.skip(item_count * 8);
                        }
                        break;
                        
                    case 55: 
                        {
                            reader.skip(4);
                            uint32_t ing_count = reader.read<uint32_t>();
                            if (ing_count > 50000) {
                                spdlog::warn("CookingOven ingredient count too large at ({}, {}): {} (clamping)",
                                    tile.x, tile.y, ing_count);
                                ing_count = 50000;
                            }
                            reader.skip(ing_count * 8);
                            reader.skip(12);
                        }
                        break;
                        
                    case 56: 
                        {
                            uint16_t str_len = reader.read<uint16_t>();
                            reader.skip(str_len);
                            reader.skip(4);
                        }
                        break;
                        
                    case 57: 
                        reader.skip(4);
                        break;
                        
                    case 58: 
                        
                        break;
                        
                    case 59: 
                        
                        break;
                        
                    case 60: 
                        reader.skip(5);
                        break;
                        
                    case 61: 
                        reader.skip(35);
                        break;
                        
                    case 62: 
                        reader.skip(14);
                        break;
                        
                    case 63: 
                        {
                            reader.skip(8); 
                            uint32_t cmd_count = reader.read<uint32_t>();
                            if (cmd_count > 50000) {
                                spdlog::warn("CyBot command count too large at ({}, {}): {} (clamping)",
                                    tile.x, tile.y, cmd_count);
                                had_warnings = true;
                                cmd_count = 50000;
                            }
                            
                            for (uint32_t i = 0; i < cmd_count; i++) {
                                reader.skip(4); 
                                reader.skip(4); 
                                reader.skip(7); 
                            }
                        }
                        break;
                        
                    case 65: 
                        reader.skip(17);
                        break;
                        
                    case 66: 
                        reader.skip(1);
                        break;
                        
                    case 67: 
                        {
                            reader.skip(4);
                            uint32_t unknown_count = reader.read<uint32_t>();
                            const size_t bytes_needed = static_cast<size_t>(unknown_count) * 4;
                            if (reader.position() + bytes_needed > size) {
                                throw std::runtime_error("ContainmentField count exceeds remaining data");
                            }
                            reader.skip(bytes_needed);
                        }
                        break;
                        
                    case 68: 
                        reader.skip(12);
                        break;
                        
                    case 72: 
                        reader.skip(12);
                        break;
                        
                    case 73: 
                        reader.skip(4);
                        break;
                        
                    case 80: 
                        reader.skip(8);
                        break;
                        
                    case 81: 
                        reader.skip(8);
                        break;
                        
                    default:
                        
                        
                        if (tile.extra_type != 0) {
                            spdlog::warn("Unknown extra_type {} at tile {} ({}, {}), fg={} - skipping extra data", 
                                (int)tile.extra_type, count, tile.x, tile.y, tile.fg);
                            had_warnings = true;
                        }
                        break;
                }
                
                
                
                
                
            }
            
            tiles.push_back(tile);
            
            } catch (const std::exception& e) {
                if (tiles_parsed_from_data > 0) {
                    --tiles_parsed_from_data;
                }
                spdlog::warn("Tile parse error at {} ({}, {}): {}. Attempting stream recovery.",
                    count, count % width, count / width, e.what());
                had_warnings = true;

                bool recovered = false;
                const size_t error_pos = reader.position();

                
                if (reader.position() + 8 <= size) {
                    try {
                        const uint16_t next_fg = reader.read<uint16_t>();
                        const uint16_t next_bg = reader.read<uint16_t>();
                        const uint16_t next_parent = reader.read<uint16_t>();
                        const uint16_t next_flags = reader.read<uint16_t>();
                        (void)next_parent;
                        (void)next_flags;

                        if (next_fg < 20000 && next_bg < 20000) {
                            reader.rewind(8);
                            recovered = true;
                            spdlog::debug("Recovered at tile {} without skipping", count);
                        } else {
                            reader.rewind(8);
                        }
                    } catch (...) {
                    }
                }

                
                if (!recovered && error_pos + 8 < size) {
                    for (size_t skip = 1; skip <= 256; ++skip) {
                        const size_t test_pos = error_pos + skip;
                        if (test_pos + 8 > size) {
                            break;
                        }

                        const uint16_t test_fg = *reinterpret_cast<const uint16_t*>(data + test_pos);
                        const uint16_t test_bg = *reinterpret_cast<const uint16_t*>(data + test_pos + 2);
                        if (test_fg < 20000 && test_bg < 20000) {
                            try {
                                const size_t current = reader.position();
                                if (test_pos > current) {
                                    reader.skip(test_pos - current);
                                }
                                recovered = true;
                                spdlog::debug("Recovered at tile {} by skipping {} bytes", count, skip);
                                break;
                            } catch (...) {
                            }
                        }
                    }
                }

                
                Tile basic_tile;
                basic_tile.x = count % width;
                basic_tile.y = count / width;
                basic_tile.fg = 0;
                basic_tile.bg = 0;
                tiles.push_back(basic_tile);

                if (!recovered) {
                    
                    tiles_filled_as_empty = tile_count - count - 1;
                    spdlog::warn("Could not recover stream at tile {}. Filling remaining {} tiles as empty.",
                        count, tiles_filled_as_empty);
                    had_warnings = true;
                    for (uint32_t i = count + 1; i < tile_count; ++i) {
                        Tile empty_tile;
                        empty_tile.x = i % width;
                        empty_tile.y = i / width;
                        empty_tile.fg = 0;
                        empty_tile.bg = 0;
                        tiles.push_back(empty_tile);
                    }
                    break;
                }

                continue;
            }
        }
        
        spdlog::info("✓ Successfully parsed {} tiles (out of {} total)!", tiles.size(), tile_count);
        spdlog::info("  - {} tiles parsed from data", tiles_parsed_from_data);
        spdlog::info("  - {} tiles filled as empty", tiles_filled_as_empty);
        
        size_t pos_after_tiles = reader.position();
        size_t remaining_bytes = size - pos_after_tiles;
        
        if (remaining_bytes > 10) {
            spdlog::info("Position after tiles: {} / {} bytes ({} remaining)", pos_after_tiles, size, remaining_bytes);
            
            
            if (remaining_bytes < 256) {
                std::ostringstream hex_dump;
                for (size_t i = 0; i < remaining_bytes; i++) {
                    hex_dump << std::hex << std::setw(2) << std::setfill('0') 
                             << (int)(data[pos_after_tiles + i]) << " ";
                }
                spdlog::info("Remaining data ({} bytes): {}", remaining_bytes, hex_dump.str());
            }
        } else if (remaining_bytes > 0) {
            spdlog::debug("Only {} bytes remaining (padding/terminator)", remaining_bytes);
        }
        
        
        
        
        dropped_items.clear();
        dropped_items_count = 0;
        last_dropped_item_uid = 0;

        if (remaining_bytes > 8) {
            try {
                size_t start_pos = reader.position();
                
                struct ScanResult { size_t offset; uint32_t count; uint32_t size; int score; };
                std::vector<ScanResult> results;

                
                for (size_t offset = 0; offset <= 128 && (start_pos + offset + 8) <= size; offset += 1) {
                    reader.seek(start_pos + offset);
                    uint32_t test_count = reader.read<uint32_t>();
                    uint32_t test_uid = reader.read<uint32_t>();
                    (void)test_uid;

                    if (test_count == 0 || test_count > 15000) continue;

                    size_t items_start = reader.position();
                    size_t bytes_left = size - items_start;

                    
                    for (uint32_t s : {16, 17, 18, 19, 20, 22, 24}) {
                        if ((size_t)test_count * s > bytes_left) continue;

                        int score = 0;
                        for (uint32_t i = 0; i < std::min(test_count, 10U); ++i) {
                            reader.seek(items_start + (i * s));
                            uint16_t id = reader.read<uint16_t>();
                            float x = reader.read<float>();
                            float y = reader.read<float>();
                            if (id > 0 && id < 22000 && x >= 0 && x < 4000 && y >= 0 && y < 4000) {
                                score++;
                            }
                        }
                        if (score >= 1) {
                            results.push_back({offset, test_count, s, score});
                        }
                    }
                }

                
                ScanResult* best = nullptr;
                for (auto& res : results) {
                    if (!best || res.score > best->score) best = &res;
                }

                if (best) {
                    reader.seek(start_pos + best->offset + 8);
                    dropped_items_count = best->count;
                    uint32_t item_size = best->size;
                    
                    spdlog::info("Smart-Scan locked: count={}, size={}, offset={}", 
                                 best->count, item_size, best->offset);

                    dropped_items.reserve(dropped_items_count);
                    for (uint32_t i = 0; i < dropped_items_count; ++i) {
                        if (reader.position() + 16 > size) break;
                        
                        size_t item_start = reader.position();
                        DroppedItem item;
                        item.id    = reader.read<uint16_t>();
                        item.x     = reader.read<float>();
                        item.y     = reader.read<float>();
                        
                        if (item_size >= 19) {
                            item.count = reader.read<uint32_t>();
                        } else {
                            item.count = reader.read<uint8_t>();
                        }
                        
                        
                        reader.seek(item_start + item_size - 4);
                        item.uid = reader.read<uint32_t>();
                        
                        if (item.id != 0) dropped_items.push_back(item);
                        reader.seek(item_start + item_size);
                    }
                } else {
                    spdlog::warn("Smart-Scan failed to find a valid items pattern");
                }
                
                if (dropped_items.size() > 0) {
                    spdlog::info("✓ Successfully parsed {} floating items", dropped_items.size());
                }
            } catch (const std::exception& e) {
                spdlog::debug("Dropped items section parse error: {}", e.what());
            }
        }
        
        
        is_valid = true;
        return true;
        
    } catch (const std::exception& e) {
        is_error = true;
        had_warnings = true;
        error_message = e.what();
        spdlog::error("✗ Parse failed: {}", e.what());
        return false;
    }
}

Tile* World::get_tile(uint32_t x, uint32_t y) {
    if (x >= width || y >= height) return nullptr;
    uint32_t index = y * width + x;
    if (index >= tiles.size()) return nullptr;
    return &tiles[index];
}

} 
