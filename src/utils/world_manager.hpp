#pragma once
#include "world_info.h"
#include "world_tile.h"
#include "world_parser_v2.h"
#include <mutex>
#include <vector>
#include <cstring>
#include <spdlog/spdlog.h>

namespace utils {

class WorldManager {
public:
    
    struct TileData {
        uint16_t Fg;
        uint16_t Bg;
    };

    static WorldManager& get_instance() {
        static WorldManager instance;
        return instance;
    }

    
    void set_current_world_v2(const world_v2::World& world) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        
        tiles_.clear();
        items_.clear();
        has_world_ = false;
        
        
        width_ = world.width;
        height_ = world.height;
        world_name_ = world.name;  
        world_v2_copy_ = world;    
        
        spdlog::info("WorldManager: Storing V2 world data - '{}' {}x{}, {} tiles", 
                     world.name, world.width, world.height, world.tiles.size());
        
        
        if (!world.tiles.empty()) {
            tiles_.reserve(world.tiles.size());
            for (const auto& tile : world.tiles) {
                TileData td;
                td.Fg = tile.fg;
                td.Bg = tile.bg;
                tiles_.push_back(td);
            }
            spdlog::info("WorldManager: Stored {} tiles", tiles_.size());
            has_world_ = true;
        } else {
            spdlog::warn("WorldManager: No tiles in V2 world data!");
        }
        
        
        if (!world.dropped_items.empty()) {
            items_.reserve(world.dropped_items.size());
            for (const auto& item : world.dropped_items) {
                world::DroppedItemInfo di;
                di.ItemId = item.id;
                di.X = item.x;
                di.Y = item.y;
                di.Amount = item.count;  
                di.Flag = item.flags;    
                di.Uid = item.uid;
                items_.push_back(di);
            }
            spdlog::info("WorldManager: Stored {} dropped items", items_.size());
        } else {
            spdlog::info("WorldManager: No dropped items in V2 world");
        }
    }

    
    void set_current_world(const world::WorldInfo& world_info) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        
        tiles_.clear();
        items_.clear();
        has_world_ = false;  
        
        
        width_ = world_info.Width;
        height_ = world_info.Height;
        
        spdlog::info("WorldManager: Storing world data - {}x{}, {} tiles, {} items", 
                     world_info.Width, world_info.Height, 
                     world_info.TilesCount, world_info.ItemDropsCount);
        
        
        if (world_info.Tiles && world_info.TilesCount > 0) {
            tiles_.reserve(world_info.TilesCount);
            for (size_t i = 0; i < world_info.TilesCount; ++i) {
                TileData tile;
                tile.Fg = world_info.Tiles[i].Fg;
                tile.Bg = world_info.Tiles[i].Bg;
                tiles_.push_back(tile);
            }
            spdlog::info("WorldManager: Stored {} tiles", tiles_.size());
            has_world_ = true;  
        } else {
            spdlog::warn("WorldManager: No tiles in world data!");
        }
        
        
        if (world_info.ItemDrops && world_info.ItemDropsCount > 0) {
            items_.reserve(world_info.ItemDropsCount);
            for (size_t i = 0; i < world_info.ItemDropsCount; ++i) {
                items_.push_back(world_info.ItemDrops[i]);
            }
            spdlog::info("WorldManager: Stored {} dropped items", items_.size());
        } else {
            spdlog::info("WorldManager: No dropped items in world");
        }
    }

    
    bool has_world() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return has_world_;
    }

    
    std::string get_world_name() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return world_name_;
    }

    
    uint32_t get_world_width() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return width_;
    }

    uint32_t get_world_height() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return height_;
    }

    world_v2::World get_world_v2() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return world_v2_copy_;
    }

    
    const std::vector<TileData>& get_tiles() const { 
        return tiles_; 
    }
    
    
    const std::vector<world::DroppedItemInfo>& get_items() const { 
        return items_; 
    }
    
    
    const std::vector<world::DroppedItemInfo>& get_live_objects() const {
        return live_objects_;
    }
    
    
    void clear_live_objects() {
        std::lock_guard<std::mutex> lock(mutex_);
        live_objects_.clear();
        spdlog::info("WorldManager: Cleared live objects");
    }
    
    
    void add_live_object(const world::DroppedItemInfo& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        live_objects_.push_back(item);
        spdlog::info("WorldManager: Added live object {} x{} at ({:.1f}, {:.1f}) [total: {}]", 
                    item.ItemId, item.Amount, item.X, item.Y, live_objects_.size());
    }
    
    
    void remove_live_object(uint32_t uid) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = std::remove_if(live_objects_.begin(), live_objects_.end(),
            [uid](const world::DroppedItemInfo& item) {
                return item.Uid == uid;
            });
        
        if (it != live_objects_.end()) {
            live_objects_.erase(it, live_objects_.end());
            spdlog::info("WorldManager: Removed live object UID {} [total: {}]", uid, live_objects_.size());
        }
    }
    
    
    void add_dropped_item(const world::DroppedItemInfo& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        items_.push_back(item);
        spdlog::debug("WorldManager: Added dropped item {} (total: {})", item.ItemId, items_.size());
    }
    
    
    void remove_dropped_item(uint16_t item_id, float x, float y) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = std::remove_if(items_.begin(), items_.end(),
            [item_id, x, y](const world::DroppedItemInfo& item) {
                return item.ItemId == item_id && 
                       std::abs(item.X - x) < 1.0f && 
                       std::abs(item.Y - y) < 1.0f;
            });
        
        if (it != items_.end()) {
            items_.erase(it, items_.end());
            spdlog::debug("WorldManager: Removed dropped item {} (total: {})", item_id, items_.size());
        }
    }
    
    
    void remove_dropped_item_by_uid(uint32_t uid) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = std::remove_if(items_.begin(), items_.end(),
            [uid](const world::DroppedItemInfo& item) {
                return item.Uid == uid;
            });
        
        if (it != items_.end()) {
            items_.erase(it, items_.end());
            spdlog::debug("WorldManager: Removed dropped item by UID {} (total: {})", uid, items_.size());
        }
    }

    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        has_world_ = false;
        tiles_.clear();
        items_.clear();
        live_objects_.clear();
        world_v2_copy_.reset();
        spdlog::info("WorldManager: Cleared all data");
    }

private:
    WorldManager() : has_world_(false), width_(0), height_(0) {}
    ~WorldManager() = default;
    WorldManager(const WorldManager&) = delete;
    WorldManager& operator=(const WorldManager&) = delete;

    mutable std::mutex mutex_;
    bool has_world_;
    uint32_t width_, height_;
    std::string world_name_;
    std::vector<TileData> tiles_;
    std::vector<world::DroppedItemInfo> items_;      
    std::vector<world::DroppedItemInfo> live_objects_; 
    world_v2::World world_v2_copy_;
};

} 
