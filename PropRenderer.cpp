#include "PropRenderer.h"
#include "Player.h"
#include <cmath>

void PropRenderer::DrawProps(const std::vector<Prop>& props) {
    for (const auto& prop : props) {
        if (prop.isCollected) continue;
        float wx = prop.worldX;
        float wy = prop.worldY;
        float wz = prop.worldZ;

        if (prop.type == PropType::CeilingLamp) {
            int col = prop.isLightOn ? GetColor(255, 245, 180) : GetColor(35, 35, 35);
            DrawCube3D(VGet(wx - 8.0f, BLOCK_SIZE - 4.0f, wz - 8.0f), VGet(wx + 8.0f, BLOCK_SIZE, wz + 8.0f), col, GetColor(20, 20, 20), TRUE);
        }
        else if (prop.type == PropType::Desk) {
            DrawCube3D(VGet(wx - 8.0f, 18.0f, wz - 5.0f), VGet(wx + 8.0f, 20.0f, wz + 5.0f), GetColor(85, 55, 30), GetColor(40, 25, 15), TRUE);
            DrawCube3D(VGet(wx - 7.0f, 0.0f, wz - 4.0f), VGet(wx - 5.0f, 18.0f, wz - 2.0f), GetColor(60, 40, 20), GetColor(20, 10, 5), TRUE);
            DrawCube3D(VGet(wx + 5.0f, 0.0f, wz + 2.0f), VGet(wx + 7.0f, 18.0f, wz + 4.0f), GetColor(60, 40, 20), GetColor(20, 10, 5), TRUE);
            DrawCube3D(VGet(wx - 7.0f, 0.0f, wz + 2.0f), VGet(wx - 5.0f, 18.0f, wz + 4.0f), GetColor(60, 40, 20), GetColor(20, 10, 5), TRUE);
            DrawCube3D(VGet(wx + 5.0f, 0.0f, wz - 4.0f), VGet(wx + 7.0f, 18.0f, wz - 2.0f), GetColor(60, 40, 20), GetColor(20, 10, 5), TRUE);
        }
        else if (prop.type == PropType::Crate) {
            DrawCube3D(VGet(wx - 4.0f, wy - 4.0f, wz - 4.0f), VGet(wx + 4.0f, wy + 4.0f, wz + 4.0f), GetColor(110, 75, 45), GetColor(45, 30, 15), TRUE);
        }
        else if (prop.type == PropType::SmallItem) {
            DrawCube3D(VGet(wx - 3.0f, wy, wz - 3.0f), VGet(wx + 3.0f, wy + 3.0f, wz + 3.0f), GetColor(210, 190, 130), GetColor(100, 80, 40), TRUE);
        }
    }
}

