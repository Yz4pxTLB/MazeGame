#pragma once
#include "Common.h"
#include "DxLib.h"
#include <vector>

class Player;

class PropRenderer {
public:
    static void DrawProps(const std::vector<Prop>& props);
    static void DrawHandModel(const Player& player, VECTOR camPos, VECTOR lookDir, VECTOR rightDir, VECTOR upDir,
        bool isLanternOn, float lanternOil, bool isMatchLit);
};