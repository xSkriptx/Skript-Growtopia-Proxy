#pragma once
#include "../extension.hpp"

namespace extension::player_state_tracker {

class IPlayerStateTrackerExtension : public IExtension {
public:
    PROVIDE_EXT_UID(0xDEADBEEF7890)
    
    virtual ~IPlayerStateTrackerExtension() = default;
    
    void free() override {}
};

} 
