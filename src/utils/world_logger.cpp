#include "world_logger.hpp"
#include "world_info.h"
#include <cstdio>
#include <cstring>
#include <ctime>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace utils {

WorldLogger& WorldLogger::get_instance() {
    static WorldLogger instance;
    return instance;
}

WorldLogger::WorldLogger() : event_count_(0), enabled_(true) {
    
#ifdef _WIN32
    _mkdir("logs");
    _mkdir("logs\\worlds");
#else
    mkdir("logs", 0777);
    mkdir("logs/worlds", 0777);
#endif
}

WorldLogger::~WorldLogger() {
    generate_daily_report();
}

void WorldLogger::log_world_entry(uint32_t player_net_id, uint32_t world_id, const char* world_name, const char* details) {
    if (!enabled_) return;

    WorldEvent event;
    event.timestamp = time(nullptr);
    event.player_net_id = player_net_id;
    event.world_id = world_id;
    strncpy(event.event_type, "WORLD_ENTER", sizeof(event.event_type));
    if (details) strncpy(event.details, details, sizeof(event.details));
    else event.details[0] = '\0';
    if (world_name) strncpy(event.world_name, world_name, sizeof(event.world_name));
    else event.world_name[0] = '\0';
    
    add_event(event);
    save_to_file(event);
}

void WorldLogger::log_world_exit(uint32_t player_net_id, uint32_t world_id) {
    if (!enabled_) return;

    WorldEvent event;
    event.timestamp = time(nullptr);
    event.player_net_id = player_net_id;
    event.world_id = world_id;
    strncpy(event.event_type, "WORLD_EXIT", sizeof(event.event_type));
    event.details[0] = '\0';
    event.world_name[0] = '\0';
    
    add_event(event);
    save_to_file(event);
}

void WorldLogger::log_world_analysis(const world::WorldInfo& world_info, uint32_t player_net_id) {
    if (!enabled_) return;

    char analysis[256];
    snprintf(analysis, sizeof(analysis), "size=%ux%u, tiles=%zu, items=%zu, weather=%u",
             world_info.Width, world_info.Height, world_info.TilesCount, 
             world_info.ItemDropsCount, world_info.CurrentWeather);

    WorldEvent event;
    event.timestamp = time(nullptr);
    event.player_net_id = player_net_id;
    event.world_id = 0;
    strncpy(event.event_type, "WORLD_ANALYSIS", sizeof(event.event_type));
    strncpy(event.details, analysis, sizeof(event.details));
    strncpy(event.world_name, world_info.Name ? world_info.Name : "Unknown", sizeof(event.world_name));
    
    add_event(event);
    save_to_file(event);
}

void WorldLogger::add_event(const WorldEvent& event) {
    if (event_count_ < MAX_EVENTS) {
        events_[event_count_++] = event;
    } else {
        
        for (size_t i = 1; i < MAX_EVENTS; i++) {
            events_[i - 1] = events_[i];
        }
        events_[MAX_EVENTS - 1] = event;
    }
}

void WorldLogger::save_to_file(const WorldEvent& event) {
    char filename[128];
    time_t now = time(nullptr);
    struct tm* tm = localtime(&now);
    strftime(filename, sizeof(filename), "logs/worlds/world_events_%Y-%m-%d.csv", tm);
    
    FILE* file = fopen(filename, "a");
    if (!file) return;
    
    char time_buf[20];
    struct tm* event_tm = localtime(&event.timestamp);
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", event_tm);
    
    fprintf(file, "%s,%u,%u,%s,%s,%s\n",
            time_buf, event.player_net_id, event.world_id,
            event.event_type, event.world_name, event.details);
    
    fclose(file);
}

void WorldLogger::generate_daily_report() {
    if (!enabled_) return;

    
}

void WorldLogger::clear_old_events(time_t max_age_seconds) {
    time_t now = time(nullptr);
    size_t write_index = 0;
    
    for (size_t i = 0; i < event_count_; i++) {
        if (now - events_[i].timestamp <= max_age_seconds) {
            events_[write_index++] = events_[i];
        }
    }
    event_count_ = write_index;
}

} 