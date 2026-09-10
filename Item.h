#pragma once
#include "Common.h"
#include <vector>
#include <string>

class Player;

class ItemManager {
public:
    static InventoryItem CreateItem(ItemId id, int extraVal = 0);
    static InventoryItem CreateDiaryItem(int diaryOrder); // 入手順に応じた日誌生成

    static InventoryItem RollCrateDrop(std::vector<ItemId>& droppedHistory, unsigned int seed);
    static ItemId RollDeskItem(unsigned int seed);

    static bool UseItem(InventoryItem& item, Player& player, float& lanternOil, std::string& outMsg);
};