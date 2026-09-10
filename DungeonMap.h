#pragma once
#include "Common.h"
#include "MazeGenerator.h"
#include <vector>

class DungeonMap {
private:
    int currentFloor = 1;
    int width = 11;
    int height = 11;
    MazeResult maze;
    std::vector<std::vector<bool>> visited;
    std::vector<Prop> props; // フロア上の小物リスト

public:
    void GenerateFloor(int floor, unsigned int baseSeed);
    void UpdateProps(float deltaTime);
    void RevealAround(int px, int py);

    // 踏破マップ・プロップ取得状態の復元用
    void SetVisitedRaw(int w, int h, const std::vector<uint8_t>& data);
    void SetPropsCollected(const std::vector<uint8_t>& data);

    bool IsVisited(int x, int y) const;
    bool IsWall(int x, int y) const;
    bool IsGoal(int x, int y) const;
    int GetTile(int x, int y) const;
    int GetWidth() const { return width; }
    int GetHeight() const { return height; }
    int GetCurrentFloor() const { return currentFloor; }
    int GetStartX() const { return maze.startX; }
    int GetStartY() const { return maze.startY; }
    std::vector<Prop>& GetProps() { return props; }
};