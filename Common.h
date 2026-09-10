#pragma once
#include <string>

constexpr int SCREEN_WIDTH = 640;
constexpr int SCREEN_HEIGHT = 480;
constexpr float BLOCK_SIZE = 100.0f;
constexpr float PI = 3.14159265f;

enum class Direction { North = 0, East = 1, South = 2, West = 3 };
enum class TileType : int { Path = 0, Wall = 1, Start = 2, Goal = 3 };
enum class Scene { SlotSelect, GamePlay };
enum class PropType { Desk, Crate, SmallItem, CeilingLamp };

enum class ItemId {
    None,
    Lantern,        // ランタン
    OilBottle,      // 燃料ボトル
    OldDiary,       // 古びた日誌
    Match,          // マッチ
    Shotgun,        // ショットガン
    ShotgunShells,  // ショットガンの弾
    GunParts,       // 銃の予備部品
    Bandage,        // 包帯
    Hemostatic,     // 止血剤
    Painkiller      // 鎮痛剤
};

struct Prop {
    PropType type = PropType::Desk;
    int gridX = 0;
    int gridY = 0;
    float worldX = 0.0f;
    float worldY = 0.0f;
    float worldZ = 0.0f;
    float hitRadius = 8.0f;
    float rotY = 0.0f;
    float flickerTimer = 0.0f;
    bool isLightOn = true;
    bool isCollected = false;
    bool isSearched = false; // 机用: 探索済みでも消えないフラグ
    const char* name = "";
    ItemId itemId = ItemId::None;
};

struct InventoryItem {
    ItemId id = ItemId::None;
    std::string name;
    std::string desc;
    int count = 0;
    int uses = 1;
    float durability = 100.0f;
};

constexpr int HAND_SLOT_COUNT = 2;