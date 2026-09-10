#include "MazeGenerator.h"
#include <random>
#include <algorithm>
#include <queue>

MazeResult MazeGenerator::Generate(int width, int height, unsigned int seed) {
    MazeResult result;
    // 全体を壁で初期化
    result.grid.assign(height, std::vector<int>(width, static_cast<int>(TileType::Wall)));

    std::mt19937 rng(seed);

    // ----------------------------------------------------------------
    // 1. 改善版 穴掘り法 (候補リストからランダム抽出して一本道を防止)
    // ----------------------------------------------------------------
    result.startX = 1;
    result.startY = 1;
    result.grid[result.startY][result.startX] = static_cast<int>(TileType::Path);

    // 掘削の起点候補リスト
    std::vector<std::pair<int, int>> candidates;
    candidates.push_back({ result.startX, result.startY });

    const int dx[4] = { 0, 0, -2, 2 };
    const int dy[4] = { -2, 2, 0, 0 };

    while (!candidates.empty()) {
        // 70%の確率で過去の掘削点からランダムに分岐させ、30%は直近から掘る
        int idx = (rng() % 10 < 7) ? (rng() % candidates.size()) : (candidates.size() - 1);
        auto [cx, cy] = candidates[idx];

        std::vector<int> dirs = { 0, 1, 2, 3 };
        std::shuffle(dirs.begin(), dirs.end(), rng);

        bool dug = false;
        for (int dir : dirs) {
            int nx = cx + dx[dir];
            int ny = cy + dy[dir];

            // 外壁を残しつつ、2マス先が壁なら通路を掘る
            if (nx > 0 && nx < width - 1 && ny > 0 && ny < height - 1) {
                if (result.grid[ny][nx] == static_cast<int>(TileType::Wall)) {
                    result.grid[cy + dy[dir] / 2][cx + dx[dir] / 2] = static_cast<int>(TileType::Path);
                    result.grid[ny][nx] = static_cast<int>(TileType::Path);
                    candidates.push_back({ nx, ny });
                    dug = true;
                    break;
                }
            }
        }

        // 周囲4方向にこれ以上掘れる壁がないセルは候補から除外
        if (!dug) {
            candidates.erase(candidates.begin() + idx);
        }
    }

    // ----------------------------------------------------------------
    // 2. ループ路の追加 (適度に壁を抜いて回遊性を持たせる)
    // ----------------------------------------------------------------
    // マップ面積に応じて数カ所の内壁を壊す
    int removeWallCount = (width * height) / 35;
    for (int i = 0; i < removeWallCount; ++i) {
        int rx = 1 + (rng() % (width - 2));
        int ry = 1 + (rng() % (height - 2));
        // 外壁以外を通路に変更
        result.grid[ry][rx] = static_cast<int>(TileType::Path);
    }

    // ----------------------------------------------------------------
    // 3. 幅優先探索 (BFS) でスタートから最も遠い地点をゴールに設定
    // ----------------------------------------------------------------
    std::vector<std::vector<int>> dist(height, std::vector<int>(width, -1));
    std::queue<std::pair<int, int>> q;

    q.push({ result.startX, result.startY });
    dist[result.startY][result.startX] = 0;

    int maxDist = 0;
    result.goalX = result.startX;
    result.goalY = result.startY;

    const int adx[4] = { 0, 1, 0, -1 };
    const int ady[4] = { -1, 0, 1, 0 };

    while (!q.empty()) {
        auto [cx, cy] = q.front();
        q.pop();

        for (int i = 0; i < 4; ++i) {
            int nx = cx + adx[i];
            int ny = cy + ady[i];

            if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                if (result.grid[ny][nx] != static_cast<int>(TileType::Wall) && dist[ny][nx] == -1) {
                    dist[ny][nx] = dist[cy][cx] + 1;
                    if (dist[ny][nx] > maxDist) {
                        maxDist = dist[ny][nx];
                        result.goalX = nx;
                        result.goalY = ny;
                    }
                    q.push({ nx, ny });
                }
            }
        }
    }

    result.grid[result.startY][result.startX] = static_cast<int>(TileType::Start);
    result.grid[result.goalY][result.goalX] = static_cast<int>(TileType::Goal);

    return result;
}