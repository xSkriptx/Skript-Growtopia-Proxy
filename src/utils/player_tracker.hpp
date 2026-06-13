#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <mutex>
#include <vector>

namespace utils {

class PlayerTracker {
public:
    struct PlayerPosition {
        float x = 0.0f;
        float y = 0.0f;
        
        PlayerPosition() = default;
        PlayerPosition(float pos_x, float pos_y) : x(pos_x), y(pos_y) {}
        
        bool operator==(const PlayerPosition& other) const {
            return x == other.x && y == other.y;
        }
        
        bool operator!=(const PlayerPosition& other) const {
            return !(*this == other);
        }
    };

    struct PlayerInfo {
        uint32_t netID = 0;
        uint32_t userID = 0;
        std::string name;
        std::string country;
        std::string mac_address;
        std::string platform_id;
        std::string device_type;
        PlayerPosition position;
        bool is_local = false;
        
        
        PlayerInfo() = default;
        
        
        PlayerInfo(uint32_t nid, uint32_t uid, const std::string& n, const std::string& c, bool local = false)
            : netID(nid), userID(uid), name(n), country(c), is_local(local) {}
    };

    static PlayerTracker& get_instance();

    void update_player_info(uint32_t netID, uint32_t userID, const std::string& name, 
                          const std::string& country, bool is_local);
    
    void update_player_position(uint32_t netID, float x, float y);
    void update_connection_info(uint32_t netID, const std::string& mac, const std::string& country);
    void update_platform_info(uint32_t netID, const std::string& platform_id);
    
    
    void update_player_name(uint32_t netID, const std::string& name);
    
    PlayerInfo get_local_player() const;
    PlayerInfo get_player_by_netid(uint32_t netID) const;
    PlayerPosition get_player_position(uint32_t netID) const;
    bool has_local_player() const;
    
    
    std::unordered_map<uint32_t, PlayerInfo> get_all_players() const;
    static std::string get_device_from_platform_id(const std::string& platform_id);
    
    void clear();

private:
    PlayerTracker() = default;
    ~PlayerTracker() = default;

    mutable std::mutex mutex_;
    std::unordered_map<uint32_t, PlayerInfo> players_; 
    uint32_t local_player_netid_ = 0;
};

} 
