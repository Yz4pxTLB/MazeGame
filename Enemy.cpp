#include "Enemy.h"
#include "DungeonMap.h"
#include "Player.h"
#include <cmath>
#include <random>

void EnemyManager::GenerateEnemies(const DungeonMap& map, unsigned int seed) {
    enemies.clear();
    std::mt19937 rng(seed + 8888);

    int enemyCount = 1 + (map.GetCurrentFloor() > 2 ? 1 : 0);
    int attempts = 0;

    while (static_cast<int>(enemies.size()) < enemyCount && attempts < 200) {
        attempts++;
        int rx = 2 + (rng() % (map.GetWidth() - 4));
        int ry = 2 + (rng() % (map.GetHeight() - 4));

        if (!map.IsWall(rx, ry) && !map.IsGoal(rx, ry)) {
            if (std::abs(rx - map.GetStartX()) + std::abs(ry - map.GetStartY()) > 4) {
                SlimeEnemy slime;
                slime.gridX = rx;
                slime.gridY = ry;
                int flippedY = map.GetHeight() - 1 - ry;
                slime.worldX = rx * BLOCK_SIZE + (BLOCK_SIZE / 2.0f);
                slime.worldZ = flippedY * BLOCK_SIZE + (BLOCK_SIZE / 2.0f);
                slime.startX = slime.worldX; slime.startZ = slime.worldZ;
                slime.targetX = slime.worldX; slime.targetZ = slime.worldZ;
                slime.hp = 2;
                slime.isAlive = true;
                slime.state = SlimeState::Wandering;
                slime.actionCooldown = 2.5f + (rng() % 10) * 0.1f;
                enemies.push_back(slime);
            }
        }
    }
}

void EnemyManager::Update(float deltaTime, const DungeonMap& map, Player& player, std::string& outMsg) {
    const int cdx[4] = { 0, 1, 0, -1 };
    const int cdy[4] = { -1, 0, 1, 0 };

    for (auto& slime : enemies) {
        if (!slime.isAlive) continue;

        slime.wobblePhase += deltaTime * 2.2f;
        if (slime.attackCooldown > 0.0f) slime.attackCooldown -= deltaTime;

        // 1. 移動アニメーション補間
        if (slime.isStepMoving) {
            slime.moveTimer += deltaTime;
            float t = slime.moveTimer / slime.moveDuration;
            if (t >= 1.0f) {
                slime.worldX = slime.targetX;
                slime.worldZ = slime.targetZ;
                slime.isStepMoving = false;
            }
            else {
                slime.worldX = slime.startX + (slime.targetX - slime.startX) * t;
                slime.worldZ = slime.startZ + (slime.targetZ - slime.startZ) * t;
            }
        }

        // 2. 距離計算（マンハッタン距離）
        int manhattanDist = std::abs(slime.gridX - player.gridX) + std::abs(slime.gridY - player.gridY);

        bool canSeePlayer = false;
        if (manhattanDist <= 3) {
            bool blocked = false;
            int steps = 8;
            float ex = static_cast<float>(slime.gridX);
            float ey = static_cast<float>(slime.gridY);
            float px = static_cast<float>(player.gridX);
            float py = static_cast<float>(player.gridY);

            for (int s = 1; s < steps; ++s) {
                int cx = static_cast<int>(ex + (px - ex) * (static_cast<float>(s) / steps));
                int cy = static_cast<int>(ey + (py - ey) * (static_cast<float>(s) / steps));
                if (map.IsWall(cx, cy)) {
                    blocked = true;
                    break;
                }
            }
            if (!blocked) canSeePlayer = true;
        }

        if (slime.state == SlimeState::Wandering) {
            if (canSeePlayer) {
                slime.state = SlimeState::Chasing;
                outMsg = "スライムがこちらの気配に気づいた！ ";
            }
        }
        else if (slime.state == SlimeState::Chasing) {
            if (manhattanDist >= 4) {
                slime.state = SlimeState::Wandering;
                outMsg = "スライムは標的を見失ったようだ ";
            }
        }

        // 3. 移動AI（速度をさらに半減）
        slime.actionCooldown -= deltaTime;
        if (slime.actionCooldown <= 0.0f && !slime.isStepMoving) {
            int nextGx = slime.gridX;
            int nextGy = slime.gridY;

            if (slime.state == SlimeState::Chasing) {
                // 追跡時：1.2秒サイクル、0.9秒かけて1マス移動（非常に鈍重）
                slime.actionCooldown = 1.20f;
                slime.moveDuration = 0.90f;

                int diffX = player.gridX - slime.gridX;
                int diffY = player.gridY - slime.gridY;

                int tryDirs[2] = { -1, -1 };
                if (std::abs(diffX) > std::abs(diffY)) {
                    tryDirs[0] = (diffX > 0) ? 1 : 3;
                    tryDirs[1] = (diffY > 0) ? 2 : 0;
                }
                else {
                    tryDirs[0] = (diffY > 0) ? 2 : 0;
                    tryDirs[1] = (diffX > 0) ? 1 : 3;
                }

                for (int d : tryDirs) {
                    if (d == -1) continue;
                    int tx = slime.gridX + cdx[d];
                    int ty = slime.gridY + cdy[d];
                    if (!map.IsWall(tx, ty)) {
                        nextGx = tx;
                        nextGy = ty;
                        break;
                    }
                }
            }
            else {
                // 徘徊時：3.0秒〜3.8秒に1回
                slime.actionCooldown = 3.0f + (rand() % 9) * 0.1f;
                slime.moveDuration = 1.00f;

                std::vector<int> validDirs;
                for (int d = 0; d < 4; ++d) {
                    int tx = slime.gridX + cdx[d];
                    int ty = slime.gridY + cdy[d];
                    if (!map.IsWall(tx, ty) && !map.IsGoal(tx, ty)) {
                        validDirs.push_back(d);
                    }
                }
                if (!validDirs.empty()) {
                    int chosen = validDirs[rand() % validDirs.size()];
                    nextGx = slime.gridX + cdx[chosen];
                    nextGy = slime.gridY + cdy[chosen];
                }
            }

            if (nextGx != slime.gridX || nextGy != slime.gridY) {
                slime.gridX = nextGx;
                slime.gridY = nextGy;
                slime.startX = slime.worldX;
                slime.startZ = slime.worldZ;
                int flippedY = map.GetHeight() - 1 - nextGy;
                slime.targetX = nextGx * BLOCK_SIZE + (BLOCK_SIZE / 2.0f);
                slime.targetZ = flippedY * BLOCK_SIZE + (BLOCK_SIZE / 2.0f);
                slime.moveTimer = 0.0f;
                slime.isStepMoving = true;
            }
        }

        // 4. 接触ダメージ
        if (slime.gridX == player.gridX && slime.gridY == player.gridY && slime.attackCooldown <= 0.0f) {
            player.TakeDamage(25);
            slime.attackCooldown = 1.6f;
            outMsg = "スライムの強酸を浴びた！ (-25 HP) ";
        }
    }
}

