#include "DungeonMap.h"
#include <random>

void DungeonMap::GenerateFloor(int floor, unsigned int baseSeed) {
    currentFloor = floor;
    unsigned int floorSeed = baseSeed + static_cast<unsigned int>(floor * 10007);

    width = 11 + (floor - 1) * 2;
    height = 11 + (floor - 1) * 2;
    maze = MazeGenerator::Generate(width, height, floorSeed);

    visited.assign(height, std::vector<bool>(width, false));
    props.clear();

    std::mt19937 rng(floorSeed + 999);

    const int cdx[4] = { 0,  1,  0, -1 };
    const int cdy[4] = { -1, 0,  1,  0 };

    for (int y = 1; y < height - 1; ++y) {
        for (int x = 1; x < width - 1; ++x) {
            if (maze.grid[y][x] != static_cast<int>(TileType::Path)) continue;

            int flippedY = height - 1 - y;
            float centerX = x * BLOCK_SIZE + (BLOCK_SIZE / 2.0f);
            float centerZ = flippedY * BLOCK_SIZE + (BLOCK_SIZE / 2.0f);

            // 1. 天井電灯
            if ((x * 3 + y * 7) % 9 == 0) {
                Prop lamp;
                lamp.type = PropType::CeilingLamp;
                lamp.gridX = x; lamp.gridY = y;
                lamp.worldX = centerX;
                lamp.worldY = BLOCK_SIZE - 2.0f;
                lamp.worldZ = centerZ;
                lamp.hitRadius = 0.0f;
                lamp.name = "天井の白熱電灯 ";
                props.push_back(lamp);
            }
            // 2. 小机・木箱の配置
            else if (rng() % 100 < 25) {
                std::vector<int> wallDirs;
                for (int d = 0; d < 4; ++d) {
                    int nx = x + cdx[d];
                    int ny = y + cdy[d];
                    if (maze.grid[ny][nx] == static_cast<int>(TileType::Wall)) {
                        wallDirs.push_back(d);
                    }
                }

                if (!wallDirs.empty()) {
                    int chosenWallDir = wallDirs[rng() % wallDirs.size()];
                    float offX = 0.0f;
                    float offZ = 0.0f;
                    float rot = 0.0f;

                    if (chosenWallDir == 0) { offZ = 42.0f; rot = 0.0f; }
                    if (chosenWallDir == 1) { offX = 42.0f; rot = PI * 0.5f; }
                    if (chosenWallDir == 2) { offZ = -42.0f; rot = PI; }
                    if (chosenWallDir == 3) { offX = -42.0f; rot = PI * 1.5f; }

                    int propKind = rng() % 100;
                    if (propKind < 45) {
                        Prop desk;
                        desk.type = PropType::Desk;
                        desk.gridX = x; desk.gridY = y;
                        desk.worldX = centerX + offX;
                        desk.worldY = 10.0f;
                        desk.worldZ = centerZ + offZ;
                        desk.rotY = rot;
                        desk.hitRadius = 24.0f;
                        desk.name = "古い作業台 ";
                        props.push_back(desk);
                        // ※机の上に確定スポーンしていた「古びた日誌」を撤去（机を調べて入手する方式に統一）
                    }
                    else if (propKind < 80) {
                        Prop crate;
                        crate.type = PropType::Crate;
                        crate.gridX = x; crate.gridY = y;
                        crate.worldX = centerX + offX;
                        crate.worldY = 4.0f;
                        crate.worldZ = centerZ + offZ;
                        crate.rotY = (rng() % 360) * (PI / 180.0f);
                        crate.hitRadius = 16.0f;
                        crate.name = "小さな木箱 ";
                        props.push_back(crate);
                    }
                    else {
                        // 床に落ちている小瓶（稀にオイル）
                        Prop scrap;
                        scrap.type = PropType::SmallItem;
                        scrap.gridX = x; scrap.gridY = y;
                        scrap.worldX = centerX + offX * 0.9f;
                        scrap.worldY = 3.0f;
                        scrap.worldZ = centerZ + offZ * 0.9f;
                        scrap.hitRadius = 12.0f;
                        scrap.name = "燃料ボトルの小瓶 ";
                        scrap.itemId = ItemId::OilBottle;
                        props.push_back(scrap);
                    }
                }
            }
        }
    }
}

void DungeonMap::UpdateProps(float deltaTime) {
    for (auto& prop : props) {
        if (prop.type == PropType::CeilingLamp) {
            prop.flickerTimer -= deltaTime;
            if (prop.flickerTimer <= 0.0f) {
                prop.flickerTimer = 0.08f + (rand() % 40) * 0.01f;
                prop.isLightOn = (rand() % 100 < 82);
            }
        }
    }
}

void DungeonMap::RevealAround(int px, int py) {
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            int nx = px + dx;
            int ny = py + dy;
            if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                visited[ny][nx] = true;
            }
        }
    }
}

bool DungeonMap::IsVisited(int x, int y) const {
    if (x < 0 || x >= width || y < 0 || y >= height) return false;
    return visited[y][x];
}

bool DungeonMap::IsWall(int x, int y) const {
    if (x < 0 || x >= width || y < 0 || y >= height) return true;
    return maze.grid[y][x] == static_cast<int>(TileType::Wall);
}

bool DungeonMap::IsGoal(int x, int y) const {
    if (x < 0 || x >= width || y < 0 || y >= height) return false;
    return maze.grid[y][x] == static_cast<int>(TileType::Goal);
}

int DungeonMap::GetTile(int x, int y) const {
    if (x < 0 || x >= width || y < 0 || y >= height) return static_cast<int>(TileType::Wall);
    return maze.grid[y][x];
}

void DungeonMap::SetVisitedRaw(int w, int h, const std::vector<uint8_t>& data) {
    if (w != width || h != height || data.size() != static_cast<size_t>(w * h)) return;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            visited[y][x] = (data[y * width + x] != 0);
        }
    }
}

void DungeonMap::SetPropsCollected(const std::vector<uint8_t>& data) {
    for (size_t i = 0; i < props.size() && i < data.size(); ++i) {
        props[i].isCollected = (data[i] != 0);
    }
}