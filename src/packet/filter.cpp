#include "filter.hpp"
#include "../core/core.hpp"
#include "../utils/text_parse.hpp"
#include <spdlog/spdlog.h>
#include <magic_enum/magic_enum.hpp>

namespace packet {
namespace filter {

PacketFilter& PacketFilter::get_instance() {
    static PacketFilter instance;
    return instance;
}

void PacketFilter::add_rule(std::shared_ptr<FilterRule> rule) {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    rules_.push_back(rule);
    spdlog::trace("Added packet filter rule: {}", rule->get_name());
}

void PacketFilter::remove_rule(const std::string& rule_name) {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    auto it = std::remove_if(rules_.begin(), rules_.end(),
        [&](const auto& rule) { return rule->get_name() == rule_name; });
    
    if (it != rules_.end()) {
        rules_.erase(it, rules_.end());
        spdlog::info("Removed packet filter rule: {}", rule_name);
    }
}

void PacketFilter::clear_rules() {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    rules_.clear();
    spdlog::trace("Cleared all packet filter rules");
}

Action PacketFilter::process_packet(packet::NetMessageType type, 
                                  ByteStream<std::uint16_t>& data, 
                                  const FilterContext& context) {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    
    
    if (type == packet::NET_MESSAGE_TRACK || 
        type == packet::NET_MESSAGE_SERVER_HELLO) {
        return Action::ALLOW;
    }
    
    Action final_action = Action::ALLOW;
    
    for (const auto& rule : rules_) {
        Action rule_action = rule->process_packet(type, data, context);
        
        
        if (rule_action == Action::BLOCK) {
            spdlog::debug("Packet blocked by rule: {} - Type: {}", rule->get_name(), static_cast<uint32_t>(type));
            return Action::BLOCK;
        }
        else if (rule_action == Action::MODIFY) {
            final_action = Action::MODIFY;
            spdlog::debug("Packet modified by rule: {}", rule->get_name());
        }
        else if (rule_action == Action::REDIRECT) {
            spdlog::debug("Packet redirected by rule: {}", rule->get_name());
            return Action::REDIRECT;
        }
    }
    
    return final_action;
}

std::vector<std::string> PacketFilter::get_active_rules() const {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    std::vector<std::string> rule_names;
    
    for (const auto& rule : rules_) {
        rule_names.push_back(rule->get_name());
    }
    
    return rule_names;
}


BlockSpecificPacketRule::BlockSpecificPacketRule(packet::NetMessageType packet_type)
    : target_type_(packet_type) {}

Action BlockSpecificPacketRule::process_packet(packet::NetMessageType type, 
                                              ByteStream<std::uint16_t>& data, 
                                              const FilterContext& context) {
    if (type == target_type_) {
        spdlog::info("Blocked packet type: {}", static_cast<uint32_t>(type));
        return Action::BLOCK;
    }
    return Action::ALLOW;
}

std::string BlockSpecificPacketRule::get_name() const {
    return std::string("Block_") + std::to_string(static_cast<uint32_t>(target_type_));
}


Action ModifyTextMessageRule::process_packet(packet::NetMessageType type, 
                                            ByteStream<std::uint16_t>& data, 
                                            const FilterContext& context) {
    if (type == packet::NET_MESSAGE_GENERIC_TEXT || type == packet::NET_MESSAGE_GAME_MESSAGE) {
        try {
            
            size_t original_pos = data.get_read_offset();
            
            
            std::string message;
            data.read(message, data.get_size() - sizeof(packet::NetMessageType) - 1);
            
            
            TextParse text_parse{message};
            if (text_parse.get("action") == "input") {
                std::string text = text_parse.get("text");
                text_parse.set("text", {"[Proxy] " + text});
                
                
                ByteStream<std::uint16_t> new_data;
                new_data.write(type);
                new_data.write(text_parse.get_raw(), false);
                
                
                data = std::move(new_data);
                
                spdlog::debug("Modified text message");
                return Action::MODIFY;
            }
            
            
            data.skip(original_pos - data.get_read_offset());
            
        } catch (const std::exception& e) {
            spdlog::warn("Failed to modify text message: {}", e.what());
        }
    }
    return Action::ALLOW;
}

std::string ModifyTextMessageRule::get_name() const {
    return "Modify_Text_Messages";
}


RateLimitRule::RateLimitRule(uint32_t max_packets_per_second)
    : max_rate_(max_packets_per_second) {}

Action RateLimitRule::process_packet(packet::NetMessageType type, 
                                    ByteStream<std::uint16_t>& data, 
                                    const FilterContext& context) {
    if (!context.source_player) {
        return Action::ALLOW;
    }
    
    
    if (type == packet::NET_MESSAGE_GAME_PACKET || 
        type == packet::NET_MESSAGE_TRACK) {
        
        return Action::ALLOW;
    }
    
    
    if (!context.incoming) {
        return Action::ALLOW;
    }
    
    uint32_t net_id = context.source_player->get_peer()->connectID;
    auto now = std::chrono::steady_clock::now();
    
    std::lock_guard<std::mutex> lock(rate_mutex_);
    
    
    auto& timestamps = packet_timestamps_[net_id];
    auto cutoff_time = now - std::chrono::seconds(2);
    
    timestamps.erase(std::remove_if(timestamps.begin(), timestamps.end(),
        [cutoff_time](const auto& ts) { 
            return ts < cutoff_time;
        }), timestamps.end());
    
    
    uint32_t effective_limit = max_rate_;
    
    
    if (timestamps.size() < 10) {
        effective_limit = max_rate_ * 2; 
    }
    
    
    if (timestamps.size() >= effective_limit) {
        spdlog::warn("Rate limit exceeded for player {}: {} packets in last 2 seconds (limit: {})", 
                    net_id, timestamps.size(), effective_limit);
        
        
        
        timestamps.push_back(now);
        return Action::ALLOW;
    }
    
    timestamps.push_back(now);
    return Action::ALLOW;
}

std::string RateLimitRule::get_name() const {
    return "Rate_Limit_" + std::to_string(max_rate_) + "_pps";
}

} 
} 
