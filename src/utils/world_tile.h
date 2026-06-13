#pragma once

#include <cstdint>



namespace world {

enum TileFlag : uint16_t {
    TILEFLAG_TILEEXTRA = 1 << 0,
    TILEFLAG_LOCKED = 1 << 1,
    TILEFLAG_SEED = 1 << 4,
    TILEFLAG_FLIPPED = 1 << 5,
    TILEFLAG_OPEN = 1 << 6,
    TILEFLAG_PUBLIC = 1 << 7,
    TILEFLAG_SILENCED = 1 << 9,
    TILEFLAG_WATER = 1 << 10,
    TILEFLAG_FIRE = 1 << 12,
    TILEFLAG_RED = 1 << 13,
    TILEFLAG_BLUE = 1 << 14,
    TILEFLAG_GREEN = 1 << 15,
};


struct TileDoorExtra;
struct TileSignExtra;
struct TileLockExtra;
struct TileSeedExtra;
struct TileDiceExtra;
struct TileProviderExtra;
struct TileAchievementBlockExtra;
struct TileHeartMonitorExtra;
struct TileMannequinExtra;
struct TileGameGraveExtra;
struct TileGameGeneratorExtra;
struct TileXenoniteExtra;
struct TilePhoneBoothExtra;
struct TileSpotlightExtra;
struct TileDisplayBlockExtra;
struct TileVendingMachineExtra;
struct TileFishTankPortExtra;
struct TileForgeExtra;
struct TileGivingTreeExtra;
struct TileSewingMachineExtra;
struct TileCountryFlagExtra;
struct TileLobsterTrapExtra;
struct TilePaintingEaselExtra;
struct TileWeatherMachineExtra;
struct TileDataBedrockExtra;
struct TileShelfExtra;
struct TileVipEntranceExtra;
struct TileChallengeTimerExtra;
struct TilePortraitExtra;
struct TileGuildWeatherMachineExtra;
struct TileDnaExtractorExtra;
struct TileHowlerExtra;
struct TileStorageBlockExtra;
struct TileCookingOvenExtra;
struct TileAudioRackExtra;
struct TileAdventureBeginsExtra;
struct TileTombRobberExtra;
struct TileTrainingPortExtra;
struct TileGuildItemExtra;
struct TileKrakenBlockExtra;
struct TileFriendsEntranceExtra;

enum eExtraTileDataType : uint8_t;

class Tile {
public:
    Tile();
    ~Tile();

public:
    uint16_t Fg{}, Bg{};
    uint16_t ParentTileIndex{};

    union {
        TileFlag Value;
        struct {
            uint16_t bHasTileExtra : 1;
            uint16_t bLocked : 1;
            uint16_t bUnk1 : 1;
            uint16_t bUnk2 : 1;
            uint16_t bSeed : 1;
            uint16_t bFlipped : 1;
            uint16_t bOpen : 1;
            uint16_t bPublic : 1;
            uint16_t bUnk3 : 1;
            uint16_t bSilenced : 1;
            uint16_t bWater : 1;
            uint16_t bUnk4 : 1;
            uint16_t bFire : 1;
            uint16_t bRed : 1;
            uint16_t bBlue : 1;
            uint16_t bGreen : 1;
        };
    } Flags{};

    uint16_t LockIndex{};
    eExtraTileDataType ExtraTileDataType;

    void* ExtraTileData;

private:
    uint8_t m_extra_tile_data_buff[64]{};
};

}