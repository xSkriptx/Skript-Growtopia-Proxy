#pragma once
#include "command_base.hpp"
#include "../../core/core.hpp"
#include <memory>




#pragma pack(push, 1)
struct MoriStatePacket {
    uint8_t type;
    uint8_t object_type;
    uint8_t jump_count;
    uint8_t animation_type;
    uint32_t net_id;
    int32_t target_net_id;
    uint32_t flags;
    float float_variable;
    uint32_t value;
    float vector_x;
    float vector_y;
    float vector_x2;
    float vector_y2;
    float particle_rotation;
    int32_t int_x;
    int32_t int_y;
    uint32_t extended_data_length;
};
#pragma pack(pop)
static_assert(sizeof(MoriStatePacket) == 56, "MoriStatePacket must be 56 bytes");



namespace command {
    void send_overlay(player::Player* player, const std::string& msg);
}

namespace command {

class FindPathCommand : public CommandBase {
public:
    FindPathCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    static void set_core(core::Core* core);
    static bool handle_shift_click(client::Client* client, uint32_t tile_x, uint32_t tile_y);
    static bool is_click_mode_enabled();

    
    static int run_path(client::Client* client, uint32_t target_x, uint32_t target_y, bool show_console, int delay_ms = -1);
    
private:
    static core::Core* s_core;
    static bool s_click_mode_enabled;
};

class PlayerTPCommand : public CommandBase {
public:
    PlayerTPCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    static void set_core(core::Core* core);
    
private:
    static core::Core* s_core;
};

class FlagCommand : public CommandBase {
public:
    FlagCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    static void set_core(core::Core* core);
    
private:
    static core::Core* s_core;
};

class InvisCommand : public CommandBase {
public:
    InvisCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    static void set_core(core::Core* core);
    static bool is_invis_enabled();
    
private:
    static core::Core* s_core;
    static bool s_invis_enabled;
};



class GhostCharCommand : public CommandBase {
public:
    GhostCharCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;

    static void set_core(core::Core* core);
    static bool is_enabled();

private:
    static core::Core* s_core;
    static bool s_ghost_enabled;
};

class SmCommand : public CommandBase {
public:
    SmCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    static void set_core(core::Core* core);
    static bool is_sm_enabled();
    
private:
    static core::Core* s_core;
    static bool s_sm_enabled;
};

class FreezeCommand : public CommandBase {
public:
    FreezeCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    static void set_core(core::Core* core);
    
private:
    static core::Core* s_core;
    void freeze_player(player::Player* player, uint32_t netid, const std::string& name, bool freeze_state);
};

class HitVFXCommand : public CommandBase {
public:
    HitVFXCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    static void set_core(core::Core* core);
    
private:
    static core::Core* s_core;
};

class AuctionBidCommand : public CommandBase {
public:
    AuctionBidCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    static void set_core(core::Core* core);
    
private:
    static core::Core* s_core;
};

class UbiclubCommand : public CommandBase {
public:
    UbiclubCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    static void set_core(core::Core* core);
    
private:
    static core::Core* s_core;
};

class WorldLockStorageCommand : public CommandBase {
public:
    WorldLockStorageCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    static void set_core(core::Core* core);
    
private:
    static core::Core* s_core;
};

class EventCommand : public CommandBase {
public:
    EventCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    static void set_core(core::Core* core);
    
private:
    static core::Core* s_core;
};

class MstateCommand : public CommandBase {
public:
    MstateCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    static void set_core(core::Core* core);
    static bool is_mstate_enabled();
    
private:
    static core::Core* s_core;
    static bool s_mstate_enabled;
};

class BetaCommand : public CommandBase {
public:
    BetaCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    static void set_core(core::Core* core);
    static bool is_beta_enabled();
    
private:
    static core::Core* s_core;
    static bool s_beta_enabled;
};

class InitWorldCommand : public CommandBase {
public:
    InitWorldCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    static void set_core(core::Core* core);
    
private:
    static core::Core* s_core;
};

class GemsCommand : public CommandBase {
public:
    GemsCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    static void set_core(core::Core* core);
    
private:
    static core::Core* s_core;
};

class BuxCommand : public CommandBase {
public:
    BuxCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    static void set_core(core::Core* core);
    
private:
    static core::Core* s_core;
};

class BubbleCommand : public CommandBase {
public:
    BubbleCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    static void set_core(core::Core* core);
    
private:
    static core::Core* s_core;
};

class OverlayCommand : public CommandBase {
public:
    OverlayCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    static void set_core(core::Core* core);
    
private:
    static core::Core* s_core;
};

class ZoomCommand : public CommandBase {
public:
    ZoomCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    static void set_core(core::Core* core);
    
private:
    static core::Core* s_core;
};

class RainbowCommand : public CommandBase {
public:
    RainbowCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    static void set_core(core::Core* core);
    static bool s_rainbow_enabled;
    
private:
    static core::Core* s_core;
};

class InfinityCommand : public CommandBase {
public:
    InfinityCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    static void set_core(core::Core* core);
    
private:
    static core::Core* s_core;
};

class DragonCommand : public CommandBase {
public:
    DragonCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    static void set_core(core::Core* core);
    static bool s_dragon_enabled;
    
private:
    static core::Core* s_core;
};

class RiftWingsCommand : public CommandBase {
public:
    RiftWingsCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    static void set_core(core::Core* core);
    static bool s_riftwings_enabled;
    
private:
    static core::Core* s_core;
};

class SetWeatherCommand : public CommandBase {
public:
    SetWeatherCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    static void set_core(core::Core* core);
    
private:
    static core::Core* s_core;
};

class SetPosCommand : public CommandBase {
public:
    SetPosCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    static void set_core(core::Core* core);
    
private:
    static core::Core* s_core;
    static void set_position(player::Player* player, uint32_t netid, float x, float y);
};

class ConsoleMessageCommand : public CommandBase {
public:
    ConsoleMessageCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    static void set_core(core::Core* core);
    
private:
    static core::Core* s_core;
};

class DialogCommand : public CommandBase {
public:
    DialogCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    static void set_core(core::Core* core);
    
private:
    static core::Core* s_core;
};

class TradeCommand : public CommandBase {
public:
    TradeCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    static void set_core(core::Core* core);
    
private:
    static core::Core* s_core;
};

class ActionCommand : public CommandBase {
public:
    ActionCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    static void set_core(core::Core* core);
    
private:
    static core::Core* s_core;
};

class AntiGravityCommand : public CommandBase {
public:
    AntiGravityCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;

