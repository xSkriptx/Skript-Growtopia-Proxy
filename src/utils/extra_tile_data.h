#pragma once

#include <cstdint>
#include <vector>
#include <string>



namespace world {

enum eExtraTileDataType : uint8_t {
    None = 0,
    Door = 1,
    Sign = 2,
    Lock = 3,
    Seed = 4,
    Dice = 8,
    Provider = 9,
    AchievementBlock = 10,
    HeartMonitor = 11,
    Mannequin = 14,
    GameGrave = 16,
    GameGenerator = 17,
    Xenonite = 18,
    PhoneBooth = 19,
    Spotlight = 22,
    DisplayBlock = 23,
    VendingMachine = 24,
    FishTankPort = 25,
    Forge = 27,
    GivingTree = 28,
    SewingMachine = 32,
    CountryFlag = 33,
    LobsterTrap = 34,
    PaintingEasel = 35,
    WeatherMachine = 40,
    DataBedrock = 42,
    Shelf = 43,
    VipEntrance = 44,
    ChallengeTimer = 45,
    Portrait = 48,
    GuildWeatherMachine = 49,
    DnaExtractor = 51,
    Howler = 52,
    StorageBlock = 54,
    CookingOven = 55,
    AudioRack = 56,
    AdventureBegin = 58,
    TombRobber = 59,
    TrainingPort = 61,
    GuildItem = 65,
    KrakenGalaticBlock = 80,
    FriendsEntrance = 81,
};


struct TileDoorExtra {
    ~TileDoorExtra(){};
    const char* Label;  
    uint8_t Unk1;
};

struct TileSignExtra {
    const char* Label;  
};

struct TileLockExtra {
    uint8_t Flag;
    uint8_t MinimumLevel;
    uint32_t OwnerId;
    uint8_t UnkArr[7];
    uint32_t* AccessList;  
    size_t AccessListSize;
};

struct TileSeedExtra {
    uint32_t TimePassed;
    uint8_t FruitCount;
};

struct TileDiceExtra {
    uint8_t Pattern;
};

struct TileProviderExtra {
    uint32_t TimePassed;
};

struct TileAchievementBlockExtra {
    uint32_t UserId;
    uint8_t Achievement;
};

struct TileHeartMonitorExtra {
    const char* Name;  
    uint32_t UserId;
};

struct TileMannequinExtra {
    const char* Label;  
    uint8_t Unk1;
    uint16_t Unk2;
    uint16_t Unk3;
    uint16_t Hat;
    uint16_t Shirt;
    uint16_t Pants;
    uint16_t Boots;
    uint16_t Face;
    uint16_t Hand;
    uint16_t Back;
    uint16_t Hair;
    uint16_t Neck;
};

struct TileGameGraveExtra {
    uint8_t Team;
};

struct TileGameGeneratorExtra {};

struct TileXenoniteExtra {
    uint8_t Unk1[5];
};

struct TilePhoneBoothExtra {
    uint16_t Hat;
    uint16_t Shirt;
    uint16_t Pants;
    uint16_t Shoes;
    uint16_t Face;
    uint16_t Hand;
    uint16_t Back;
    uint16_t Hair;
    uint16_t Neck;
};

struct TileSpotlightExtra {};

struct TileDisplayBlockExtra {
    uint32_t ItemId;
};

struct TileVendingMachineExtra {
    uint32_t ItemId;
    int32_t Price;
};

struct TileFishTankPortExtra {
    uint8_t Flag;
    struct FishInfo {
        uint32_t FishItemId, Lbs;
    };
    FishInfo* Fishes;  
    size_t FishesSize;
};

struct TileForgeExtra {
    uint32_t Temperature;
};

struct TileGivingTreeExtra {
    uint8_t Harvested;
    uint8_t DecorationPercent;
    uint32_t Unk1;
};

struct TileSewingMachineExtra {
    uint32_t* BoltIdList;  
    size_t BoltIdListSize;
};

struct TileCountryFlagExtra {
    const char* Country;  
};

struct TileLobsterTrapExtra {};

struct TilePaintingEaselExtra {
    uint32_t ItemId;
    const char* Label;  
};

struct TileWeatherMachineExtra {
    uint32_t Flag;
};

struct TileDataBedrockExtra {
    uint8_t Unk1[21];
};

struct TileShelfExtra {
    uint32_t TopLeft, TopRight;
    uint32_t BottomLeft, BottomRight;
};

struct TileVipEntranceExtra {
    uint8_t Unk1;
    uint32_t OwnerId;
    uint32_t* AllowedIdList;  
    size_t AllowedIdListSize;
};

struct TileChallengeTimerExtra {};

struct TilePortraitExtra {
    const char* Label;  
    uint32_t Unk1, Unk2, Unk3, Unk4;
    uint32_t Face, Hat, Hair;
    uint16_t Unk5, Unk6;
};

struct TileGuildWeatherMachineExtra {
    uint32_t Unk1;
    uint32_t Gravity;
    uint8_t Flag;
};

struct TileDnaExtractorExtra {};

struct TileHowlerExtra {};

struct TileStorageBlockExtra {
    struct ItemInfo {
        uint32_t ItemId, ItemAmount;
    };
    ItemInfo* Items;  
    size_t ItemsSize;
};

struct TileCookingOvenExtra {};

struct TileAudioRackExtra {
    const char* Note;  
    uint32_t Volume;
};

struct TileAdventureBeginsExtra {};

struct TileTombRobberExtra {};

struct TileTrainingPortExtra {
    uint32_t FishLb;
    uint16_t FishStatus;
    uint32_t FishId;
    uint32_t FishTotalExp;
    uint8_t Unk1[8];
    uint32_t FishLevel;
    uint32_t Unk2;
    uint8_t Unk3[5];
};

struct TileGuildItemExtra {
    uint8_t Unk1[17];
};

struct TileKrakenBlockExtra {
    uint8_t PatternIndex;
    uint32_t Unk1;
    uint8_t R, G, B;
};

struct TileFriendsEntranceExtra {
    uint32_t OwnerUserId;
    uint16_t Unk1;
    uint32_t* AllowedUserList;  
    size_t AllowedUserListSize;
};

}