void PropRenderer::DrawHandModel(const Player& player, VECTOR camPos, VECTOR lookDir, VECTOR rightDir, VECTOR upDir,
    bool isLanternOn, float lanternOil, bool isMatchLit) {
    ItemId cur = player.GetActiveHandItem();
    if (cur == ItemId::None) return;

    SetWriteZBuffer3D(FALSE);
    float bobY = player.GetCameraBobY() * 0.35f;

    // 1. 携帯ランタン
    if (cur == ItemId::Lantern) {
        VECTOR lPos = VGet(
            camPos.x + lookDir.x * 16.0f + rightDir.x * 7.5f,
            camPos.y + lookDir.y * 16.0f - 5.5f + bobY,
            camPos.z + lookDir.z * 16.0f + rightDir.z * 7.5f
        );

        DrawCube3D(VGet(lPos.x - 1.4f, lPos.y - 2.2f, lPos.z - 1.4f), VGet(lPos.x + 1.4f, lPos.y - 1.8f, lPos.z + 1.4f), GetColor(50, 50, 55), GetColor(20, 20, 20), TRUE);
        int glassCol = (isLanternOn && lanternOil > 0.0f) ? GetColor(255, 215, 100) : GetColor(40, 35, 30);
        DrawCube3D(VGet(lPos.x - 1.1f, lPos.y - 1.8f, lPos.z - 1.1f), VGet(lPos.x + 1.1f, lPos.y + 0.8f, lPos.z + 1.1f), glassCol, GetColor(30, 25, 20), TRUE);
        DrawCube3D(VGet(lPos.x - 1.5f, lPos.y + 0.8f, lPos.z - 1.5f), VGet(lPos.x + 1.5f, lPos.y + 1.3f, lPos.z + 1.5f), GetColor(65, 60, 60), GetColor(25, 25, 25), TRUE);
        DrawCube3D(VGet(lPos.x - 0.2f, lPos.y + 1.3f, lPos.z - 0.2f), VGet(lPos.x + 0.2f, lPos.y + 2.0f, lPos.z + 0.2f), GetColor(120, 120, 130), GetColor(40, 40, 40), TRUE);
    }
    // 2. マッチ
    else if (cur == ItemId::Match) {
        VECTOR mPos = VGet(
            camPos.x + lookDir.x * 14.0f + rightDir.x * 6.5f,
            camPos.y + lookDir.y * 14.0f - 4.5f + bobY,
            camPos.z + lookDir.z * 14.0f + rightDir.z * 6.5f
        );

        DrawCube3D(VGet(mPos.x - 0.15f, mPos.y - 2.5f, mPos.z - 0.15f), VGet(mPos.x + 0.15f, mPos.y + 0.5f, mPos.z + 0.15f), GetColor(180, 150, 100), GetColor(70, 50, 30), TRUE);
        if (isMatchLit) {
            DrawCube3D(VGet(mPos.x - 0.4f, mPos.y + 0.5f, mPos.z - 0.4f), VGet(mPos.x + 0.4f, mPos.y + 1.4f, mPos.z + 0.4f), GetColor(255, 120, 30), GetColor(255, 240, 100), TRUE);
        }
        else {
            DrawCube3D(VGet(mPos.x - 0.25f, mPos.y + 0.5f, mPos.z - 0.25f), VGet(mPos.x + 0.25f, mPos.y + 0.8f, mPos.z + 0.25f), GetColor(160, 40, 30), GetColor(60, 20, 20), TRUE);
        }
    }
    // 3. 本格3D古びたショットガン (ドット絵感を排除した幾何造形)
    else if (cur == ItemId::Shotgun) {
        VECTOR gBase = VGet(
            camPos.x + lookDir.x * 13.0f + rightDir.x * 5.0f,
            camPos.y + lookDir.y * 13.0f - 4.0f + bobY,
            camPos.z + lookDir.z * 13.0f + rightDir.z * 5.0f
        );

        // レシーバー（機関部）
        DrawCube3D(VGet(gBase.x - 0.6f, gBase.y - 0.9f, gBase.z - 0.6f),
            VGet(gBase.x + 0.6f, gBase.y + 0.7f, gBase.z + 0.6f),
            GetColor(40, 40, 45), GetColor(18, 18, 20), TRUE);

        // ロングスチールバレル (前方に伸びる一体型銃身)
        for (int i = 1; i <= 8; ++i) {
            float dist = i * 1.6f;
            VECTOR bPos = VGet(gBase.x + lookDir.x * dist, gBase.y + lookDir.y * dist + 0.35f, gBase.z + lookDir.z * dist);
            DrawCube3D(VGet(bPos.x - 0.32f, bPos.y - 0.32f, bPos.z - 0.32f),
                VGet(bPos.x + 0.32f, bPos.y + 0.32f, bPos.z + 0.32f),
                GetColor(50, 50, 55), GetColor(20, 20, 22), TRUE);
        }

        // アンダーチューブ (装弾パイプ)
        for (int i = 1; i <= 6; ++i) {
            float dist = i * 1.6f;
            VECTOR mPos = VGet(gBase.x + lookDir.x * dist, gBase.y + lookDir.y * dist - 0.35f, gBase.z + lookDir.z * dist);
            DrawCube3D(VGet(mPos.x - 0.28f, mPos.y - 0.28f, mPos.z - 0.28f),
                VGet(mPos.x + 0.28f, mPos.y + 0.28f, mPos.z + 0.28f),
                GetColor(38, 38, 42), GetColor(15, 15, 18), TRUE);
        }

        // 木製フォアエンド (ハンドガード)
        for (int i = 2; i <= 4; ++i) {
            float dist = i * 1.6f;
            VECTOR fPos = VGet(gBase.x + lookDir.x * dist, gBase.y + lookDir.y * dist - 0.35f, gBase.z + lookDir.z * dist);
            DrawCube3D(VGet(fPos.x - 0.55f, fPos.y - 0.55f, fPos.z - 0.55f),
                VGet(fPos.x + 0.55f, fPos.y + 0.45f, fPos.z + 0.55f),
                GetColor(115, 68, 32), GetColor(45, 25, 10), TRUE);
        }

        // ウッドストック (手前へ伸びる木製銃床)
        for (int i = 1; i <= 4; ++i) {
            float dist = -i * 1.3f;
            VECTOR sPos = VGet(gBase.x + lookDir.x * dist, gBase.y + lookDir.y * dist - 0.3f * i, gBase.z + lookDir.z * dist);
            DrawCube3D(VGet(sPos.x - 0.5f, sPos.y - 0.8f, sPos.z - 0.5f),
                VGet(sPos.x + 0.5f, sPos.y + 0.5f, sPos.z + 0.5f),
                GetColor(105, 62, 28), GetColor(40, 22, 10), TRUE);
        }
    }

    SetWriteZBuffer3D(TRUE);
}