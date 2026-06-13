#pragma once
#include <cstdint>
#include <cstring>
#include <ctime>


namespace world {
    struct WorldInfo;
}

namespace utils {

class WorldLogger {
public:
    struct WorldEvent {
        time_t timestamp;
        uint32_t player_net_id;
        uint32_t world_id;
        char event_type[32];
        char details[256];
        char world_name[64];
    };

    static WorldLogger& get_instance();

    void log_world_entry(uint32_t player_net_id, uint32_t world_id, const char* world_name, const char* details = "");
    void log_world_exit(uint32_t player_net_id, uint32_t world_id);
    void log_world_analysis(const world::WorldInfo& world_info, uint32_t player_net_id);

    void generate_daily_report();
    void clear_old_events(time_t max_age_seconds);

    bool enabled() const { return enabled_; }
    void set_enabled(bool enabled) { enabled_ = enabled; }

private:
    WorldLogger();
    ~WorldLogger();

    void add_event(const WorldEvent& event);
    void save_to_file(const WorldEvent& event);

    static const size_t MAX_EVENTS = 1000;
    WorldEvent events_[MAX_EVENTS];
    size_t event_count_;
    bool enabled_;
};

} 