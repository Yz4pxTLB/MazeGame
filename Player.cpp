#include "Player.h"
#include "DungeonMap.h"
#include <cmath>
#include <algorithm>

void Player::TakeDamage(int dmg) {
    hp = std::max(0, hp - dmg);
}

void Player::Heal(int amount) {
    hp = std::min(MAX_HP, hp + amount);
}

void Player::SetSpawn(int x, int y, float initYaw) {
    gridX = x;
    gridY = y;
    posX = static_cast<float>(x);
    posY = static_cast<float>(y);
    startX = posX; startY = posY;
    targetX = posX; targetY = posY;
    yaw = initYaw;
    pitch = 0.0f;
    isMoving = false;
    isContinuous = false;
    animTimer = 0.0f;
    walkCycle = 0.0f;
    currentBobY = 0.0f;
    heldMoveDir = -1;
}

void Player::AddLookAngles(float deltaX, float deltaY) {
    constexpr float SENSITIVITY = 0.003f;
    yaw += deltaX * SENSITIVITY;
    while (yaw < 0.0f) yaw += PI * 2.0f;
    while (yaw >= PI * 2.0f) yaw -= PI * 2.0f;

    pitch -= deltaY * SENSITIVITY;
    pitch = std::clamp(pitch, -1.15f, 1.15f);
}

int Player::GetFacingDirection() const {
    float norm = yaw + (PI * 0.25f);
    while (norm >= PI * 2.0f) norm -= PI * 2.0f;
    return static_cast<int>(norm / (PI * 0.5f)) % 4;
}

void Player::StartStep(const DungeonMap& map, int relDir, float initialTimer) {
    int targetDir = (GetFacingDirection() + relDir) % 4;
    const int fdx[4] = { 0,  1,  0, -1 };
    const int fdy[4] = { -1,  0,  1,  0 };
    int nextX = gridX + fdx[targetDir];
    int nextY = gridY + fdy[targetDir];

    if (!map.IsWall(nextX, nextY)) {
        isMoving = true;
        animTimer = initialTimer;

        // HP割合に応じて歩行時間を滑らかに補間
        // HP 100%: 0.20秒 (通常) / HP 0〜30%: 0.45〜0.48秒 (重傷・かつてのスライム並みの鈍足)
        float hpRatio = static_cast<float>(hp) / static_cast<float>(MAX_HP);
        hpRatio = std::clamp(hpRatio, 0.0f, 1.0f);
        animDuration = 0.20f + (1.0f - hpRatio) * 0.28f;

        startX = static_cast<float>(gridX);
        startY = static_cast<float>(gridY);
        targetX = static_cast<float>(nextX);
        targetY = static_cast<float>(nextY);

        gridX = nextX;
        gridY = nextY;
    }
    else {
        isMoving = false;
        isContinuous = false;
        animTimer = 0.0f;
    }
}

void Player::Update(float deltaTime, const DungeonMap& map) {
    if (isMoving) {
        walkCycle += deltaTime * (PI / animDuration);
        float targetBob = -std::sin(walkCycle) * 1.8f;
        currentBobY += (targetBob - currentBobY) * 0.5f;
    }
    else {
        currentBobY += (0.0f - currentBobY) * 0.2f;
    }

    if (!isMoving) {
        if (heldMoveDir != -1) {
            isContinuous = true;
            StartStep(map, heldMoveDir, 0.0f);
        }
        return;
    }

    animTimer += deltaTime;

    if (animTimer >= animDuration) {
        float surplus = animTimer - animDuration;
        posX = targetX;
        posY = targetY;

        if (heldMoveDir != -1) {
            isContinuous = true;
            StartStep(map, heldMoveDir, surplus);
        }
        else {
            isMoving = false;
            isContinuous = false;
            animTimer = 0.0f;
        }
    }
    else {
        float t = animTimer / animDuration;
        float progress = isContinuous ? t : (0.5f * (1.0f - std::cos(t * PI)));
        posX = startX + (targetX - startX) * progress;
        posY = startY + (targetY - startY) * progress;
    }
}