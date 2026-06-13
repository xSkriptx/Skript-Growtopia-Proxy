#include "player_tracker.hpp"
#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <algorithm>
#include <cctype>

namespace utils {

PlayerTracker& PlayerTracker::get_instance() {
    static PlayerTracker instance;
    return instance;
}

void PlayerTracker::update_player_info(uint32_t netID, uint32_t userID, const std::string& name, 
                                     const std::string& country, bool is_local) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    PlayerInfo info{};
    auto it = players_.find(netID);
    if (it != players_.end()) {
        info = it->second; 
    }

    info.netID = netID;
    if (userID != 0) {
        info.userID = userID;
    }
    if (!name.empty()) {
        info.name = name;
    }
    if (!country.empty()) {
        info.country = country;
    }
    info.is_local = is_local;
    
    players_[netID] = info;
    
    if (is_local) {
        local_player_netid_ = netID;
        spdlog::info("Local player tracked - NetID: {}, UserID: {}, Name: {}, Country: {}", 
                    netID, userID, name, country);
    } else {
        spdlog::debug("Player tracked - NetID: {}, UserID: {}, Name: {}", 
                     netID, userID, name);
    }
}

void PlayerTracker::update_player_position(uint32_t netID, float x, float y) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = players_.find(netID);
    if (it != players_.end()) {
        PlayerPosition new_position(x, y);
        
        
        if (it->second.position != new_position) {
            it->second.position = new_position;
            spdlog::debug("Player {} position updated: X={}, Y={}", netID, x, y);
        }
    } else {
        
        PlayerInfo info;
        info.netID = netID;
        info.name = fmt::format("Player_{}", netID); 
        info.position = PlayerPosition(x, y);
        players_[netID] = info;
        spdlog::debug("Auto-tracked player {} at position X={}, Y={}", netID, x, y);
    }
}

void PlayerTracker::update_connection_info(uint32_t netID, const std::string& mac, const std::string& country) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = players_.find(netID);
    if (it != players_.end()) {
        it->second.mac_address = mac;
        if (!country.empty()) {
            it->second.country = country;
        }
        spdlog::debug("Updated connection info for netID {}: MAC={}, Country={}", 
                     netID, mac, country);
    } else {
        spdlog::warn("Cannot update connection info: netID {} not found in tracker", netID);
    }
}

void PlayerTracker::update_platform_info(uint32_t netID, const std::string& platform_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = players_.find(netID);
    if (it != players_.end()) {
        it->second.platform_id = platform_id;
        it->second.device_type = get_device_from_platform_id(platform_id);
        spdlog::debug("Updated platform info for netID {}: {} -> {}",
                     netID, platform_id, it->second.device_type);
    } else {
        PlayerInfo info{};
        info.netID = netID;
        info.platform_id = platform_id;
        info.device_type = get_device_from_platform_id(platform_id);
        players_[netID] = info;
        spdlog::debug("Auto-tracked player {} with platform {} ({})",
                     netID, platform_id, info.device_type);
    }
}

PlayerTracker::PlayerInfo PlayerTracker::get_local_player() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (local_player_netid_ == 0) {
        return PlayerInfo{};
    }
    
    auto it = players_.find(local_player_netid_);
    if (it != players_.end()) {
        return it->second;
    }
    
    return PlayerInfo{};
}

PlayerTracker::PlayerInfo PlayerTracker::get_player_by_netid(uint32_t netID) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = players_.find(netID);
    if (it != players_.end()) {
        return it->second;
    }
    
    return PlayerInfo{};
}

PlayerTracker::PlayerPosition PlayerTracker::get_player_position(uint32_t netID) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = players_.find(netID);
    if (it != players_.end()) {
        return it->second.position;
    }
    
    return PlayerPosition{};
}

bool PlayerTracker::has_local_player() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return local_player_netid_ != 0;
}

void PlayerTracker::update_player_name(uint32_t netID, const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = players_.find(netID);
    if (it != players_.end()) {
        it->second.name = name;
        spdlog::debug("Updated player {} name to '{}'", netID, name);
    } else {
        
        PlayerInfo info;
        info.netID = netID;
        info.name = name;
        players_[netID] = info;
        spdlog::debug("Auto-tracked player {} with name '{}'", netID, name);
    }
}

std::unordered_map<uint32_t, PlayerTracker::PlayerInfo> PlayerTracker::get_all_players() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return players_;
}

std::string PlayerTracker::get_device_from_platform_id(const std::string& platform_id) {
    std::string normalized = platform_id;
    normalized.erase(std::remove_if(normalized.begin(), normalized.end(), [](unsigned char c) {
        return std::isspace(c);
    }), normalized.end());

    if (normalized == "0,1,1") {
        return "Windows";
    }
    if (normalized == "1,0,0" || normalized == "1,1,0") {
        return "Android";
    }
    if (normalized == "2,0,0" || normalized == "2,1,0") {
        return "iOS";
    }
    return "Unknown";
}

void PlayerTracker::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    players_.clear();
    local_player_netid_ = 0;
}

} 
