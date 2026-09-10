#pragma comment(linker, "/subsystem:windows")
#define NOMINMAX
#include "DxLib.h"
#include "Common.h"
#include "Input.h"
#include "DungeonMap.h"
#include "Player.h"
#include "Enemy.h"
#include "Item.h"
#include "PropRenderer.h"
#include "SaveManager.h"
#include <cmath>
#include <random>
#include <vector>
#include <string>
#include <algorithm>

enum class MenuState {
    Closed,
    MainMenu,
    MapViewer,
    InventoryViewer
};

// Shift-JIS対応の安全なテキスト自動折り返し関数 (全角文字幅で安全に改行)
static std::vector<std::string> WrapAndSplitText(const std::string& text, int maxCharsPerLine = 22) {
    std::vector<std::string> result;
    std::string currentLine = "";
    int currentWidth = 0;

    for (size_t i = 0; i < text.size(); ) {
        if (text[i] == '\r') {
            i++;
            continue;
        }
        if (text[i] == '\n') {
            result.push_back(currentLine);
            currentLine.clear();
            currentWidth = 0;
            i++;
            continue;
        }

        unsigned char uc = static_cast<unsigned char>(text[i]);
        bool isMultiByte = ((uc >= 0x81 && uc <= 0x9F) || (uc >= 0xE0 && uc <= 0xFC));
        int charBytes = (isMultiByte && (i + 1 < text.size())) ? 2 : 1;
        int charWidth = 1;

        if (currentWidth + charWidth > maxCharsPerLine) {
            result.push_back(currentLine);
            currentLine.clear();
            currentWidth = 0;
        }

        currentLine += text.substr(i, charBytes);
        currentWidth += charWidth;
        i += charBytes;
    }

    if (!currentLine.empty()) {
        result.push_back(currentLine);
    }
    return result;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    ChangeWindowMode(TRUE);
    SetGraphMode(SCREEN_WIDTH, SCREEN_HEIGHT, 32);
    SetWindowText("3D Dungeon Escape - Abandoned");

    if (DxLib_Init() == -1) return -1;
    SetDrawScreen(DX_SCREEN_BACK);

    SetUseZBuffer3D(TRUE);
    SetWriteZBuffer3D(TRUE);
    SetCameraNearFar(1.0f, 2000.0f);

    SetFogEnable(TRUE);

    ChangeLightTypeDir(VGet(0.2f, -1.0f, 0.2f));
    SetLightDifColor(GetColorF(0.12f, 0.12f, 0.16f, 1.0f));

    int lanternLight = CreatePointLightHandle(VGet(0, 0, 0), 520.0f, 0.15f, 0.002f, 0.0f);
    SetLightDifColorHandle(lanternLight, GetColorF(1.0f, 0.78f, 0.42f, 1.0f));

    int matchLight = CreatePointLightHandle(VGet(0, 0, 0), 240.0f, 0.2f, 0.008f, 0.0f);
    SetLightDifColorHandle(matchLight, GetColorF(1.0f, 0.55f, 0.25f, 1.0f));

    constexpr int MAX_CEILING_LIGHTS = 3;
    int ceilingLights[MAX_CEILING_LIGHTS];
    for (int i = 0; i < MAX_CEILING_LIGHTS; ++i) {
        ceilingLights[i] = CreatePointLightHandle(VGet(0, 0, 0), 220.0f, 0.1f, 0.006f, 0.0f);
        SetLightEnableHandle(ceilingLights[i], FALSE);
    }

    bool isLanternOn = true;
    float lanternOil = 100.0f;
    constexpr float MAX_OIL = 100.0f;

    bool isMatchLit = false;
    float matchTimer = 0.0f;
    constexpr float MATCH_DURATION = 30.0f;

    int diaryFoundCount = 0;

    std::vector<InventoryItem> inventory;
    std::vector<ItemId> crateDropHistory;

    Input input;
    DungeonMap dungeon;
    Player player;
    EnemyManager enemyManager;
    SaveManager saveManager;

    saveManager.LoadAllHeaders();

    Scene currentScene = Scene::SlotSelect;
    MenuState menuState = MenuState::Closed;

    int selectedSlot = 0;
    int autoSaveTimer = 0;
    unsigned int currentSeed = 0;
    bool isDevMode = false;

    int menuCursor = 0;
    constexpr int MENU_ITEM_COUNT = 5;
    const char* menuItems[MENU_ITEM_COUNT] = {
        "1. MAP (CHART) ",
        "2. INVENTORY & STATUS ",
        "3. SAVE GAME ",
        "4. RESUME ",
        "5. RETURN TO TITLE "
    };

    int invCursor = 0;
    int prevInvCursor = -1;
    int invScroll = 0;
    constexpr int INV_PAGE_ITEMS = 5;

    int descScroll = 0;
    constexpr int DESC_PAGE_LINES = 3;

    int popupScroll = 0;
    constexpr int POPUP_PAGE_LINES = 8;

    std::string diaryPopupText = "";
    char messageText[64] = "";
    int messageTimer = 0;

    auto AddItemToInventory = [&](const InventoryItem& newItem) {
        if (newItem.id == ItemId::None) return;

        bool isUnique = (newItem.id == ItemId::Shotgun ||
            newItem.id == ItemId::Match ||
            newItem.id == ItemId::OldDiary ||
            newItem.id == ItemId::Lantern);

        if (!isUnique) {
            for (auto& item : inventory) {
                if (item.id == newItem.id) {
                    item.count += newItem.count;
                    return;
                }
            }
        }
        inventory.push_back(newItem);
        };

    auto ConsumeOneMatch = [&]() -> bool {
        for (auto it = inventory.begin(); it != inventory.end(); ++it) {
            if (it->id == ItemId::Match && it->uses > 0) {
                it->uses--;
                if (it->uses <= 0) {
                    inventory.erase(it);
                }
                else {
                    *it = ItemManager::CreateItem(ItemId::Match, it->uses);
                }
                return true;
            }
        }
        return false;
        };

    // セーブ処理 (手持ちスロット2枠を両方保存)
    auto ExecuteSave = [&](bool isAuto) {
        GameSaveData data;
        data.header.isValid = true;
        data.header.floor = dungeon.GetCurrentFloor();
        data.header.seed = currentSeed;

        data.posX = player.posX;
        data.posY = player.posY;
        data.gridX = player.gridX;
        data.gridY = player.gridY;
        data.yaw = player.yaw;
        data.pitch = player.pitch;

        // 手持ち2枠と選択中枠を保存
        data.handSlot0 = static_cast<int>(player.handSlots[0]);
        data.handSlot1 = static_cast<int>(player.handSlots[1]);
        data.activeHandIndex = player.activeHandIndex;

        data.isLanternOn = isLanternOn;
        data.lanternOil = lanternOil;
        data.isMatchLit = isMatchLit;
        data.matchTimer = matchTimer;
        data.diaryCount = diaryFoundCount;

        for (const auto& item : inventory) {
            data.inventory.push_back({ static_cast<int>(item.id), item.count, item.uses, item.durability });
        }

        data.mapWidth = dungeon.GetWidth();
        data.mapHeight = dungeon.GetHeight();
        data.visitedData.resize(data.mapWidth * data.mapHeight);
        for (int y = 0; y < data.mapHeight; ++y) {
            for (int x = 0; x < data.mapWidth; ++x) {
                data.visitedData[y * data.mapWidth + x] = dungeon.IsVisited(x, y) ? 1 : 0;
            }
        }

        const auto& props = dungeon.GetProps();
        data.propsCollected.resize(props.size());
        for (size_t i = 0; i < props.size(); ++i) {
            data.propsCollected[i] = props[i].isCollected ? 1 : 0;
        }

        saveManager.SaveSlot(selectedSlot, data);
        if (isAuto) autoSaveTimer = 90;
        else {
            sprintf_s(messageText, "進行状況をセーブしました ");
            messageTimer = 110;
        }
        };

    LONGLONG prevTime = GetNowHiPerformanceCount();

    while (ProcessMessage() == 0) {
        LONGLONG currentTime = GetNowHiPerformanceCount();
        float deltaTime = static_cast<float>(currentTime - prevTime) / 1000000.0f;
        prevTime = currentTime;
        if (deltaTime > 0.1f) deltaTime = 0.1f;

        bool isAltPressed = input.IsPressed(KEY_INPUT_LALT) || input.IsPressed(KEY_INPUT_RALT);
        bool lockMouse = (currentScene == Scene::GamePlay && menuState == MenuState::Closed && !isAltPressed);
        SetMouseDispFlag(!lockMouse);
        input.Update(lockMouse);

        if (input.IsTriggered(KEY_INPUT_ESCAPE)) {
            if (!diaryPopupText.empty()) {
                diaryPopupText.clear();
            }
            else if (menuState == MenuState::MapViewer || menuState == MenuState::InventoryViewer) {
                menuState = MenuState::MainMenu;
            }
            else if (menuState == MenuState::MainMenu) {
                menuState = MenuState::Closed;
            }
            else if (currentScene == Scene::SlotSelect) {
                break;
            }
        }

        // ====================================================
        // 1. ロジック更新
        // ====================================================
        if (currentScene == Scene::SlotSelect) {
            if (input.IsTriggered(KEY_INPUT_UP) || input.IsTriggered(KEY_INPUT_W)) {
                selectedSlot = (selectedSlot + SaveManager::SLOT_COUNT - 1) % SaveManager::SLOT_COUNT;
            }
            if (input.IsTriggered(KEY_INPUT_DOWN) || input.IsTriggered(KEY_INPUT_S)) {
                selectedSlot = (selectedSlot + 1) % SaveManager::SLOT_COUNT;
            }

            if (input.IsTriggered(KEY_INPUT_RETURN) || input.IsTriggered(KEY_INPUT_Z) || input.IsTriggered(KEY_INPUT_SPACE)) {
                GameSaveData loadedData;
                if (saveManager.LoadSlot(selectedSlot, loadedData) && loadedData.header.isValid) {
                    currentSeed = loadedData.header.seed;
                    dungeon.GenerateFloor(loadedData.header.floor, currentSeed);

                    player.SetSpawn(loadedData.gridX, loadedData.gridY, loadedData.yaw);
                    player.posX = loadedData.posX;
                    player.posY = loadedData.posY;
                    player.pitch = loadedData.pitch;

                    dungeon.SetVisitedRaw(loadedData.mapWidth, loadedData.mapHeight, loadedData.visitedData);
                    dungeon.SetPropsCollected(loadedData.propsCollected);

                    // 手持ちスロット2枠の完全復元
                    player.handSlots[0] = static_cast<ItemId>(loadedData.handSlot0);
                    player.handSlots[1] = static_cast<ItemId>(loadedData.handSlot1);
                    player.activeHandIndex = loadedData.activeHandIndex;

                    isLanternOn = loadedData.isLanternOn;
                    lanternOil = loadedData.lanternOil;
                    isMatchLit = loadedData.isMatchLit;
                    matchTimer = loadedData.matchTimer;
                    diaryFoundCount = loadedData.diaryCount;

                    inventory.clear();
                    for (const auto& invData : loadedData.inventory) {
                        InventoryItem item;
                        if (static_cast<ItemId>(invData.id) == ItemId::OldDiary) {
                            item = ItemManager::CreateDiaryItem(invData.uses);
                        }
                        else {
                            item = ItemManager::CreateItem(static_cast<ItemId>(invData.id), invData.uses);
                        }
                        item.durability = invData.durability;
                        item.count = invData.count;
                        inventory.push_back(item);
                    }
                    crateDropHistory.clear();
                    enemyManager.GenerateEnemies(dungeon, currentSeed + dungeon.GetCurrentFloor() * 333);
                }
                else {
                    std::random_device rd;
                    currentSeed = rd();

                    dungeon.GenerateFloor(1, currentSeed);
                    player.SetSpawn(dungeon.GetStartX(), dungeon.GetStartY());
                    dungeon.RevealAround(player.gridX, player.gridY);

                    inventory.clear();
                    // 初期所持アイテム
                    inventory.push_back(ItemManager::CreateItem(ItemId::Lantern));
                    inventory.push_back(ItemManager::CreateItem(ItemId::Shotgun, 100)); // 耐久100%
                    inventory.push_back(ItemManager::CreateItem(ItemId::ShotgunShells, 4)); // 散弾4発
                    inventory.push_back(ItemManager::CreateItem(ItemId::Match, 2));

                    // 手持ちスロット設定
                    player.handSlots[0] = ItemId::Lantern;  // 枠1: ランタン
                    player.handSlots[1] = ItemId::Shotgun;  // 枠2: 古びたショットガン
                    player.activeHandIndex = 0;             // 初期はランタンを構える
                    player.hp = 100;

                    isLanternOn = true;
                    lanternOil = 100.0f;
                    isMatchLit = false;
                    matchTimer = 0.0f;
                    diaryFoundCount = 0;

                    crateDropHistory.clear();
                    enemyManager.GenerateEnemies(dungeon, currentSeed + 333);
                    ExecuteSave(true);
                }

                menuState = MenuState::Closed;
                currentScene = Scene::GamePlay;
            }

            if (input.IsTriggered(KEY_INPUT_DELETE) || input.IsTriggered(KEY_INPUT_BACK)) {
                saveManager.DeleteSlot(selectedSlot);
            }
        }
        else if (currentScene == Scene::GamePlay) {
            if (autoSaveTimer > 0) autoSaveTimer--;
            if (messageTimer > 0) messageTimer--;
            dungeon.UpdateProps(deltaTime);

            std::string enemyMsg = "";
            enemyManager.Update(deltaTime, dungeon, player, enemyMsg);
            if (!enemyMsg.empty()) {
                sprintf_s(messageText, "%s", enemyMsg.c_str());
                messageTimer = 100;
            }

            if (player.GetActiveHandItem() == ItemId::Lantern && isLanternOn) {
                lanternOil -= deltaTime * 0.22f;
                if (lanternOil <= 0.0f) {
                    lanternOil = 0.0f;
                    isLanternOn = false;
                    sprintf_s(messageText, "ランタンの油が尽きて消えてしまった ");
                    messageTimer = 120;
                }
            }

            if (isMatchLit) {
                matchTimer -= deltaTime;
                if (matchTimer <= 0.0f) {
                    matchTimer = 0.0f;
                    isMatchLit = false;
                    sprintf_s(messageText, "マッチの火が燃え尽きた ");
                    messageTimer = 100;
                }
            }

            bool isCtrl = input.IsPressed(KEY_INPUT_LCONTROL) || input.IsPressed(KEY_INPUT_RCONTROL);
            bool isShift = input.IsPressed(KEY_INPUT_LSHIFT) || input.IsPressed(KEY_INPUT_RSHIFT);

            if (isCtrl && isShift && input.IsTriggered(KEY_INPUT_F1)) {
                isDevMode = !isDevMode;
            }
            else if (!isCtrl && !isShift && (input.IsTriggered(KEY_INPUT_F1) || input.IsTriggered(KEY_INPUT_TAB) || input.IsTriggered(KEY_INPUT_M))) {
                menuState = (menuState == MenuState::Closed) ? MenuState::MainMenu : MenuState::Closed;
            }

            if (input.IsTriggered(KEY_INPUT_Q)) {
                player.SwitchHandSlot();
                ItemId cur = player.GetActiveHandItem();
                if (cur == ItemId::Lantern) sprintf_s(messageText, "ランタンを構えた ");
                else if (cur == ItemId::Match) sprintf_s(messageText, "マッチを構えた ");
                else if (cur == ItemId::Shotgun) sprintf_s(messageText, "古びたショットガンを構えた ");
                else sprintf_s(messageText, "手元を空けた ");
                messageTimer = 80;
            }

            if (input.IsTriggered(KEY_INPUT_L)) {
                ItemId cur = player.GetActiveHandItem();
                if (cur == ItemId::Lantern) {
                    if (isLanternOn) {
                        isLanternOn = false;
                        sprintf_s(messageText, "ランタンの火を消した ");
                        messageTimer = 80;
                    }
                    else {
                        if (lanternOil <= 0.0f) {
                            sprintf_s(messageText, "油が入っていないため点火できない ");
                            messageTimer = 90;
                        }
                        else {
                            if (isMatchLit) {
                                isLanternOn = true;
                                sprintf_s(messageText, "マッチの火をランタンに移した ");
                                messageTimer = 100;
                            }
                            else if (ConsumeOneMatch()) {
                                isLanternOn = true;
                                sprintf_s(messageText, "マッチを擦ってランタンに火を点けた ");
                                messageTimer = 100;
                            }
                            else {
                                sprintf_s(messageText, "火を点けるためのマッチがない！ ");
                                messageTimer = 100;
                            }
                        }
                    }
                }
                else if (cur == ItemId::Match) {
                    if (isMatchLit) {
                        isMatchLit = false;
                        matchTimer = 0.0f;
                        sprintf_s(messageText, "マッチの火を吹き消した ");
                        messageTimer = 80;
                    }
                    else {
                        if (ConsumeOneMatch()) {
                            isMatchLit = true;
                            matchTimer = MATCH_DURATION;
                            sprintf_s(messageText, "マッチを擦った (30秒間点火) ");
                            messageTimer = 100;
                        }
                        else {
                            sprintf_s(messageText, "擦れるマッチがもう残っていない ");
                            messageTimer = 90;
                        }
                    }
                }
            }

            if (menuState == MenuState::MainMenu) {
                if (input.IsTriggered(KEY_INPUT_UP) || input.IsTriggered(KEY_INPUT_W)) {
                    menuCursor = (menuCursor + MENU_ITEM_COUNT - 1) % MENU_ITEM_COUNT;
                }
                if (input.IsTriggered(KEY_INPUT_DOWN) || input.IsTriggered(KEY_INPUT_S)) {
                    menuCursor = (menuCursor + 1) % MENU_ITEM_COUNT;
                }

                if (input.IsTriggered(KEY_INPUT_RETURN) || input.IsTriggered(KEY_INPUT_Z) || input.IsTriggered(KEY_INPUT_SPACE)) {
                    if (menuCursor == 0) {
                        menuState = MenuState::MapViewer;
                    }
                    else if (menuCursor == 1) {
                        menuState = MenuState::InventoryViewer;
                        invCursor = 0;
                        invScroll = 0;
                        descScroll = 0;
                        prevInvCursor = -1;
                    }
                    else if (menuCursor == 2) {
                        ExecuteSave(false);
                    }
                    else if (menuCursor == 3) {
                        menuState = MenuState::Closed;
                    }
                    else if (menuCursor == 4) {
                        ExecuteSave(true);
                        saveManager.LoadAllHeaders();
                        currentScene = Scene::SlotSelect;
                        menuState = MenuState::Closed;
                    }
                }
            }
            else if (menuState == MenuState::MapViewer) {
                if (input.IsTriggered(KEY_INPUT_RETURN) || input.IsTriggered(KEY_INPUT_Z) || input.IsTriggered(KEY_INPUT_BACK)) {
                    menuState = MenuState::MainMenu;
                }
            }
            else if (menuState == MenuState::InventoryViewer) {
                if (!diaryPopupText.empty()) {
                    auto pLines = WrapAndSplitText(diaryPopupText, 21);
                    int maxPopScroll = std::max(0, static_cast<int>(pLines.size()) - POPUP_PAGE_LINES);

                    if (input.IsTriggered(KEY_INPUT_UP) || input.IsTriggered(KEY_INPUT_W)) {
                        popupScroll = std::max(0, popupScroll - 1);
                    }
                    if (input.IsTriggered(KEY_INPUT_DOWN) || input.IsTriggered(KEY_INPUT_S)) {
                        popupScroll = std::min(maxPopScroll, popupScroll + 1);
                    }
                    if (input.IsTriggered(KEY_INPUT_RETURN) || input.IsTriggered(KEY_INPUT_Z) || input.IsTriggered(KEY_INPUT_SPACE)) {
                        diaryPopupText.clear();
                    }
                }
                else {
                    int totalItems = static_cast<int>(inventory.size());
                    if (totalItems > 0) {
                        if (input.IsTriggered(KEY_INPUT_UP) || input.IsTriggered(KEY_INPUT_W)) {
                            invCursor = (invCursor + totalItems - 1) % totalItems;
                        }
                        if (input.IsTriggered(KEY_INPUT_DOWN) || input.IsTriggered(KEY_INPUT_S)) {
                            invCursor = (invCursor + 1) % totalItems;
                        }

                        if (invCursor != prevInvCursor) {
                            descScroll = 0;
                            prevInvCursor = invCursor;
                        }

                        auto curDescLines = WrapAndSplitText(inventory[invCursor].desc, 23);
                        int maxDescScroll = std::max(0, static_cast<int>(curDescLines.size()) - DESC_PAGE_LINES);

                        if (input.IsTriggered(KEY_INPUT_LEFT) || input.IsTriggered(KEY_INPUT_A)) {
                            descScroll = std::max(0, descScroll - 1);
                        }
                        if (input.IsTriggered(KEY_INPUT_RIGHT) || input.IsTriggered(KEY_INPUT_D)) {
                            descScroll = std::min(maxDescScroll, descScroll + 1);
                        }

                        if (invCursor < invScroll) invScroll = invCursor;
                        if (invCursor >= invScroll + INV_PAGE_ITEMS) invScroll = invCursor - INV_PAGE_ITEMS + 1;

                        if (input.IsTriggered(KEY_INPUT_1)) {
                            player.handSlots[0] = inventory[invCursor].id;
                            sprintf_s(messageText, "スロット1に装備した ");
                            messageTimer = 80;
                        }
                        if (input.IsTriggered(KEY_INPUT_2)) {
                            player.handSlots[1] = inventory[invCursor].id;
                            sprintf_s(messageText, "スロット2に装備した ");
                            messageTimer = 80;
                        }

                        if (input.IsTriggered(KEY_INPUT_RETURN) || input.IsTriggered(KEY_INPUT_Z) || input.IsTriggered(KEY_INPUT_SPACE)) {
                            auto& item = inventory[invCursor];
                            if (item.id == ItemId::OldDiary) {
                                diaryPopupText = item.desc;
                                popupScroll = 0;
                            }
                            else {
                                std::string outMsg = "";
                                if (ItemManager::UseItem(item, player, lanternOil, outMsg)) {
                                    sprintf_s(messageText, "%s", outMsg.c_str());
                                    messageTimer = 100;
                                    item.count--;
                                    if (item.count <= 0) {
                                        inventory.erase(inventory.begin() + invCursor);
                                        invCursor = std::max(0, static_cast<int>(inventory.size()) - 1);
                                        if (invScroll > 0 && invScroll + INV_PAGE_ITEMS > static_cast<int>(inventory.size())) {
                                            invScroll = std::max(0, invScroll - 1);
                                        }
                                        prevInvCursor = -1;
                                    }
                                }
                                else {
                                    sprintf_s(messageText, "%s", outMsg.c_str());
                                    messageTimer = 80;
                                }
                            }
                        }
                    }
                }
            }
            else if (menuState == MenuState::Closed) {
                if (!isAltPressed) {
                    player.AddLookAngles(static_cast<float>(input.GetDeltaX()), static_cast<float>(input.GetDeltaY()));
                }

                int currentInputMove = -1;
                if (input.IsPressed(KEY_INPUT_UP) || input.IsPressed(KEY_INPUT_W)) currentInputMove = 0;
                else if (input.IsPressed(KEY_INPUT_DOWN) || input.IsPressed(KEY_INPUT_S)) currentInputMove = 2;
                else if (input.IsPressed(KEY_INPUT_LEFT) || input.IsPressed(KEY_INPUT_A)) currentInputMove = 3;
                else if (input.IsPressed(KEY_INPUT_RIGHT) || input.IsPressed(KEY_INPUT_D)) currentInputMove = 1;

                player.SetMoveInput(currentInputMove);

                if (dungeon.IsGoal(player.gridX, player.gridY) && !player.isMoving) {
                    int nextFloor = dungeon.GetCurrentFloor() + 1;
                    dungeon.GenerateFloor(nextFloor, currentSeed);
                    player.SetSpawn(dungeon.GetStartX(), dungeon.GetStartY());
                    dungeon.RevealAround(player.gridX, player.gridY);

                    crateDropHistory.clear();
                    enemyManager.GenerateEnemies(dungeon, currentSeed + nextFloor * 333);
                    ExecuteSave(true);
                }

                player.Update(deltaTime, dungeon);

                int currentGridX = static_cast<int>(std::round(player.posX));
                int currentGridY = static_cast<int>(std::round(player.posY));
                dungeon.RevealAround(currentGridX, currentGridY);

                float camZ = (dungeon.GetHeight() - 1 - player.posY) * BLOCK_SIZE + (BLOCK_SIZE / 2.0f);
                VECTOR camPos = VGet(
                    player.posX * BLOCK_SIZE + (BLOCK_SIZE / 2.0f),
                    BLOCK_SIZE * 0.55f + player.GetCameraBobY(),
                    camZ
                );
                VECTOR lookDir = VGet(
                    std::sin(player.yaw) * std::cos(player.pitch),
                    std::sin(player.pitch),
                    std::cos(player.yaw) * std::cos(player.pitch)
                );

                int targetedIndex = -1;
                float minRayDist = 999.0f;
                auto& props = dungeon.GetProps();

                for (size_t i = 0; i < props.size(); ++i) {
                    if (props[i].isCollected || props[i].type == PropType::CeilingLamp) continue;

                    VECTOR toProp = VGet(props[i].worldX - camPos.x, props[i].worldY - camPos.y, props[i].worldZ - camPos.z);
                    float t = toProp.x * lookDir.x + toProp.y * lookDir.y + toProp.z * lookDir.z;

                    if (t > 12.0f && t < 120.0f) {
                        VECTOR closestPoint = VGet(camPos.x + lookDir.x * t, camPos.y + lookDir.y * t, camPos.z + lookDir.z * t);
                        float diffX = props[i].worldX - closestPoint.x;
                        float diffY = props[i].worldY - closestPoint.y;
                        float diffZ = props[i].worldZ - closestPoint.z;
                        float rayDist = std::sqrt(diffX * diffX + diffY * diffY + diffZ * diffZ);

                        float effectiveRadius = (props[i].type == PropType::Desk) ? 24.0f :
                            (props[i].type == PropType::Crate) ? 16.0f : 12.0f;

                        if (rayDist <= effectiveRadius && rayDist < minRayDist) {
                            minRayDist = rayDist;
                            targetedIndex = static_cast<int>(i);
                        }
                    }
                }

                bool isInteracting = input.IsTriggered(KEY_INPUT_E) || (targetedIndex != -1 && input.IsMouseLeftTriggered());

                if (targetedIndex != -1 && isInteracting) {
                    auto& prop = props[targetedIndex];

                    if (prop.type == PropType::SmallItem) {
                        prop.isCollected = true;
                        if (prop.itemId == ItemId::OldDiary) {
                            diaryFoundCount++;
                            AddItemToInventory(ItemManager::CreateDiaryItem(diaryFoundCount));
                        }
                        else {
                            AddItemToInventory(ItemManager::CreateItem(prop.itemId));
                        }
                        sprintf_s(messageText, "「%s」を手に入れた ", prop.name);
                        messageTimer = 110;
                    }
                    else if (prop.type == PropType::Crate) {
                        prop.isCollected = true;
                        InventoryItem drop = ItemManager::RollCrateDrop(crateDropHistory, currentSeed + prop.gridX * 67 + prop.gridY * 89);

                        if (drop.id == ItemId::None) {
                            const char* emptyCrateMsgs[] = {
                                "木箱の蓋を開けたが、中には湿った埃と木屑しか残っていなかった ",
                                "底が抜けており、何も入っていない ",
                                "何者かに荒らされた跡があり、中はもぬけの殻だ "
                            };
                            sprintf_s(messageText, "%s", emptyCrateMsgs[rand() % 3]);
                        }
                        else {
                            AddItemToInventory(drop);
                            sprintf_s(messageText, "木箱から「%s」を見つけた！ ", drop.name.c_str());
                        }
                        messageTimer = 120;
                    }
                    else if (prop.type == PropType::Desk) {
                        if (player.GetActiveHandItem() == ItemId::Shotgun) {
                            int gunIdx = -1;
                            int partIdx = -1;
                            for (size_t i = 0; i < inventory.size(); ++i) {
                                if (inventory[i].id == ItemId::Shotgun && gunIdx == -1) gunIdx = static_cast<int>(i);
                                if (inventory[i].id == ItemId::GunParts && inventory[i].count > 0 && partIdx == -1) partIdx = static_cast<int>(i);
                            }

                            if (gunIdx != -1) {
                                // 1. 予備部品がある場合：耐久+20% & 応急整備回数を最大(2回)に回復
                                if (partIdx != -1) {
                                    inventory[partIdx].count--;
                                    inventory[gunIdx].durability = std::min(100.0f, inventory[gunIdx].durability + 20.0f);
                                    inventory[gunIdx].uses = 2; // 応急整備可能回数を2回に全快

                                    if (inventory[partIdx].count <= 0) {
                                        inventory.erase(inventory.begin() + partIdx);
                                    }
                                    sprintf_s(messageText, "予備部品で整備した (耐久+20%% / 応急手入れ回数回復) ");
                                }
                                // 2. 予備部品がない場合：応急整備 (耐久+5% / 2回まで)
                                else {
                                    if (inventory[gunIdx].durability >= 100.0f) {
                                        sprintf_s(messageText, "ショットガンはすでに万全な状態だ ");
                                    }
                                    else if (inventory[gunIdx].uses > 0) {
                                        inventory[gunIdx].durability = std::min(100.0f, inventory[gunIdx].durability + 5.0f);
                                        inventory[gunIdx].uses--;
                                        sprintf_s(messageText, "作業台の工具で応急手入れを行った (+5%% / 残り%d回) ", inventory[gunIdx].uses);
                                    }
                                    else {
                                        sprintf_s(messageText, "これ以上の応急手入れは効かない。予備部品が必要だ ");
                                    }
                                }
                            }
                            else {
                                sprintf_s(messageText, "手入れするショットガンが見当たらない ");
                            }
                            messageTimer = 130;
                        }
                        else {
                            if (!prop.isSearched) {
                                prop.isSearched = true;
                                ItemId deskItem = ItemManager::RollDeskItem(currentSeed + prop.gridX * 43 + prop.gridY * 91);

                                if (deskItem == ItemId::None) {
                                    const char* emptyDeskMsgs[] = {
                                        "引き出しを開けたが、湿った蜘蛛の巣しか見当たらない ",
                                        "作業台をくまなく探したが、めぼしい物は残っていなかった ",
                                        "錆び付いた金具の破片が転がっているだけで、使い物にならない "
                                    };
                                    sprintf_s(messageText, "%s", emptyDeskMsgs[rand() % 3]);
                                }
                                else if (deskItem == ItemId::Shotgun) {
                                    int existingGunIdx = -1;
                                    for (size_t i = 0; i < inventory.size(); ++i) {
                                        if (inventory[i].id == ItemId::Shotgun) {
                                            existingGunIdx = static_cast<int>(i);
                                            break;
                                        }
                                    }

                                    if (existingGunIdx != -1) {
                                        inventory[existingGunIdx].durability = 100.0f;
                                        inventory[existingGunIdx].uses = 2; // 応急回数もリフレッシュ
                                        sprintf_s(messageText, "より状態の良いショットガンと入れ替えた (耐久100%%) ");
                                    }
                                    else {
                                        AddItemToInventory(ItemManager::CreateItem(ItemId::Shotgun, 100));
                                        sprintf_s(messageText, "作業台から古びたショットガンを手に入れた！ ");
                                    }
                                }
                                else if (deskItem == ItemId::OldDiary) {
                                    diaryFoundCount++;
                                    InventoryItem drop = ItemManager::CreateDiaryItem(diaryFoundCount);
                                    AddItemToInventory(drop);
                                    sprintf_s(messageText, "作業台から「%s」を見つけた！ ", drop.name.c_str());
                                }
                                else {
                                    InventoryItem drop = ItemManager::CreateItem(deskItem);
                                    AddItemToInventory(drop);
                                    sprintf_s(messageText, "作業台から「%s」を見つけた！ ", drop.name.c_str());
                                }
                            }
                            else {
                                sprintf_s(messageText, "作業台にはもう物はない。銃の整備に利用できそうだ ");
                            }
                            messageTimer = 120;
                        }
                    }
                }
                else if (player.GetActiveHandItem() == ItemId::Shotgun &&
                    (input.IsTriggered(KEY_INPUT_SPACE) || (targetedIndex == -1 && input.IsMouseLeftTriggered()))) {
                    int shellIdx = -1;
                    int gunIdx = -1;
                    for (size_t i = 0; i < inventory.size(); ++i) {
                        if (inventory[i].id == ItemId::ShotgunShells && inventory[i].count > 0 && shellIdx == -1) shellIdx = static_cast<int>(i);
                        if (inventory[i].id == ItemId::Shotgun && gunIdx == -1) gunIdx = static_cast<int>(i);
                    }

                    if (shellIdx != -1) {
                        inventory[shellIdx].count--;
                        if (inventory[shellIdx].count <= 0) inventory.erase(inventory.begin() + shellIdx);

                        if (gunIdx != -1 && gunIdx < static_cast<int>(inventory.size())) {
                            inventory[gunIdx].durability = std::max(0.0f, inventory[gunIdx].durability - 5.0f);
                        }

                        player.pitch += 0.14f;

                        if (enemyManager.TryShootAtEnemy(camPos, lookDir, 280.0f)) {
                            sprintf_s(messageText, "散弾が直撃し、スライムを吹き飛ばした！ ");
                        }
                        else {
                            sprintf_s(messageText, "発砲！ 通路の壁に散弾がめり込んだ ");
                        }
                        messageTimer = 110;
                    }
                    else {
                        sprintf_s(messageText, "カチリ…… 散弾が装填されていない ");
                        messageTimer = 80;
                    }
                }
            }
        }

        // ====================================================
        // 2. 描画処理
        // ====================================================
        ClearDrawScreen();

        if (currentScene == Scene::SlotSelect) {
            SetUseZBuffer3D(FALSE);
            DrawBox(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GetColor(15, 15, 25), TRUE);
            DrawFormatString(SCREEN_WIDTH / 2 - 80, 50, GetColor(255, 255, 255), "== SELECT DATA ==");

            for (int i = 0; i < SaveManager::SLOT_COUNT; ++i) {
                const auto& header = saveManager.GetSlotHeader(i);
                int boxY = 120 + i * 80;
                bool isCursor = (i == selectedSlot);

                int boxColor = isCursor ? GetColor(40, 70, 140) : GetColor(30, 30, 45);
                int edgeColor = isCursor ? GetColor(255, 215, 0) : GetColor(80, 80, 100);
                DrawBox(120, boxY, SCREEN_WIDTH - 120, boxY + 60, boxColor, TRUE);
                DrawBox(120, boxY, SCREEN_WIDTH - 120, boxY + 60, edgeColor, FALSE);

                DrawFormatString(140, boxY + 12, GetColor(255, 255, 255), "SLOT %d", i + 1);
                if (header.isValid) {
                    DrawFormatString(140, boxY + 32, GetColor(100, 255, 100), "FLOOR: B%dF (RESUME) ", header.floor);
                }
                else {
                    DrawFormatString(140, boxY + 32, GetColor(150, 150, 150), "- NO DATA -");
                }
            }

            DrawFormatString(120, 380, GetColor(180, 180, 180), "[W/S or UP/DOWN] : Select Slot");
            DrawFormatString(120, 400, GetColor(180, 180, 180), "[ENTER / Z]      : Start / Resume");
            DrawFormatString(120, 420, GetColor(180, 180, 180), "[DEL / BACK]     : Delete Data");
        }
        else if (currentScene == Scene::GamePlay) {
            float hpRatio = static_cast<float>(player.hp) / static_cast<float>(Player::MAX_HP);
            hpRatio = std::clamp(hpRatio, 0.0f, 1.0f);

            float fogStart = 15.0f + 55.0f * hpRatio;
            float fogEnd = 165.0f + 485.0f * hpRatio;

            int fogR = static_cast<int>(24 * (1.0f - hpRatio) + 10 * hpRatio);
            int fogG = static_cast<int>(4 * (1.0f - hpRatio) + 10 * hpRatio);
            int fogB = static_cast<int>(6 * (1.0f - hpRatio) + 16 * hpRatio);

            SetFogColor(fogR, fogG, fogB);
            SetFogStartEnd(fogStart, fogEnd);

            SetUseZBuffer3D(FALSE);
            DrawBox(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GetColor(fogR, fogG, fogB), TRUE);
            SetUseZBuffer3D(TRUE);

            float camZ = (dungeon.GetHeight() - 1 - player.posY) * BLOCK_SIZE + (BLOCK_SIZE / 2.0f);
            VECTOR camPos = VGet(
                player.posX * BLOCK_SIZE + (BLOCK_SIZE / 2.0f),
                BLOCK_SIZE * 0.55f + player.GetCameraBobY(),
                camZ
            );
            VECTOR lookDir = VGet(
                std::sin(player.yaw) * std::cos(player.pitch),
                std::sin(player.pitch),
                std::cos(player.yaw) * std::cos(player.pitch)
            );
            VECTOR rightDir = VGet(std::cos(player.yaw), 0.0f, -std::sin(player.yaw));
            VECTOR upDir = VGet(0.0f, 1.0f, 0.0f);

            VECTOR camTarget = VGet(camPos.x + lookDir.x * 100.0f, camPos.y + lookDir.y * 100.0f, camPos.z + lookDir.z * 100.0f);
            SetCameraPositionAndTarget_UpVecY(camPos, camTarget);

            if (player.GetActiveHandItem() == ItemId::Lantern && isLanternOn && lanternOil > 0.0f) {
                SetLightEnableHandle(lanternLight, TRUE);
                SetLightPositionHandle(lanternLight, camPos);
            }
            else {
                SetLightEnableHandle(lanternLight, FALSE);
            }

            if (player.GetActiveHandItem() == ItemId::Match && isMatchLit) {
                SetLightEnableHandle(matchLight, TRUE);
                SetLightPositionHandle(matchLight, camPos);
            }
            else {
                SetLightEnableHandle(matchLight, FALSE);
            }

            struct VisibleLamp { VECTOR pos; float dist; };
            std::vector<VisibleLamp> visibleLamps;
            constexpr float MAX_LAMP_AFFECT_DIST = 450.0f;

            const auto& props = dungeon.GetProps();
            for (const auto& prop : props) {
                if (prop.type == PropType::CeilingLamp && prop.isLightOn) {
                    float dx = prop.worldX - camPos.x;
                    float dz = prop.worldZ - camPos.z;
                    float d = std::sqrt(dx * dx + dz * dz);

                    if (d < MAX_LAMP_AFFECT_DIST) {
                        bool isBlocked = false;
                        int steps = 10;
                        for (int s = 1; s < steps; ++s) {
                            float cx = (camPos.x + dx * (static_cast<float>(s) / steps)) / BLOCK_SIZE;
                            float cz = (camPos.z + dz * (static_cast<float>(s) / steps)) / BLOCK_SIZE;
                            int gx = static_cast<int>(cx);
                            int gy = dungeon.GetHeight() - 1 - static_cast<int>(cz);

                            if (dungeon.IsWall(gx, gy)) {
                                isBlocked = true;
                                break;
                            }
                        }
                        if (!isBlocked) {
                            visibleLamps.push_back({ VGet(prop.worldX, BLOCK_SIZE * 0.82f, prop.worldZ), d });
                        }
                    }
                }
            }

            std::sort(visibleLamps.begin(), visibleLamps.end(), [](const VisibleLamp& a, const VisibleLamp& b) {
                return a.dist < b.dist;
                });

            for (int i = 0; i < MAX_CEILING_LIGHTS; ++i) {
                if (i < static_cast<int>(visibleLamps.size())) {
                    float fade = 1.0f - (visibleLamps[i].dist / MAX_LAMP_AFFECT_DIST);
                    fade = std::clamp(fade, 0.0f, 1.0f);
                    COLOR_F lightCol = GetColorF(0.85f * fade, 0.92f * fade, 1.0f * fade, 1.0f);

                    SetLightEnableHandle(ceilingLights[i], TRUE);
                    SetLightPositionHandle(ceilingLights[i], visibleLamps[i].pos);
                    SetLightDifColorHandle(ceilingLights[i], lightCol);
                }
                else {
                    SetLightEnableHandle(ceilingLights[i], FALSE);
                }
            }

            for (int y = 0; y < dungeon.GetHeight(); ++y) {
                for (int x = 0; x < dungeon.GetWidth(); ++x) {
                    int flippedY = dungeon.GetHeight() - 1 - y;
                    VECTOR minPos = VGet(x * BLOCK_SIZE, 0.0f, flippedY * BLOCK_SIZE);
                    VECTOR maxPos = VGet((x + 1) * BLOCK_SIZE, BLOCK_SIZE, (flippedY + 1) * BLOCK_SIZE);

                    int tile = dungeon.GetTile(x, y);
                    if (tile == static_cast<int>(TileType::Wall)) {
                        DrawCube3D(minPos, maxPos, GetColor(50, 55, 70), GetColor(25, 28, 38), TRUE);
                    }
                    else if (tile == static_cast<int>(TileType::Goal)) {
                        VECTOR goalMax = VGet((x + 1) * BLOCK_SIZE, BLOCK_SIZE * 0.2f, (flippedY + 1) * BLOCK_SIZE);
                        DrawCube3D(minPos, goalMax, GetColor(180, 140, 30), GetColor(60, 50, 10), TRUE);
                    }
                }
            }

            PropRenderer::DrawProps(props);
            enemyManager.Draw3D(dungeon);
            PropRenderer::DrawHandModel(player, camPos, lookDir, rightDir, upDir, isLanternOn, lanternOil, isMatchLit);

            SetUseZBuffer3D(FALSE);

            // 照準表示
            int cx = SCREEN_WIDTH / 2;
            int cy = SCREEN_HEIGHT / 2;
            int chColor = GetColor(180, 180, 180);

            int targetedIndex = -1;
            float minRayDist = 999.0f;
            for (size_t i = 0; i < props.size(); ++i) {
                if (props[i].isCollected || props[i].type == PropType::CeilingLamp) continue;
                VECTOR toProp = VGet(props[i].worldX - camPos.x, props[i].worldY - camPos.y, props[i].worldZ - camPos.z);
                float t = toProp.x * lookDir.x + toProp.y * lookDir.y + toProp.z * lookDir.z;
                if (t > 12.0f && t < 120.0f) {
                    VECTOR closestPoint = VGet(camPos.x + lookDir.x * t, camPos.y + lookDir.y * t, camPos.z + lookDir.z * t);
                    float diffX = props[i].worldX - closestPoint.x;
                    float diffY = props[i].worldY - closestPoint.y;
                    float diffZ = props[i].worldZ - closestPoint.z;
                    float rayDist = std::sqrt(diffX * diffX + diffY * diffY + diffZ * diffZ);

                    float effectiveRadius = (props[i].type == PropType::Desk) ? 24.0f :
                        (props[i].type == PropType::Crate) ? 16.0f : 12.0f;

                    if (rayDist <= effectiveRadius && rayDist < minRayDist) {
                        minRayDist = rayDist;
                        targetedIndex = static_cast<int>(i);
                    }
                }
            }

            const char* targetAction = nullptr;
            if (targetedIndex != -1) {
                chColor = GetColor(255, 215, 0);
                if (props[targetedIndex].type == PropType::Desk) {
                    if (player.GetActiveHandItem() == ItemId::Shotgun) {
                        targetAction = "作業台で古びたショットガンを整備する ";
                    }
                    else if (!props[targetedIndex].isSearched) {
                        targetAction = "古い作業台を調べる ";
                    }
                    else {
                        targetAction = "古い作業台 (銃を構えて整備可能) ";
                    }
                }
                else {
                    targetAction = props[targetedIndex].name;
                }
            }

            DrawCircle(cx, cy, 1, chColor, TRUE);
            DrawLine(cx - 6, cy, cx - 2, cy, chColor);
            DrawLine(cx + 2, cy, cx + 6, cy, chColor);
            DrawLine(cx, cy - 6, cx, cy - 2, chColor);
            DrawLine(cx, cy + 2, cx, cy + 6, chColor);

            if (targetAction) {
                DrawFormatString(cx - 60, cy + 12, chColor, "[E / Click] %s", targetAction);
            }
            if (messageTimer > 0) {
                DrawFormatString(cx - 100, cy + 32, GetColor(100, 255, 120), "%s", messageText);
            }

            if (isDevMode) {
                constexpr int MINI_TILE = 4;
                for (int y = 0; y < dungeon.GetHeight(); ++y) {
                    for (int x = 0; x < dungeon.GetWidth(); ++x) {
                        int tile = dungeon.GetTile(x, y);
                        int color = GetColor(30, 30, 30);
                        if (tile == static_cast<int>(TileType::Wall))  color = GetColor(150, 150, 150);
                        if (tile == static_cast<int>(TileType::Goal))  color = GetColor(255, 215, 0);
                        if (tile == static_cast<int>(TileType::Start)) color = GetColor(50, 200, 50);

                        DrawBox(10 + x * MINI_TILE, 10 + y * MINI_TILE,
                            10 + (x + 1) * MINI_TILE, 10 + (y + 1) * MINI_TILE, color, TRUE);
                    }
                }
                int px = 10 + static_cast<int>(player.posX * MINI_TILE) + MINI_TILE / 2;
                int py = 10 + static_cast<int>(player.posY * MINI_TILE) + MINI_TILE / 2;
                DrawCircle(px, py, 2, GetColor(255, 50, 50), TRUE);
                DrawFormatString(10, 10 + dungeon.GetHeight() * MINI_TILE + 5, GetColor(255, 100, 100), "[DEV MODE ON]");
            }

            if (menuState == MenuState::MainMenu) {
                int winW = 320;
                int winH = 300;
                int winX = (SCREEN_WIDTH - winW) / 2;
                int winY = (SCREEN_HEIGHT - winH) / 2;

                SetDrawBlendMode(DX_BLENDMODE_ALPHA, 230);
                DrawBox(winX, winY, winX + winW, winY + winH, GetColor(20, 20, 30), TRUE);
                SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

                DrawBox(winX, winY, winX + winW, winY + winH, GetColor(200, 180, 120), FALSE);
                DrawFormatString(winX + 20, winY + 18, GetColor(255, 230, 150), "== MENU ==");

                for (int i = 0; i < MENU_ITEM_COUNT; ++i) {
                    int itemY = winY + 48 + i * 44;
                    bool isCursor = (i == menuCursor);

                    int itemBg = isCursor ? GetColor(50, 70, 120) : GetColor(25, 30, 45);
                    int itemBorder = isCursor ? GetColor(255, 215, 0) : GetColor(60, 60, 80);

                    DrawBox(winX + 20, itemY, winX + winW - 20, itemY + 36, itemBg, TRUE);
                    DrawBox(winX + 20, itemY, winX + winW - 20, itemY + 36, itemBorder, FALSE);

                    int textColor = isCursor ? GetColor(255, 255, 255) : GetColor(180, 180, 180);
                    DrawFormatString(winX + 35, itemY + 10, textColor, menuItems[i]);
                }
            }
            // インベントリ ＆ ステータス画面
            else if (menuState == MenuState::InventoryViewer) {
                int winW = 480;
                int winH = 390;
                int winX = (SCREEN_WIDTH - winW) / 2;
                int winY = (SCREEN_HEIGHT - winH) / 2;

                SetDrawBlendMode(DX_BLENDMODE_ALPHA, 235);
                DrawBox(winX, winY, winX + winW, winY + winH, GetColor(20, 20, 28), TRUE);
                SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
                DrawBox(winX, winY, winX + winW, winY + winH, GetColor(200, 180, 120), FALSE);

                DrawFormatString(winX + 20, winY + 14, GetColor(255, 230, 150), "== INVENTORY & STATUS ==");

                const char* hpCondition = "良好 ";
                int hpColor = GetColor(100, 255, 100);
                if (player.hp <= 30) {
                    hpCondition = "重傷 (強い鈍足・視界狭窄) ";
                    hpColor = GetColor(255, 60, 60);
                }
                else if (player.hp <= 60) {
                    hpCondition = "軽傷 (かすかな鈍足) ";
                    hpColor = GetColor(255, 200, 50);
                }
                DrawFormatString(winX + 20, winY + 38, hpColor, "身体状態: %s", hpCondition);

                auto GetHandName = [](ItemId id) -> const char* {
                    if (id == ItemId::Lantern) return "ランタン ";
                    if (id == ItemId::Match) return "マッチ ";
                    if (id == ItemId::Shotgun) return "古びたショットガン ";
                    return "なし ";
                    };
                DrawFormatString(winX + 20, winY + 58, GetColor(200, 200, 220),
                    "[枠1]: %s%s  [枠2]: %s%s  (Q: 切替)",
                    GetHandName(player.handSlots[0]), (player.activeHandIndex == 0 ? "★" : " "),
                    GetHandName(player.handSlots[1]), (player.activeHandIndex == 1 ? "★" : " "));

                DrawFormatString(winX + 20, winY + 80, GetColor(255, 215, 120), "OIL GAUGE: %.1f%%", lanternOil);
                int barX = winX + 20;
                int barY = winY + 98;
                int barW = 440;
                int barH = 10;
                DrawBox(barX, barY, barX + barW, barY + barH, GetColor(40, 40, 50), TRUE);
                int fillW = static_cast<int>(barW * (lanternOil / MAX_OIL));
                int gaugeColor = (lanternOil > 25.0f) ? GetColor(255, 180, 50) : GetColor(255, 60, 60);
                DrawBox(barX, barY, barX + fillW, barY + barH, gaugeColor, TRUE);
                DrawBox(barX, barY, barX + barW, barY + barH, GetColor(150, 150, 150), FALSE);

                DrawLine(winX + 15, winY + 116, winX + winW - 15, winY + 116, GetColor(80, 80, 100));

                if (invScroll > 0) {
                    DrawFormatString(winX + winW / 2 - 25, winY + 120, GetColor(255, 215, 0), "▲ MORE ▲");
                }
                else {
                    DrawFormatString(winX + 20, winY + 120, GetColor(220, 220, 240), "[所持品一覧]");
                }

                if (inventory.empty()) {
                    DrawFormatString(winX + 30, winY + 150, GetColor(130, 130, 140), "- 所持品はありません -");
                }
                else {
                    int drawCount = std::min(INV_PAGE_ITEMS, static_cast<int>(inventory.size()) - invScroll);
                    for (int i = 0; i < drawCount; ++i) {
                        int actualIdx = invScroll + i;
                        int itemY = winY + 138 + i * 26;
                        bool isSel = (actualIdx == invCursor);

                        if (isSel) {
                            DrawBox(winX + 20, itemY - 2, winX + winW - 20, itemY + 22, GetColor(45, 65, 110), TRUE);
                            DrawBox(winX + 20, itemY - 2, winX + winW - 20, itemY + 22, GetColor(255, 215, 0), FALSE);
                        }

                        int textColor = isSel ? GetColor(255, 255, 255) : GetColor(180, 180, 180);
                        if (inventory[actualIdx].id == ItemId::Match) {
                            DrawFormatString(winX + 30, itemY + 2, textColor, "%s", inventory[actualIdx].name.c_str());
                        }
                        else if (inventory[actualIdx].id == ItemId::Shotgun) {
                            DrawFormatString(winX + 30, itemY + 2, textColor, "%s (損耗: %.0f%% / 応急残: %d回)",
                                inventory[actualIdx].name.c_str(),
                                100.0f - inventory[actualIdx].durability,
                                inventory[actualIdx].uses);
                        }
                        else {
                            DrawFormatString(winX + 30, itemY + 2, textColor, "%s x%d", inventory[actualIdx].name.c_str(), inventory[actualIdx].count);
                        }
                    }

                    if (invScroll + INV_PAGE_ITEMS < static_cast<int>(inventory.size())) {
                        DrawFormatString(winX + winW / 2 - 25, winY + 270, GetColor(255, 215, 0), "▼ MORE ▼");
                    }

                    DrawLine(winX + 15, winY + 288, winX + winW - 15, winY + 288, GetColor(80, 80, 100));

                    auto descLines = WrapAndSplitText(inventory[invCursor].desc, 23);
                    int totalDescLines = static_cast<int>(descLines.size());

                    DrawFormatString(winX + 20, winY + 294, GetColor(100, 220, 255), "説明:");
                    if (totalDescLines > DESC_PAGE_LINES) {
                        DrawFormatString(winX + 70, winY + 294, GetColor(255, 215, 0), "[A/D or ←/→ でスクロール %d/%d]", descScroll + 1, totalDescLines);
                    }

                    for (int l = 0; l < DESC_PAGE_LINES; ++l) {
                        int lineIdx = descScroll + l;
                        if (lineIdx < totalDescLines) {
                            DrawFormatString(winX + 20, winY + 312 + l * 18, GetColor(200, 200, 200), "%s", descLines[lineIdx].c_str());
                        }
                    }

                    DrawFormatString(winX + 20, winY + 368, GetColor(255, 215, 100), "[ENTER]: 使用/読む  [1]: 枠1装備  [2]: 枠2装備");
                }

                if (!diaryPopupText.empty()) {
                    int popW = 440;
                    int popH = 260;
                    int popX = (SCREEN_WIDTH - popW) / 2;
                    int popY = (SCREEN_HEIGHT - popH) / 2;

                    DrawBox(popX, popY, popX + popW, popY + popH, GetColor(30, 25, 20), TRUE);
                    DrawBox(popX, popY, popX + popW, popY + popH, GetColor(255, 220, 130), FALSE);

                    auto popLines = WrapAndSplitText(diaryPopupText, 22);
                    int totalPopLines = static_cast<int>(popLines.size());

                    if (totalPopLines > POPUP_PAGE_LINES) {
                        DrawFormatString(popX + popW - 140, popY + 12, GetColor(255, 215, 0), "[W/S: スクロール]");
                    }

                    for (int l = 0; l < POPUP_PAGE_LINES; ++l) {
                        int lineIdx = popupScroll + l;
                        if (lineIdx < totalPopLines) {
                            DrawFormatString(popX + 15, popY + 15 + l * 24, GetColor(255, 240, 180), "%s", popLines[lineIdx].c_str());
                        }
                    }

                    DrawFormatString(popX + 15, popY + popH - 24, GetColor(150, 255, 150), "[ESC / ENTER]: 閉じる");
                }
            }
            else if (menuState == MenuState::MapViewer) {
                int winW = 340;
                int winH = 340;
                int winX = (SCREEN_WIDTH - winW) / 2;
                int winY = (SCREEN_HEIGHT - winH) / 2;

                SetDrawBlendMode(DX_BLENDMODE_ALPHA, 235);
                DrawBox(winX, winY, winX + winW, winY + winH, GetColor(20, 20, 25), TRUE);
                SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

                DrawBox(winX, winY, winX + winW, winY + winH, GetColor(200, 180, 120), FALSE);
                DrawFormatString(winX + 15, winY + 12, GetColor(255, 230, 150), "MAP - B%dF (ESC / ENTER to Back)", dungeon.GetCurrentFloor());

                int mapPixelArea = winW - 50;
                int tileSize = mapPixelArea / dungeon.GetWidth();
                if (tileSize < 4) tileSize = 4;
                if (tileSize > 14) tileSize = 14;

                int offsetX = winX + (winW - dungeon.GetWidth() * tileSize) / 2;
                int offsetY = winY + 40 + (winH - 50 - dungeon.GetHeight() * tileSize) / 2;

                for (int y = 0; y < dungeon.GetHeight(); ++y) {
                    for (int x = 0; x < dungeon.GetWidth(); ++x) {
                        int tx = offsetX + x * tileSize;
                        int ty = offsetY + y * tileSize;

                        if (dungeon.IsVisited(x, y)) {
                            int tile = dungeon.GetTile(x, y);
                            int color = GetColor(50, 50, 60);
                            if (tile == static_cast<int>(TileType::Wall))  color = GetColor(160, 160, 170);
                            if (tile == static_cast<int>(TileType::Goal))  color = GetColor(255, 215, 0);
                            if (tile == static_cast<int>(TileType::Start)) color = GetColor(60, 180, 60);

                            DrawBox(tx, ty, tx + tileSize, ty + tileSize, color, TRUE);
                        }
                        else {
                            DrawBox(tx, ty, tx + tileSize, ty + tileSize, GetColor(30, 30, 35), FALSE);
                        }
                    }
                }

                int ppx = offsetX + static_cast<int>(player.posX * tileSize) + tileSize / 2;
                int ppy = offsetY + static_cast<int>(player.posY * tileSize) + tileSize / 2;
                DrawCircle(ppx, ppy, tileSize / 3 + 1, GetColor(255, 50, 50), TRUE);
                DrawLine(ppx, ppy,
                    ppx + static_cast<int>(std::sin(player.yaw) * (tileSize + 2)),
                    ppy - static_cast<int>(std::cos(player.yaw) * (tileSize + 2)),
                    GetColor(255, 220, 0));
            }

            // HUD (オイル残量を隠し、シンプルに整理)
            DrawFormatString(10, SCREEN_HEIGHT - 40, GetColor(220, 220, 220),
                "Floor: B%dF  [TAB]: Menu/Status  [Q]: 構え切替  [L]: 点火/アクション",
                dungeon.GetCurrentFloor());
            DrawFormatString(10, SCREEN_HEIGHT - 20, GetColor(160, 160, 160),
                "[WASD]: 移動  [Mouse]: 視点  [E/Click]: 調べる  [Space]: 発砲");

            if (autoSaveTimer > 0) {
                DrawFormatString(SCREEN_WIDTH - 240, 20, GetColor(100, 255, 100), "[AUTO SAVED]");
            }
        }

        ScreenFlip();
    }

    DeleteLightHandle(lanternLight);
    DeleteLightHandle(matchLight);
    for (int i = 0; i < MAX_CEILING_LIGHTS; ++i) {
        DeleteLightHandle(ceilingLights[i]);
    }
    DxLib_End();
    return 0;
}