bool EnemyManager::TryShootAtEnemy(VECTOR camPos, VECTOR lookDir, float maxDist) {
    for (auto& slime : enemies) {
        if (!slime.isAlive) continue;

        VECTOR toEnemy = VGet(slime.worldX - camPos.x, 15.0f - camPos.y, slime.worldZ - camPos.z);
        float t = toEnemy.x * lookDir.x + toEnemy.y * lookDir.y + toEnemy.z * lookDir.z;

        if (t > 10.0f && t < maxDist) {
            VECTOR closest = VGet(camPos.x + lookDir.x * t, camPos.y + lookDir.y * t, camPos.z + lookDir.z * t);
            float diffX = slime.worldX - closest.x;
            float diffZ = slime.worldZ - closest.z;
            float d = std::sqrt(diffX * diffX + diffZ * diffZ);

            if (d < 24.0f) {
                slime.hp -= 2;
                if (slime.hp <= 0) {
                    slime.isAlive = false;
                }
                return true;
            }
        }
    }
    return false;
}

void EnemyManager::Draw3D(const DungeonMap& map) {
    for (const auto& slime : enemies) {
        if (!slime.isAlive) continue;

        float wobble = std::sin(slime.wobblePhase) * 2.5f;
        float r = 16.0f + wobble;
        float h = 14.0f - wobble * 0.7f;

        VECTOR center = VGet(slime.worldX, h * 0.5f, slime.worldZ);
        int bodyColor = (slime.state == SlimeState::Chasing) ? GetColor(180, 80, 40) : GetColor(40, 180, 60);
        int edgeColor = (slime.state == SlimeState::Chasing) ? GetColor(100, 30, 20) : GetColor(20, 80, 30);

        DrawCube3D(VGet(center.x - r, 0.0f, center.z - r),
            VGet(center.x + r, h, center.z + r),
            bodyColor, edgeColor, TRUE);

        DrawCube3D(VGet(center.x - 4.0f, 6.0f, center.z - 4.0f),
            VGet(center.x + 4.0f, 14.0f, center.z + 4.0f),
            GetColor(240, 255, 140), GetColor(120, 150, 40), TRUE);
    }
}