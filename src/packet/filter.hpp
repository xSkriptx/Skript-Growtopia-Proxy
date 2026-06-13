#pragma once
#include <functional>
#include <vector>
#include <memory>
#include <mutex>
#include <map>
#include <chrono>
#include "packet_types.hpp"
#include "../utils/byte_stream.hpp"
#include "../player/player.hpp"

namespace packet {
namespace filter {

enum class Action {
    ALLOW,      
    BLOCK,      
    MODIFY,     
    REDIRECT    
};

struct FilterContext {
    player::Player* source_player;
    player::Player* target_player;
    bool incoming; 
    std::chrono::steady_clock::time_point timestamp;
};

class FilterRule {
public:
    virtual ~FilterRule() = default;
    virtual Action process_packet(packet::NetMessageType type, 
                                 ByteStream<std::uint16_t>& data, 
                                 const FilterContext& context) = 0;
    virtual std::string get_name() const = 0;
};

class PacketFilter {
public:
    static PacketFilter& get_instance();
    
    void add_rule(std::shared_ptr<FilterRule> rule);
    void remove_rule(const std::string& rule_name);
    void clear_rules();
    
    Action process_packet(packet::NetMessageType type, 
                         ByteStream<std::uint16_t>& data, 
                         const FilterContext& context);
    
    std::vector<std::string> get_active_rules() const;

private:
    PacketFilter() = default;
    ~PacketFilter() = default;
    
    mutable std::mutex rules_mutex_;
    std::vector<std::shared_ptr<FilterRule>> rules_;
};


class BlockSpecificPacketRule : public FilterRule {
public:
    BlockSpecificPacketRule(packet::NetMessageType packet_type);
    
    Action process_packet(packet::NetMessageType type, 
                         ByteStream<std::uint16_t>& data, 
                         const FilterContext& context) override;
    
    std::string get_name() const override;

private:
    packet::NetMessageType target_type_;
};

class ModifyTextMessageRule : public FilterRule {
public:
    Action process_packet(packet::NetMessageType type, 
                         ByteStream<std::uint16_t>& data, 
                         const FilterContext& context) override;
    
    std::string get_name() const override;
};

class RateLimitRule : public FilterRule {
public:
    RateLimitRule(uint32_t max_packets_per_second);
    
    Action process_packet(packet::NetMessageType type, 
                         ByteStream<std::uint16_t>& data, 
                         const FilterContext& context) override;
    
    std::string get_name() const override;

private:
    uint32_t max_rate_;
    mutable std::mutex rate_mutex_;
    std::map<uint32_t, std::vector<std::chrono::steady_clock::time_point>> packet_timestamps_;
};

} 
} 