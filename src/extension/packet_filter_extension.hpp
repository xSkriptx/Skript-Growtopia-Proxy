#pragma once
#include "extension.hpp"
#include "../packet/filter.hpp"

namespace extension::packet_filter {

class PacketFilterExtension final : public IExtension {
    core::Core* core_;
    
public:
    PROVIDE_EXT_UID(0x5041434B); 

    explicit PacketFilterExtension(core::Core* core) : core_{ core } {}
    ~PacketFilterExtension() override = default;

    void init() override {
        
        auto& filter = packet::filter::PacketFilter::get_instance();
        
        
        filter.clear_rules();
        
        
        
        filter.add_rule(std::make_shared<packet::filter::RateLimitRule>(300)); 
        
        
        
        
        spdlog::trace("Packet filter extension loaded - TRACK PACKETS (type 6) ARE ALLOWED");
    }

    void free() override {
        packet::filter::PacketFilter::get_instance().clear_rules();
        delete this;
    }
};

}
