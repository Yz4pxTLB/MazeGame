#include "SaveManager.h"
#include <fstream>

std::string SaveManager::GetFilePath(int slotIndex) const {
    return "slot" + std::to_string(slotIndex + 1) + ".dat";
}

void SaveManager::LoadAllHeaders() {
    for (int i = 0; i < SLOT_COUNT; ++i) {
        headers[i] = SaveSlotHeader{ false, 1, 0 };
        std::ifstream ifs(GetFilePath(i), std::ios::binary);
        if (ifs) {
            ifs.read(reinterpret_cast<char*>(&headers[i]), sizeof(SaveSlotHeader));
        }
    }
}

bool SaveManager::SaveSlot(int slotIndex, const GameSaveData& data) {
    if (slotIndex < 0 || slotIndex >= SLOT_COUNT) return false;

    std::ofstream ofs(GetFilePath(slotIndex), std::ios::binary);
    if (!ofs) return false;

    ofs.write(reinterpret_cast<const char*>(&data.header), sizeof(SaveSlotHeader));
    ofs.write(reinterpret_cast<const char*>(&data.posX), sizeof(float));
    ofs.write(reinterpret_cast<const char*>(&data.posY), sizeof(float));
    ofs.write(reinterpret_cast<const char*>(&data.gridX), sizeof(int));
    ofs.write(reinterpret_cast<const char*>(&data.gridY), sizeof(int));
    ofs.write(reinterpret_cast<const char*>(&data.yaw), sizeof(float));
    ofs.write(reinterpret_cast<const char*>(&data.pitch), sizeof(float));

    // 手持ち枠2つとアクティブ枠の保存
    ofs.write(reinterpret_cast<const char*>(&data.handSlot0), sizeof(int));
    ofs.write(reinterpret_cast<const char*>(&data.handSlot1), sizeof(int));
    ofs.write(reinterpret_cast<const char*>(&data.activeHandIndex), sizeof(int));

    ofs.write(reinterpret_cast<const char*>(&data.isLanternOn), sizeof(bool));
    ofs.write(reinterpret_cast<const char*>(&data.lanternOil), sizeof(float));
    ofs.write(reinterpret_cast<const char*>(&data.isMatchLit), sizeof(bool));
    ofs.write(reinterpret_cast<const char*>(&data.matchTimer), sizeof(float));
    ofs.write(reinterpret_cast<const char*>(&data.diaryCount), sizeof(int));

    int invCount = static_cast<int>(data.inventory.size());
    ofs.write(reinterpret_cast<const char*>(&invCount), sizeof(int));
    for (const auto& item : data.inventory) {
        ofs.write(reinterpret_cast<const char*>(&item), sizeof(InventorySaveData));
    }

    ofs.write(reinterpret_cast<const char*>(&data.mapWidth), sizeof(int));
    ofs.write(reinterpret_cast<const char*>(&data.mapHeight), sizeof(int));
    int mapSize = data.mapWidth * data.mapHeight;
    if (mapSize > 0 && data.visitedData.size() == static_cast<size_t>(mapSize)) {
        ofs.write(reinterpret_cast<const char*>(data.visitedData.data()), mapSize);
    }

    int propCount = static_cast<int>(data.propsCollected.size());
    ofs.write(reinterpret_cast<const char*>(&propCount), sizeof(int));
    if (propCount > 0) {
        ofs.write(reinterpret_cast<const char*>(data.propsCollected.data()), propCount);
    }

    if (ofs) {
        headers[slotIndex] = data.header;
        return true;
    }
    return false;
}

bool SaveManager::LoadSlot(int slotIndex, GameSaveData& outData) {
    if (slotIndex < 0 || slotIndex >= SLOT_COUNT) return false;

    std::ifstream ifs(GetFilePath(slotIndex), std::ios::binary);
    if (!ifs) return false;

    ifs.read(reinterpret_cast<char*>(&outData.header), sizeof(SaveSlotHeader));
    if (!outData.header.isValid) return false;

    ifs.read(reinterpret_cast<char*>(&outData.posX), sizeof(float));
    ifs.read(reinterpret_cast<char*>(&outData.posY), sizeof(float));
    ifs.read(reinterpret_cast<char*>(&outData.gridX), sizeof(int));
    ifs.read(reinterpret_cast<char*>(&outData.gridY), sizeof(int));
    ifs.read(reinterpret_cast<char*>(&outData.yaw), sizeof(float));
    ifs.read(reinterpret_cast<char*>(&outData.pitch), sizeof(float));

    // 手持ち枠2つとアクティブ枠の復元
    ifs.read(reinterpret_cast<char*>(&outData.handSlot0), sizeof(int));
    ifs.read(reinterpret_cast<char*>(&outData.handSlot1), sizeof(int));
    ifs.read(reinterpret_cast<char*>(&outData.activeHandIndex), sizeof(int));

    ifs.read(reinterpret_cast<char*>(&outData.isLanternOn), sizeof(bool));
    ifs.read(reinterpret_cast<char*>(&outData.lanternOil), sizeof(float));
    ifs.read(reinterpret_cast<char*>(&outData.isMatchLit), sizeof(bool));
    ifs.read(reinterpret_cast<char*>(&outData.matchTimer), sizeof(float));
    ifs.read(reinterpret_cast<char*>(&outData.diaryCount), sizeof(int));

    int invCount = 0;
    ifs.read(reinterpret_cast<char*>(&invCount), sizeof(int));
    outData.inventory.resize(invCount);
    for (int i = 0; i < invCount; ++i) {
        ifs.read(reinterpret_cast<char*>(&outData.inventory[i]), sizeof(InventorySaveData));
    }

    ifs.read(reinterpret_cast<char*>(&outData.mapWidth), sizeof(int));
    ifs.read(reinterpret_cast<char*>(&outData.mapHeight), sizeof(int));
    int mapSize = outData.mapWidth * outData.mapHeight;
    outData.visitedData.resize(mapSize);
    if (mapSize > 0) {
        ifs.read(reinterpret_cast<char*>(outData.visitedData.data()), mapSize);
    }

    int propCount = 0;
    ifs.read(reinterpret_cast<char*>(&propCount), sizeof(int));
    outData.propsCollected.resize(propCount);
    if (propCount > 0) {
        ifs.read(reinterpret_cast<char*>(outData.propsCollected.data()), propCount);
    }

    return bool(ifs);
}

bool SaveManager::DeleteSlot(int slotIndex) {
    if (slotIndex < 0 || slotIndex >= SLOT_COUNT) return false;
    std::remove(GetFilePath(slotIndex).c_str());
    headers[slotIndex] = SaveSlotHeader{ false, 1, 0 };
    return true;
}

const SaveSlotHeader& SaveManager::GetSlotHeader(int slotIndex) const {
    return headers[slotIndex];
}