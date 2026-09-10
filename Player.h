#pragma once
#include "Common.h"

class DungeonMap;

class Player {
public:
    int gridX = 1;
    int gridY = 1;
    float posX = 1.0f;
    float posY = 1.0f;

    float yaw = PI / 2.0f;
    float pitch = 0.0f;

    bool isMoving = false;
    bool isContinuous = false; // 連続移動中フラグ (減速をキャンセル)
    float animTimer = 0.0f;
    float animDuration = 0.22f;

    float startX = 0.0f, startY = 0.0f;
    float targetX = 0.0f, targetY = 0.0f;
    float walkCycle = 0.0f;
    float currentBobY = 0.0f;

    int heldMoveDir = -1; // 押し続けられている方向

    int hp = 100;
    constexpr static int MAX_HP = 100;

    ItemId handSlots[HAND_SLOT_COUNT] = { ItemId::Lantern, ItemId::None };
    int activeHandIndex = 0;

    void SetSpawn(int x, int y, float initYaw = PI / 2.0f);
    void Update(float deltaTime, const DungeonMap& map);
    void AddLookAngles(float deltaX, float deltaY);
    void SetMoveInput(int relDir) { heldMoveDir = relDir; }

    void TakeDamage(int dmg);
    void Heal(int amount);

    ItemId GetActiveHandItem() const { return handSlots[activeHandIndex]; }
    void SwitchHandSlot() { activeHandIndex = (activeHandIndex + 1) % HAND_SLOT_COUNT; }

    int GetFacingDirection() const;
    float GetCameraBobY() const { return currentBobY; }

private:
    void StartStep(const DungeonMap& map, int relDir, float initialTimer = 0.0f);
};