    static void set_core(core::Core* core);

private:
    static core::Core* s_core;
};

class AntiPunchCommand : public CommandBase {
public:
    AntiPunchCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;

    static void set_core(core::Core* core);

private:
    static core::Core* s_core;
};

class TitleIconCommand : public CommandBase {
public:
    TitleIconCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    static void set_core(core::Core* core);
    
private:
    static core::Core* s_core;
};

class VisionCommand : public CommandBase {
public:
    VisionCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    static void set_core(core::Core* core);
    static bool is_vision_enabled();
    
private:
    static core::Core* s_core;
    static bool s_vision_enabled;
};


class SpawnBGLsCommand : public CommandBase {
public:
    SpawnBGLsCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;

    static void set_core(core::Core* core);

private:
    static core::Core* s_core;
};

class ChestCommand : public CommandBase {
public:
    ChestCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    
    static void set_core(core::Core* core);
    
private:
    static core::Core* s_core;
};

class RemaCommand : public CommandBase {
public:
    RemaCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;

    static void set_core(core::Core* core);

private:
    static core::Core* s_core;
};




class SkinCommand : public CommandBase {
public:
    SkinCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    static void set_core(core::Core* core);
private:
    static core::Core* s_core;
};

class DeathAnimCommand : public CommandBase {
public:
    DeathAnimCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    static void set_core(core::Core* core);
private:
    static core::Core* s_core;
};

class RespawnAnimCommand : public CommandBase {
public:
    RespawnAnimCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    static void set_core(core::Core* core);
private:
    static core::Core* s_core;
};

class ParticleCommand : public CommandBase {
public:
    ParticleCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    static void set_core(core::Core* core);
private:
    static core::Core* s_core;
};

class ItemEffectCommand : public CommandBase {
public:
    ItemEffectCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    static void set_core(core::Core* core);
private:
    static core::Core* s_core;
};

class BannerCommand : public CommandBase {
public:
    BannerCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    static void set_core(core::Core* core);
private:
    static core::Core* s_core;
};

class DisguiseCommand : public CommandBase {
public:
    DisguiseCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    static void set_core(core::Core* core);
private:
    static core::Core* s_core;
};

class PaintballCommand : public CommandBase {
public:
    PaintballCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    static void set_core(core::Core* core);
private:
    static core::Core* s_core;
};

class BroadcastCommand : public CommandBase {
public:
    BroadcastCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    static void set_core(core::Core* core);
private:
    static core::Core* s_core;
};

class RoleSkinCommand : public CommandBase {
public:
    RoleSkinCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    static void set_core(core::Core* core);
private:
    static core::Core* s_core;
};

class LevelUpCommand : public CommandBase {
public:
    LevelUpCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    static void set_core(core::Core* core);
private:
    static core::Core* s_core;
};

class RawVariantCommand : public CommandBase {
public:
    RawVariantCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    static void set_core(core::Core* core);
private:
    static core::Core* s_core;
};

class PlayersInfoCommand : public CommandBase {
public:
    PlayersInfoCommand();
    void execute(client::Client* client, const std::vector<std::string>& args) override;
    std::unique_ptr<CommandBase> clone() const override;
    static void set_core(core::Core* core);
private:
    static core::Core* s_core;
};

} 

