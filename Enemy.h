#pragma once
#include "Common.h"
#include "DxLib.h"
#include <vector>

class DungeonMap;
class Player;

enum class SlimeState {
    Wandering, // ランダム徘徊
    Chasing    // 追跡
};

struct SlimeEnemy {
    int gridX = 0;
    int gridY = 0;
    float worldX = 0.0f;
    float worldZ = 0.0f;

    // 移動補間用
    float startX = 0.0f, startZ = 0.0f;
    float targetX = 0.0f, targetZ = 0.0f;
    float moveTimer = 0.0f;
    float moveDuration = 0.35f;
    bool isStepMoving = false;

    SlimeState state = SlimeState::Wandering;
    float actionCooldown = 0.0f; // 次の行動までの待機タイマー

    int hp = 2;
    bool isAlive = true;
    float wobblePhase = 0.0f;
    float attackCooldown = 0.0f;
};

class EnemyManager {
private:
    std::vector<SlimeEnemy> enemies;

public:
    void GenerateEnemies(const DungeonMap& map, unsigned int seed);
    void Update(float deltaTime, const DungeonMap& map, Player& player, std::string& outMsg);
    bool TryShootAtEnemy(VECTOR camPos, VECTOR lookDir, float maxDist);
    void Draw3D(const DungeonMap& map);
    const std::vector<SlimeEnemy>& GetEnemies() const { return enemies; }
};