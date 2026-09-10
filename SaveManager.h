#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct SaveSlotHeader {
    bool isValid = false;
    int floor = 1;
    unsigned int seed = 0;
};

struct InventorySaveData {
    int id = 0;
    int count = 0;
    int uses = 1;
    float durability = 100.0f;
};

struct GameSaveData {
    SaveSlotHeader header;

    float posX = 1.0f;
    float posY = 1.0f;
    int gridX = 1;
    int gridY = 1;
    float yaw = 1.5707963f;
    float pitch = 0.0f;

    // 手持ちスロット2枠と現在選択中の枠インデックス
    int handSlot0 = 1;
    int handSlot1 = 0;
    int activeHandIndex = 0;

    bool isLanternOn = true;
    float lanternOil = 100.0f;
    bool isMatchLit = false;
    float matchTimer = 0.0f;
    int diaryCount = 0;

    std::vector<InventorySaveData> inventory;

    int mapWidth = 0;
    int mapHeight = 0;
    std::vector<uint8_t> visitedData;
    std::vector<uint8_t> propsCollected;
};

class SaveManager {
public:
    static constexpr int SLOT_COUNT = 3;

private:
    SaveSlotHeader headers[SLOT_COUNT];
    std::string GetFilePath(int slotIndex) const;

public:
    void LoadAllHeaders();
    bool LoadSlot(int slotIndex, GameSaveData& outData);
    bool SaveSlot(int slotIndex, const GameSaveData& data);
    bool DeleteSlot(int slotIndex);
    const SaveSlotHeader& GetSlotHeader(int slotIndex) const;
};