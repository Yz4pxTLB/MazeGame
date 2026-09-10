#pragma once
#include "Common.h"
#include <vector>

struct MazeResult {
    std::vector<std::vector<int>> grid;
    int startX = 1;
    int startY = 1;
    int goalX = 1;
    int goalY = 1;
};

class MazeGenerator {
public:
    static MazeResult Generate(int width, int height, unsigned int seed);
};