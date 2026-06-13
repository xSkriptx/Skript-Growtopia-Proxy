#pragma once
#include <cstdint>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <spdlog/spdlog.h>

namespace utils {

struct InventoryItem {
    uint16_t id;
    uint8_t amount;
    uint8_t flags;
};

class InventoryManager {
public:
    static InventoryManager& get_instance() {
        static InventoryManager instance;
        return instance;
    }

    void parse_inventory(const std::vector<std::byte>& data) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        spdlog::info("=== PARSING INVENTORY ===");
        spdlog::info("Data size: {} bytes", data.size());
        
        if (data.size() < 7) {
            spdlog::warn("Inventory data too small: {} bytes", data.size());
            
            items_.clear();
            inventory_size_ = 0;
            return;
        }
        
        
        std::ostringstream hex;
        for (size_t i = 0; i < std::min(size_t(20), data.size()); i++) {
            hex << std::hex << std::setw(2) << std::setfill('0') << (int)data[i] << " ";
        }
        spdlog::info("First 20 bytes: {}", hex.str());
        
        size_t offset = 1; 
        
        
        inventory_size_ = *reinterpret_cast<const uint32_t*>(&data[offset]);
        offset += 4;
        
        
        uint16_t item_count = *reinterpret_cast<const uint16_t*>(&data[offset]);
        offset += 2;
        
        spdlog::info("Inventory: size={}, items={}", inventory_size_, item_count);
        
        
        items_.clear();
        
        
        last_update_ = std::chrono::steady_clock::now();
        
        
        if (item_count > 500) {
            spdlog::error("Item count {} is unreasonable! Skipping parse.", item_count);
            inventory_size_ = 0;
            return;
        }
        
        
        for (uint16_t i = 0; i < item_count; i++) {
            if (offset + 4 > data.size()) {
                spdlog::warn("Not enough data for item {} (offset={}, size={})", i, offset, data.size());
                break;
            }
            
            InventoryItem item;
            item.id = *reinterpret_cast<const uint16_t*>(&data[offset]);
            offset += 2;
            item.amount = static_cast<uint8_t>(data[offset++]);
            item.flags = static_cast<uint8_t>(data[offset++]);
            
            items_[item.id] = item;
            
            if (i < 5) {  
                spdlog::info("  Item {}: id={}, amount={}, flags={}", i, item.id, item.amount, item.flags);
            }
        }
        
        spdlog::info("✓ Parsed {} inventory items", items_.size());
    }
    
    bool has_item(uint16_t item_id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return items_.find(item_id) != items_.end();
    }
    
    uint8_t get_item_count(uint16_t item_id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = items_.find(item_id);
        return (it != items_.end()) ? it->second.amount : 0;
    }
    
    const std::unordered_map<uint16_t, InventoryItem>& get_items() const {
        return items_;
    }

    
    [[nodiscard]] std::vector<InventoryItem> get_items_snapshot() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<InventoryItem> out;
        out.reserve(items_.size());
        for (const auto& [id, item] : items_) {
            out.push_back(item);
        }
        return out;
    }
    
    uint32_t get_inventory_size() const {
        return inventory_size_;
    }
    
    
    bool is_fresh() const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_update_).count();
        return elapsed <= 5;
    }
    
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        items_.clear();
        inventory_size_ = 0;
        last_update_ = std::chrono::steady_clock::time_point{};
    }

private:
    InventoryManager() : inventory_size_(0), last_update_{} {}
    ~InventoryManager() = default;
    InventoryManager(const InventoryManager&) = delete;
    InventoryManager& operator=(const InventoryManager&) = delete;
    
    mutable std::mutex mutex_;
    uint32_t inventory_size_;
    std::unordered_map<uint16_t, InventoryItem> items_;
    std::chrono::steady_clock::time_point last_update_;
};

} 
