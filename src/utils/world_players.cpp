#include "world_players.hpp"

namespace utils {

WorldPlayers& WorldPlayers::get_instance() {
    static WorldPlayers instance;
    return instance;
}

void WorldPlayers::add_player(uint32_t net_id, const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    players_[net_id] = PlayerInfo(net_id, name);
}

void WorldPlayers::remove_player(uint32_t net_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    players_.erase(net_id);
}

std::unordered_map<uint32_t, WorldPlayers::PlayerInfo> WorldPlayers::get_all_players() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return players_;
}

WorldPlayers::PlayerInfo WorldPlayers::get_player_by_netid(uint32_t net_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = players_.find(net_id);
    if (it != players_.end()) {
        return it->second;
    }
    return PlayerInfo();
}

void WorldPlayers::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    players_.clear();
}

} 
