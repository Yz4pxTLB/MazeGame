#pragma once
#include "DxLib.h"

class Input {
private:
    char currentKeys[256] = {};
    char prevKeys[256] = {};
    int currentMouse = 0;
    int prevMouse = 0;
    int deltaMouseX = 0;
    int deltaMouseY = 0;

public:
    void Update(bool lockMouse);
    bool IsTriggered(int keyCode) const;
    bool IsPressed(int keyCode) const;
    bool IsMouseLeftTriggered() const; // 復活
    int GetDeltaX() const { return deltaMouseX; }
    int GetDeltaY() const { return deltaMouseY; }
};