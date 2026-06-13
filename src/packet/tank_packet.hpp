#pragma once
#include <cstdint>

namespace packet {


#pragma pack(push, 1)
struct TankUpdatePacket {
    
    uint8_t type;              
    uint8_t object_type;       
    uint8_t jump_count;        
    uint8_t animation_type;    

    
    int32_t net_id;            
    int32_t target_net_id;     
    uint32_t flags;            
    float float_var;           
    uint32_t int_data;         
    float vec_x;               
    float vec_y;               
    float vec_x2;              
    float vec_y2;              
    float particle_time;       
    int32_t int_x;             
    int32_t int_y;             
    uint32_t extra_data_size;  
     
    
    bool is_item_drop() const { return net_id == -1; }      
    bool is_item_update() const { return net_id == -4; }    
    bool is_item_collect() const { return net_id > 0; }     
};
#pragma pack(pop)

static_assert(sizeof(TankUpdatePacket) == 56, "TankUpdatePacket must be exactly 56 bytes");

} 
