#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <mutex>

namespace utils {

class WorldPlayers {
public:
    struct PlayerInfo {
        uint32_t net_id = 0;
        std::string name;
        
        PlayerInfo() = default;
        PlayerInfo(uint32_t nid, const std::string& n) : net_id(nid), name(n) {}
    };

    static WorldPlayers& get_instance();

    
    void add_player(uint32_t net_id, const std::string& name);
    
    
    void remove_player(uint32_t net_id);
    
    
    std::unordered_map<uint32_t, PlayerInfo> get_all_players() const;
    
    
    PlayerInfo get_player_by_netid(uint32_t net_id) const;
    
    
    void clear();

private:
    WorldPlayers() = default;
    ~WorldPlayers() = default;

    mutable std::mutex mutex_;
    std::unordered_map<uint32_t, PlayerInfo> players_; 
};

} 
