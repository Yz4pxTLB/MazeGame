#include "Input.h"
#include "Common.h"

void Input::Update(bool lockMouse) {
    for (int i = 0; i < 256; ++i) {
        prevKeys[i] = currentKeys[i];
    }
    GetHitKeyStateAll(currentKeys);

    prevMouse = currentMouse;
    currentMouse = GetMouseInput();

    if (lockMouse) {
        int mx, my;
        GetMousePoint(&mx, &my);
        deltaMouseX = mx - (SCREEN_WIDTH / 2);
        deltaMouseY = my - (SCREEN_HEIGHT / 2);
        SetMousePoint(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
    }
    else {
        deltaMouseX = 0;
        deltaMouseY = 0;
    }
}

bool Input::IsTriggered(int keyCode) const {
    return (currentKeys[keyCode] == 1 && prevKeys[keyCode] == 0);
}

bool Input::IsPressed(int keyCode) const {
    return (currentKeys[keyCode] == 1);
}

bool Input::IsMouseLeftTriggered() const {
    return ((currentMouse & MOUSE_INPUT_LEFT) && !(prevMouse & MOUSE_INPUT_LEFT));
}