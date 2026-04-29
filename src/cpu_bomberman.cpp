#include "cpu_bomberman.hpp"

#include "bomb.hpp"
#include "game_map.hpp"
#include "sprite_atlas.hpp"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <queue>
#include <random>
#include <vector>

// Símbolos globales definidos en bomberman.cpp usados por el Agent.
extern GLuint uniformModel;
extern GLuint uniformUvRect;
extern GLuint uniformFlipX;
extern GLuint uniformTintColor;
extern SpriteAtlas gPlayerAtlas;
extern GLuint texture;
extern GLuint enemyTexture;
extern std::vector<Enemy*> gEnemies;

namespace CpuBomberman {

struct CpuState {
    Move currentMove = MOVE_NONE;
    float moveLockSeconds = 0.0f;

    float bombCooldownSeconds = 0.0f;
};

static CpuState gCpuStateByPlayerId[4];

static std::mt19937& rng()
{
    static std::mt19937 gen{std::random_device{}()};
    return gen;
}

static float rand01()
{
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(rng());
}

static int firstCpuIndex(GameMode mode)
{
    // VS 1P: players[0] es humano => CPU desde 1.
    // VS 2P: players[0] y players[1] son humanos => CPU desde 2.
    if (mode == GameMode::VsTwoPlayers) return 2;
    return 1;
}

static Difficulty difficultyFor(const Settings& settings, int playerId)
{
    if (playerId < 0 || playerId >= 4) return Difficulty::Medium;
    return settings.difficultyByPlayerId[(size_t)playerId];
}

static GLint moveToFacingDirKey(Move mov)
{
    switch (mov) {
        case MOVE_UP: return GLFW_KEY_UP;
        case MOVE_DOWN: return GLFW_KEY_DOWN;
        case MOVE_LEFT: return GLFW_KEY_LEFT;
        case MOVE_RIGHT: return GLFW_KEY_RIGHT;
        default: return GLFW_KEY_DOWN;
    }
}

static bool bombBlocksCellForPlayer(int row, int col, int playerId)
{
    for (auto* b : gBombs) {
        if (!b) continue;
        if (b->state == BombState::DONE) continue;
        if (b->gridRow == row && b->gridCol == col) {
            return b->blocksForPlayer(playerId);
        }
    }
    return false;
}

static bool cellHasAnyBomb(int row, int col)
{
    for (auto* b : gBombs) {
        if (!b) continue;
        if (b->state == BombState::DONE) continue;
        if (b->gridRow == row && b->gridCol == col) return true;
    }
    return false;
}

static void markBlastArea(const GameMap& map,
                          int bombRow,
                          int bombCol,
                          int power,
                          std::vector<uint8_t>& outMask,
                          int rows,
                          int cols)
{
    auto mark = [&](int r, int c) {
        if (r < 0 || c < 0 || r >= rows || c >= cols) return;
        outMask[(size_t)r * (size_t)cols + (size_t)c] = 1;
    };

    mark(bombRow, bombCol);

    const int dr[4] = {0, -1, 0, 1};
    const int dc[4] = {1, 0, -1, 0};

    for (int i = 0; i < 4; ++i) {
        for (int d = 1; d <= power; ++d) {
            const int r = bombRow + dr[i] * d;
            const int c = bombCol + dc[i] * d;

            if (r < 0 || c < 0 || r >= rows || c >= cols) break;
            if (!map.isWalkable(r, c)) break;

            mark(r, c);

            // Si la siguiente celda es muro, visualmente sería end; en lógica, igual corta aquí.
            const int nr = r + dr[i];
            const int nc = c + dc[i];
            if (nr < 0 || nc < 0 || nr >= rows || nc >= cols) break;
            if (!map.isWalkable(nr, nc)) break;
        }
    }
}

static std::vector<uint8_t> buildDangerMask(const GameMap& map, Difficulty awareness)
{
    const int rows = map.getRows();
    const int cols = map.getCols();
    std::vector<uint8_t> danger((size_t)rows * (size_t)cols, 0);

    if (awareness == Difficulty::Easy) {
        return danger;
    }

    // Peligro “pronto”:
    // - Bombas EXPLODING: siempre.
    // - Bombas FUSE a punto de explotar: ventana para que la CPU pueda reaccionar.
    // - Bombas remotas: se consideran peligrosas mientras existan.
    static constexpr float kImminentSeconds = 1.25f;

    for (auto* b : gBombs) {
        if (!b) continue;
        if (b->state == BombState::DONE) continue;

        const bool exploding = (b->state == BombState::EXPLODING);
        const bool imminentFuse =
            (b->state == BombState::FUSE && !b->remoteControlled &&
             (b->fuseTime - b->fuseTimer) <= kImminentSeconds);
        const bool remoteBomb = b->remoteControlled;

        if (!exploding && !imminentFuse && !remoteBomb) continue;

        const int effectivePower = (awareness == Difficulty::Hard)
            ? b->power
            : std::min(2, b->power);

        markBlastArea(map, b->gridRow, b->gridCol, effectivePower, danger, rows, cols);
    }

    return danger;
}

static bool isDangerousCell(const std::vector<uint8_t>& danger, int row, int col, int cols)
{
    if (row < 0 || col < 0) return true;
    const size_t idx = (size_t)row * (size_t)cols + (size_t)col;
    if (idx >= danger.size()) return true;
    return danger[idx] != 0;
}

static std::vector<Move> validMoves(const GameMap& map,
                                   const std::vector<uint8_t>& danger,
                                   const Player& self,
                                   bool avoidDanger)
{
    int sr = 0, sc = 0;
    map.ndcToGrid(self.position, sr, sc);

    struct Cand { Move m; int r; int c; };
    const Cand cands[] = {
        {MOVE_UP, sr - 1, sc},
        {MOVE_DOWN, sr + 1, sc},
        {MOVE_LEFT, sr, sc - 1},
        {MOVE_RIGHT, sr, sc + 1}
    };

    std::vector<Move> out;
    out.reserve(4);

    const int rows = map.getRows();
    const int cols = map.getCols();

    for (const auto& c : cands) {
        if (c.r < 0 || c.c < 0 || c.r >= rows || c.c >= cols) continue;
        if (!map.isWalkable(c.r, c.c)) continue;
        if (bombBlocksCellForPlayer(c.r, c.c, self.playerId)) continue;
        if (avoidDanger && isDangerousCell(danger, c.r, c.c, cols)) continue;
        out.push_back(c.m);
    }

    return out;
}

static Move pickRandomMove(const std::vector<Move>& moves)
{
    if (moves.empty()) return MOVE_NONE;
    std::uniform_int_distribution<int> dist(0, (int)moves.size() - 1);
    return moves[(size_t)dist(rng())];
}

static void snapPlayerToTileCenter(Player& self, const GameMap& map)
{
    int r = 0, c = 0;
    map.ndcToGrid(self.position, r, c);
    self.position = map.gridToNDC(r, c);
}

static void cpuContinueTileMove(Player& self, const GameMap& map, CpuState& st, float deltaTime)
{
    // Si el target deja de ser válido (muro o bomba), cancelar el movimiento.
    int targetR = 0, targetC = 0;
    map.ndcToGrid(self.targetPos, targetR, targetC);

    if (targetR < 0 || targetC < 0 || targetR >= map.getRows() || targetC >= map.getCols() ||
        !map.isWalkable(targetR, targetC) ||
        bombBlocksCellForPlayer(targetR, targetC, self.playerId))
    {
        self.movingToTarget = false;
        self.isWalking = false;
        snapPlayerToTileCenter(self, map);
        return;
    }

    const float tileSize = map.getTileSize();
    float stepSize = std::max(0.0f, self.speed * deltaTime);
    if (tileSize > 0.0001f) {
        stepSize = std::min(stepSize, tileSize * 0.45f);
    }

    glm::vec2 d = self.targetPos - self.position;
    const float distToTarget = std::sqrt(d.x * d.x + d.y * d.y);

    if (distToTarget <= stepSize || distToTarget <= 0.00001f) {
        self.position = self.targetPos;
        self.movingToTarget = false;
        self.isWalking = false;
    } else {
        self.position += (d / distToTarget) * stepSize;
        self.isWalking = true;
    }

    self.facingDirKey = moveToFacingDirKey(st.currentMove);
}

static void cpuStartTileMove(Player& self, const GameMap& map, CpuState& st, Move desired, float deltaTime)
{
    self.movingToTarget = false;
    self.isWalking = false;

    if (desired == MOVE_NONE) {
        return;
    }

    self.facingDirKey = moveToFacingDirKey(desired);

    // Alinear al centro del tile actual antes de elegir el siguiente.
    int sr = 0, sc = 0;
    map.ndcToGrid(self.position, sr, sc);
    self.position = map.gridToNDC(sr, sc);

    int tr = sr;
    int tc = sc;

    if (desired == MOVE_UP) tr--;
    else if (desired == MOVE_DOWN) tr++;
    else if (desired == MOVE_LEFT) tc--;
    else if (desired == MOVE_RIGHT) tc++;

    if (tr < 0 || tc < 0 || tr >= map.getRows() || tc >= map.getCols()) return;
    if (!map.isWalkable(tr, tc)) return;
    if (bombBlocksCellForPlayer(tr, tc, self.playerId)) return;

    st.currentMove = desired;
    self.targetPos = map.gridToNDC(tr, tc);
    self.movingToTarget = true;

    // Avanzar un paso inmediatamente.
    cpuContinueTileMove(self, map, st, deltaTime);
}

static bool canEscapeOwnBomb(const GameMap& map, const Player& self)
{
    // ¿Existe una celda fuera del área de explosión alcanzable antes de que explote?
    static constexpr float kFuseTimeSeconds = 3.0f;

    const int rows = map.getRows();
    const int cols = map.getCols();
    if (rows <= 0 || cols <= 0) return false;

    int sr = 0, sc = 0;
    map.ndcToGrid(self.position, sr, sc);

    std::vector<uint8_t> blast((size_t)rows * (size_t)cols, 0);
    markBlastArea(map, sr, sc, self.explosionPower, blast, rows, cols);

    const float tileSize = map.getTileSize();
    if (tileSize <= 0.0001f) return false;

    const float tilesPerSecond = std::max(0.1f, self.speed / tileSize);
    const int maxSteps = std::max(1, (int)std::floor(tilesPerSecond * kFuseTimeSeconds));

    std::vector<int> dist((size_t)rows * (size_t)cols, -1);
    std::queue<std::pair<int, int>> q;

    auto idx = [&](int r, int c) { return r * cols + c; };

    dist[(size_t)idx(sr, sc)] = 0;
    q.push({sr, sc});

    while (!q.empty()) {
        const int r = q.front().first;
        const int c = q.front().second;
        q.pop();

        const int d = dist[(size_t)idx(r, c)];
        if (d > maxSteps) continue;

        // Si ya estamos fuera del blast, es escape válido.
        if (blast[(size_t)idx(r, c)] == 0) return true;

        const int nr[4] = {r - 1, r + 1, r, r};
        const int nc[4] = {c, c, c - 1, c + 1};

        for (int k = 0; k < 4; ++k) {
            const int rr = nr[k];
            const int cc = nc[k];
            if (rr < 0 || cc < 0 || rr >= rows || cc >= cols) continue;
            if (!map.isWalkable(rr, cc)) continue;
            if (bombBlocksCellForPlayer(rr, cc, self.playerId)) continue;

            const int ii = idx(rr, cc);
            if (dist[(size_t)ii] != -1) continue;
            dist[(size_t)ii] = d + 1;
            q.push({rr, cc});
        }
    }

    return false;
}

static void tryPlaceBomb(Player& self, const GameMap& map)
{
    if (!self.isAlive() || self.isGameOver()) return;
    if (!self.canPlaceBomb()) return;

    int r = 0, c = 0;
    map.ndcToGrid(self.position, r, c);

    if (cellHasAnyBomb(r, c)) return;

    glm::vec2 tileCenter = map.gridToNDC(r, c);
    Bomb* bomb = new Bomb(tileCenter, r, c,
                          /*owner=*/&self,
                          /*power=*/self.explosionPower,
                          /*remote=*/self.hasRemoteControl);
    gBombs.push_back(bomb);
    self.activeBombs++;
}

static Move bfsNextStepToNearestPowerUp(const GameMap& map,
                                       const std::vector<uint8_t>& danger,
                                       const Player& self)
{
    const int rows = map.getRows();
    const int cols = map.getCols();
    if (rows <= 0 || cols <= 0) return MOVE_NONE;

    int sr = 0, sc = 0;
    map.ndcToGrid(self.position, sr, sc);

    auto idx = [&](int r, int c) { return r * cols + c; };

    std::vector<int> prev((size_t)rows * (size_t)cols, -1);
    std::queue<std::pair<int, int>> q;

    prev[(size_t)idx(sr, sc)] = idx(sr, sc);
    q.push({sr, sc});

    int foundR = -1, foundC = -1;

    while (!q.empty()) {
        const int r = q.front().first;
        const int c = q.front().second;
        q.pop();

        PowerUpType t;
        if (map.getVisiblePowerUpType(r, c, t)) {
            foundR = r;
            foundC = c;
            break;
        }

        const int nr[4] = {r - 1, r + 1, r, r};
        const int nc[4] = {c, c, c - 1, c + 1};

        for (int k = 0; k < 4; ++k) {
            const int rr = nr[k];
            const int cc = nc[k];
            if (rr < 0 || cc < 0 || rr >= rows || cc >= cols) continue;
            if (prev[(size_t)idx(rr, cc)] != -1) continue;
            if (!map.isWalkable(rr, cc)) continue;
            if (bombBlocksCellForPlayer(rr, cc, self.playerId)) continue;
            if (isDangerousCell(danger, rr, cc, cols)) continue;

            prev[(size_t)idx(rr, cc)] = idx(r, c);
            q.push({rr, cc});
        }
    }

    if (foundR < 0) return MOVE_NONE;

    // Backtrack hasta el vecino inmediato del start.
    int cur = idx(foundR, foundC);
    int start = idx(sr, sc);
    while (prev[(size_t)cur] != start && cur != start) {
        cur = prev[(size_t)cur];
    }

    if (cur == start) return MOVE_NONE;

    const int rr = cur / cols;
    const int cc = cur % cols;

    if (rr == sr - 1 && cc == sc) return MOVE_UP;
    if (rr == sr + 1 && cc == sc) return MOVE_DOWN;
    if (rr == sr && cc == sc - 1) return MOVE_LEFT;
    if (rr == sr && cc == sc + 1) return MOVE_RIGHT;

    return MOVE_NONE;
}

static Move bfsNextStepToTargetCell(const GameMap& map,
                                   const std::vector<uint8_t>& danger,
                                   const Player& self,
                                   int tr,
                                   int tc)
{
    const int rows = map.getRows();
    const int cols = map.getCols();
    if (rows <= 0 || cols <= 0) return MOVE_NONE;

    int sr = 0, sc = 0;
    map.ndcToGrid(self.position, sr, sc);

    if (sr == tr && sc == tc) return MOVE_NONE;

    auto idx = [&](int r, int c) { return r * cols + c; };

    std::vector<int> prev((size_t)rows * (size_t)cols, -1);
    std::queue<std::pair<int, int>> q;

    prev[(size_t)idx(sr, sc)] = idx(sr, sc);
    q.push({sr, sc});

    bool found = false;

    while (!q.empty() && !found) {
        const int r = q.front().first;
        const int c = q.front().second;
        q.pop();

        const int nr[4] = {r - 1, r + 1, r, r};
        const int nc[4] = {c, c, c - 1, c + 1};

        for (int k = 0; k < 4; ++k) {
            const int rr = nr[k];
            const int cc = nc[k];
            if (rr < 0 || cc < 0 || rr >= rows || cc >= cols) continue;
            if (prev[(size_t)idx(rr, cc)] != -1) continue;
            if (!map.isWalkable(rr, cc)) continue;
            if (bombBlocksCellForPlayer(rr, cc, self.playerId)) continue;
            if (isDangerousCell(danger, rr, cc, cols)) continue;

            prev[(size_t)idx(rr, cc)] = idx(r, c);
            if (rr == tr && cc == tc) {
                found = true;
                break;
            }
            q.push({rr, cc});
        }
    }

    if (!found) return MOVE_NONE;

    int cur = idx(tr, tc);
    int start = idx(sr, sc);
    while (prev[(size_t)cur] != start && cur != start) {
        cur = prev[(size_t)cur];
    }

    if (cur == start) return MOVE_NONE;

    const int rr = cur / cols;
    const int cc = cur % cols;

    if (rr == sr - 1 && cc == sc) return MOVE_UP;
    if (rr == sr + 1 && cc == sc) return MOVE_DOWN;
    if (rr == sr && cc == sc - 1) return MOVE_LEFT;
    if (rr == sr && cc == sc + 1) return MOVE_RIGHT;

    return MOVE_NONE;
}

static bool hasAdjacentDestructible(const GameMap& map, const Player& self)
{
    int r = 0, c = 0;
    map.ndcToGrid(self.position, r, c);

    const int rr[4] = {r - 1, r + 1, r, r};
    const int cc[4] = {c, c, c - 1, c + 1};

    for (int k = 0; k < 4; ++k) {
        if (map.isDestructible(rr[k], cc[k])) return true;
    }
    return false;
}

static bool hasAdjacentHiddenPowerUp(const GameMap& map, const Player& self)
{
    int r = 0, c = 0;
    map.ndcToGrid(self.position, r, c);

    const int rr[4] = {r - 1, r + 1, r, r};
    const int cc[4] = {c, c, c - 1, c + 1};

    for (int k = 0; k < 4; ++k) {
        PowerUpType t;
        if (map.getHiddenPowerUpType(rr[k], cc[k], t)) return true;
    }
    return false;
}

static Move bfsNextStepToAdjacentDestructible(const GameMap& map,
                                             const std::vector<uint8_t>& danger,
                                             const Player& self)
{
    const int rows = map.getRows();
    const int cols = map.getCols();
    if (rows <= 0 || cols <= 0) return MOVE_NONE;

    int sr = 0, sc = 0;
    map.ndcToGrid(self.position, sr, sc);

    auto idx = [&](int r, int c) { return r * cols + c; };

    std::vector<int> prev((size_t)rows * (size_t)cols, -1);
    std::queue<std::pair<int, int>> q;

    prev[(size_t)idx(sr, sc)] = idx(sr, sc);
    q.push({sr, sc});

    int foundR = -1, foundC = -1;

    while (!q.empty()) {
        const int r = q.front().first;
        const int c = q.front().second;
        q.pop();

        // Meta: estar en una celda caminable adyacente a un destructible.
        const int ar[4] = {r - 1, r + 1, r, r};
        const int ac[4] = {c, c, c - 1, c + 1};
        for (int k = 0; k < 4; ++k) {
            if (map.isDestructible(ar[k], ac[k])) {
                foundR = r;
                foundC = c;
                break;
            }
        }
        if (foundR >= 0) break;

        const int nr[4] = {r - 1, r + 1, r, r};
        const int nc[4] = {c, c, c - 1, c + 1};

        for (int k = 0; k < 4; ++k) {
            const int rr = nr[k];
            const int cc = nc[k];
            if (rr < 0 || cc < 0 || rr >= rows || cc >= cols) continue;
            if (prev[(size_t)idx(rr, cc)] != -1) continue;
            if (!map.isWalkable(rr, cc)) continue;
            if (bombBlocksCellForPlayer(rr, cc, self.playerId)) continue;
            if (isDangerousCell(danger, rr, cc, cols)) continue;

            prev[(size_t)idx(rr, cc)] = idx(r, c);
            q.push({rr, cc});
        }
    }

    if (foundR < 0) return MOVE_NONE;

    int cur = idx(foundR, foundC);
    int start = idx(sr, sc);
    while (prev[(size_t)cur] != start && cur != start) {
        cur = prev[(size_t)cur];
    }

    if (cur == start) return MOVE_NONE;

    const int rr = cur / cols;
    const int cc = cur % cols;

    if (rr == sr - 1 && cc == sc) return MOVE_UP;
    if (rr == sr + 1 && cc == sc) return MOVE_DOWN;
    if (rr == sr && cc == sc - 1) return MOVE_LEFT;
    if (rr == sr && cc == sc + 1) return MOVE_RIGHT;

    return MOVE_NONE;
}

static Move bfsNextStepToAdjacentHiddenPowerUp(const GameMap& map,
                                              const std::vector<uint8_t>& danger,
                                              const Player& self)
{
    const int rows = map.getRows();
    const int cols = map.getCols();
    if (rows <= 0 || cols <= 0) return MOVE_NONE;

    int sr = 0, sc = 0;
    map.ndcToGrid(self.position, sr, sc);

    auto idx = [&](int r, int c) { return r * cols + c; };

    std::vector<int> prev((size_t)rows * (size_t)cols, -1);
    std::queue<std::pair<int, int>> q;

    prev[(size_t)idx(sr, sc)] = idx(sr, sc);
    q.push({sr, sc});

    int foundR = -1, foundC = -1;

    while (!q.empty()) {
        const int r = q.front().first;
        const int c = q.front().second;
        q.pop();

        // Meta: estar en una celda caminable adyacente a un power-up oculto.
        const int ar[4] = {r - 1, r + 1, r, r};
        const int ac[4] = {c, c, c - 1, c + 1};
        for (int k = 0; k < 4; ++k) {
            PowerUpType t;
            if (map.getHiddenPowerUpType(ar[k], ac[k], t)) {
                foundR = r;
                foundC = c;
                break;
            }
        }
        if (foundR >= 0) break;

        const int nr[4] = {r - 1, r + 1, r, r};
        const int nc[4] = {c, c, c - 1, c + 1};

        for (int k = 0; k < 4; ++k) {
            const int rr = nr[k];
            const int cc = nc[k];
            if (rr < 0 || cc < 0 || rr >= rows || cc >= cols) continue;
            if (prev[(size_t)idx(rr, cc)] != -1) continue;
            if (!map.isWalkable(rr, cc)) continue;
            if (bombBlocksCellForPlayer(rr, cc, self.playerId)) continue;
            if (isDangerousCell(danger, rr, cc, cols)) continue;

            prev[(size_t)idx(rr, cc)] = idx(r, c);
            q.push({rr, cc});
        }
    }

    if (foundR < 0) return MOVE_NONE;

    int cur = idx(foundR, foundC);
    int start = idx(sr, sc);
    while (prev[(size_t)cur] != start && cur != start) {
        cur = prev[(size_t)cur];
    }

    if (cur == start) return MOVE_NONE;

    const int rr = cur / cols;
    const int cc = cur % cols;

    if (rr == sr - 1 && cc == sc) return MOVE_UP;
    if (rr == sr + 1 && cc == sc) return MOVE_DOWN;
    if (rr == sr && cc == sc - 1) return MOVE_LEFT;
    if (rr == sr && cc == sc + 1) return MOVE_RIGHT;

    return MOVE_NONE;
}

static bool tryDetonateRemoteBombIfSafe(Player& self, const GameMap& map, float deltaTime, float perSecond)
{
    if (!self.hasRemoteControl) return false;
    if (perSecond <= 0.0f) return false;
    if (rand01() >= perSecond * std::max(0.0f, deltaTime)) return false;

    Bomb* owned = nullptr;
    for (auto* b : gBombs) {
        if (!b) continue;
        if (b->state != BombState::FUSE) continue;
        if (b->remoteControlled != true) continue;
        if (b->ownerIndex != self.playerId) continue;
        owned = b;
        break;
    }
    if (!owned) return false;

    int sr = 0, sc = 0;
    map.ndcToGrid(self.position, sr, sc);

    const int rows = map.getRows();
    const int cols = map.getCols();
    std::vector<uint8_t> blast((size_t)rows * (size_t)cols, 0);
    markBlastArea(map, owned->gridRow, owned->gridCol, owned->power, blast, rows, cols);

    const size_t selfIdx = (size_t)sr * (size_t)cols + (size_t)sc;
    if (selfIdx < blast.size() && blast[selfIdx] != 0) {
        return false;
    }

    owned->detonate();
    return true;
}

static bool clearLineWalkable(const GameMap& map, int r1, int c1, int r2, int c2)
{
    if (r1 == r2) {
        const int start = std::min(c1, c2) + 1;
        const int end = std::max(c1, c2) - 1;
        for (int c = start; c <= end; ++c) {
            if (!map.isWalkable(r1, c)) return false;
        }
        return true;
    }
    if (c1 == c2) {
        const int start = std::min(r1, r2) + 1;
        const int end = std::max(r1, r2) - 1;
        for (int r = start; r <= end; ++r) {
            if (!map.isWalkable(r, c1)) return false;
        }
        return true;
    }
    return false;
}

static bool canHitTargetWithBomb(const GameMap& map,
                                const Player& self,
                                int selfRow,
                                int selfCol,
                                int targetRow,
                                int targetCol)
{
    // Regla simplificada: el fuego viaja en cruz y se corta con el primer tile no-walkable.
    if (selfRow != targetRow && selfCol != targetCol) return false;

    const int dist = (selfRow == targetRow)
        ? std::abs(targetCol - selfCol)
        : std::abs(targetRow - selfRow);

    if (dist <= 0) return false;
    if (dist > self.explosionPower) return false;

    return clearLineWalkable(map, selfRow, selfCol, targetRow, targetCol);
}

static bool opponentCouldHitCellWithBombNow(const GameMap& map,
                                           const Player& opponent,
                                           int oppRow,
                                           int oppCol,
                                           int cellRow,
                                           int cellCol)
{
    // Heurística: si el oponente puede poner bomba ahora mismo y tú estás en su línea
    // dentro de su rango, evita quedarse alineado.
    if (!opponent.isAlive() || opponent.isGameOver()) return false;
    if (!opponent.canPlaceBomb()) return false;

    if (oppRow != cellRow && oppCol != cellCol) return false;

    const int dist = (oppRow == cellRow)
        ? std::abs(cellCol - oppCol)
        : std::abs(cellRow - oppRow);

    if (dist <= 0) return false;
    if (dist > opponent.explosionPower) return false;

    return clearLineWalkable(map, oppRow, oppCol, cellRow, cellCol);
}

void updateCpuPlayers(GameMode mode,
                      const GameMap* map,
                      std::vector<Player*>& players,
                      float deltaTime,
                      const Context& context,
                      const Settings& settings)
{
    if (!map) return;

    const int start = firstCpuIndex(mode);
    if (start < 0) return;

    const int rows = map->getRows();
    const int cols = map->getCols();
    const std::vector<uint8_t> dangerEasy = buildDangerMask(*map, Difficulty::Easy);
    const std::vector<uint8_t> dangerMedium = buildDangerMask(*map, Difficulty::Medium);
    const std::vector<uint8_t> dangerHard = buildDangerMask(*map, Difficulty::Hard);

    for (int i = start; i < (int)players.size(); ++i) {
        Player* p = players[i];
        if (!p) continue;

        if (!p->isAlive() || p->isGameOver()) {
            p->isWalking = false;
            continue;
        }

        const int pid = p->playerId;
        CpuState& st = (pid >= 0 && pid < 4) ? gCpuStateByPlayerId[pid] : gCpuStateByPlayerId[0];

        st.moveLockSeconds = std::max(0.0f, st.moveLockSeconds - deltaTime);
        st.bombCooldownSeconds = std::max(0.0f, st.bombCooldownSeconds - deltaTime);

        Difficulty diff = difficultyFor(settings, pid);

        const std::vector<uint8_t>& danger =
            (diff == Difficulty::Easy) ? dangerEasy :
            (diff == Difficulty::Medium) ? dangerMedium :
            dangerHard;

        // Movimiento por tiles (como los enemigos): mientras tenga target, seguirlo.
        if (p->movingToTarget) {
            cpuContinueTileMove(*p, *map, st, deltaTime);
            continue;
        }

        const bool avoidDanger = (diff != Difficulty::Easy);
        std::vector<Move> moves = validMoves(*map, danger, *p, /*avoidDanger=*/avoidDanger);

        // Si está encerrado por seguridad, relaja y permite cualquier movimiento válido.
        if (moves.empty() && avoidDanger) {
            moves = validMoves(*map, danger, *p, /*avoidDanger=*/false);
        }

        auto moveIsStillValid = [&](Move m) {
            if (m == MOVE_NONE) return false;
            return std::find(moves.begin(), moves.end(), m) != moves.end();
        };

        Move desired = MOVE_NONE;
        bool holdPosition = false;

        int sr = 0, sc = 0;
        map->ndcToGrid(p->position, sr, sc);

        // Objetivo (combat): rival vivo más cercano.
        Player* target = nullptr;
        int tr = -1;
        int tc = -1;
        float bestDist = 1e9f;
        for (size_t j = 0; j < players.size(); ++j) {
            Player* other = players[j];
            if (!other) continue;
            if (other == p) continue;
            if (!other->isAlive() || other->isGameOver()) continue;
            if (diff == Difficulty::Hard && other->invincible) continue;

            int orow = 0, ocol = 0;
            map->ndcToGrid(other->position, orow, ocol);
            const float manhattan = (float)std::abs(orow - sr) + (float)std::abs(ocol - sc);
            if (manhattan < bestDist) {
                bestDist = manhattan;
                target = other;
                tr = orow;
                tc = ocol;
            }
        }

        const bool adjDestr = hasAdjacentDestructible(*map, *p);
        const bool adjHidden = (diff == Difficulty::Hard) ? hasAdjacentHiddenPowerUp(*map, *p) : false;

        PowerUpType tmp;
        const bool standingOnVisiblePowerUp = map->getVisiblePowerUpType(sr, sc, tmp);

        if (diff == Difficulty::Medium) {
            // Inteligente "honesto": busca power-ups visibles; si no hay, rompe destructibles para descubrir.
            desired = bfsNextStepToNearestPowerUp(*map, danger, *p);
            if (desired == MOVE_NONE) {
                if (standingOnVisiblePowerUp) {
                    holdPosition = true;
                } else if (adjDestr) {
                    holdPosition = true;
                } else {
                    desired = bfsNextStepToAdjacentDestructible(*map, danger, *p);
                }
            }

            // Si no hay power-ups ni un destructible cercano, perseguir rivales.
            if (desired == MOVE_NONE && !holdPosition && target != nullptr) {
                desired = bfsNextStepToTargetCell(*map, danger, *p, tr, tc);
            }
        } else if (diff == Difficulty::Hard) {
            // "Tramposo": además de visibles, conoce los power-ups ocultos.
            desired = bfsNextStepToNearestPowerUp(*map, danger, *p);
            if (desired == MOVE_NONE) {
                if (standingOnVisiblePowerUp) {
                    holdPosition = true;
                } else if (adjHidden) {
                    holdPosition = true;
                } else {
                    desired = bfsNextStepToAdjacentHiddenPowerUp(*map, danger, *p);

                    // Si no hay ocultos, perseguir al rival vivo más cercano.
                    if (desired == MOVE_NONE && target != nullptr) {
                        desired = bfsNextStepToTargetCell(*map, danger, *p, tr, tc);
                    }
                }
            }

            // Hard: usar info del oponente (rango de explosión, etc.) para no quedarse alineado.
            if (target != nullptr && opponentCouldHitCellWithBombNow(*map, *target, tr, tc, sr, sc)) {
                // Preferir romper línea de visión (fila/columna) con un movimiento perpendicular.
                Move breakLine = MOVE_NONE;
                if (sr == tr) {
                    if (std::find(moves.begin(), moves.end(), MOVE_UP) != moves.end()) breakLine = MOVE_UP;
                    else if (std::find(moves.begin(), moves.end(), MOVE_DOWN) != moves.end()) breakLine = MOVE_DOWN;
                } else if (sc == tc) {
                    if (std::find(moves.begin(), moves.end(), MOVE_LEFT) != moves.end()) breakLine = MOVE_LEFT;
                    else if (std::find(moves.begin(), moves.end(), MOVE_RIGHT) != moves.end()) breakLine = MOVE_RIGHT;
                }

                if (breakLine != MOVE_NONE) {
                    desired = breakLine;
                    holdPosition = false;
                    st.moveLockSeconds = 0.0f;
                }
            }
        }

        // Si estás en peligro, no sostengas posición.
        if (holdPosition && avoidDanger && isDangerousCell(danger, sr, sc, cols)) {
            holdPosition = false;
        }

        // Fallback/wander (si no estamos sosteniendo posición por objetivo).
        if (desired == MOVE_NONE && !holdPosition) {
            if (st.moveLockSeconds <= 0.0f || !moveIsStillValid(st.currentMove)) {
                desired = pickRandomMove(moves);
                st.currentMove = desired;
                // Mantener un poquito la dirección para evitar “temblor”.
                st.moveLockSeconds = 0.25f + rand01() * 0.55f;
            } else {
                desired = st.currentMove;
            }
        }

        // Bombas: probabilidades por segundo (FPS independiente).
        bool wantBomb = false;

        if (diff == Difficulty::Easy && st.bombCooldownSeconds <= 0.0f && p->canPlaceBomb()) {
            const float perSecond = 0.20f;
            wantBomb = (rand01() < perSecond * deltaTime);
        }

        if (st.bombCooldownSeconds <= 0.0f && p->canPlaceBomb()) {
            // Combate: si puedo matar a alguien con una bomba en mi tile ahora mismo, priorizarlo.
            if ((diff == Difficulty::Medium || diff == Difficulty::Hard) &&
                target != nullptr &&
                !target->invincible &&
                canHitTargetWithBomb(*map, *p, sr, sc, tr, tc))
            {
                wantBomb = canEscapeOwnBomb(*map, *p);
            }

            if (!wantBomb) {
                if (diff == Difficulty::Easy) {
                    const float perSecond = 0.20f;
                    wantBomb = (rand01() < perSecond * deltaTime);
                } else if (diff == Difficulty::Medium) {
                    const float perSecond = 0.16f;
                    wantBomb = (rand01() < perSecond * deltaTime);
                    if (wantBomb) {
                        wantBomb = canEscapeOwnBomb(*map, *p);
                    }
                } else if (diff == Difficulty::Hard) {
                    // Sabe dónde están ocultos: solo rompe si está pegado a un oculto.
                    const float perSecond = adjHidden ? 0.30f : 0.0f;
                    wantBomb = (rand01() < perSecond * deltaTime);
                    if (wantBomb) {
                        wantBomb = canEscapeOwnBomb(*map, *p);
                    }
                }
            }
        }

        if (wantBomb) {
            // Evitar spam.
            st.bombCooldownSeconds = 0.65f;
            tryPlaceBomb(*p, *map);
        }

        // Remote Control: si tiene bombas "remotas", detonarlas cuando sea seguro.
        {
            const float perSecond =
                (diff == Difficulty::Hard) ? 1.20f :
                (diff == Difficulty::Medium) ? 0.95f :
                0.45f;
            (void)tryDetonateRemoteBombIfSafe(*p, *map, deltaTime, perSecond);
        }

        if (holdPosition) {
            p->isWalking = false;
            p->movingToTarget = false;
            snapPlayerToTileCenter(*p, *map);
            continue;
        }

        if (desired == MOVE_NONE) {
            p->isWalking = false;
            p->movingToTarget = false;
            snapPlayerToTileCenter(*p, *map);
            continue;
        }

        // Movimiento por tiles (como los enemigos).
        cpuStartTileMove(*p, *map, st, desired, deltaTime);

        // Si el move elegido acaba siendo peligroso (porque el jugador está justo en el borde),
        // liberamos el lock para que recalcule antes.
        if (avoidDanger) {
            int pr = 0, pc = 0;
            map->ndcToGrid(p->position, pr, pc);
            if (isDangerousCell(danger, pr, pc, cols)) {
                st.moveLockSeconds = 0.0f;
            }
        }
    }
}

static bool bombBlocksCellForAgent(int row, int col)
{
    for (auto* b : gBombs) {
        if (!b) continue;
        if (b->state == BombState::DONE) continue;
        if (b->gridRow == row && b->gridCol == col) return true;
    }
    return false;
}

static bool isBombStillActiveAtCell(const glm::ivec2& cell)
{
    for (auto* b : gBombs) {
        if (!b) continue;
        if (b->state == BombState::DONE) continue;
        if (b->gridRow == cell.x && b->gridCol == cell.y) return true;
    }
    return false;
}

static void refreshOwnedBombTiles(std::vector<glm::ivec2>& ownedTiles)
{
    for (std::vector<glm::ivec2>::iterator it = ownedTiles.begin(); it != ownedTiles.end();) {
        if (!isBombStillActiveAtCell(*it)) {
            it = ownedTiles.erase(it);
        } else {
            ++it;
        }
    }
}

static EnemyDirection moveToEnemyDirection(Move m)
{
    switch (m) {
        case MOVE_UP: return EnemyDirection::UP;
        case MOVE_DOWN: return EnemyDirection::DOWN;
        case MOVE_LEFT: return EnemyDirection::LEFT;
        case MOVE_RIGHT: return EnemyDirection::RIGHT;
        default: return EnemyDirection::NONE;
    }
}

static std::vector<Move> validMovesForAgent(const GameMap& map,
                                            const std::vector<uint8_t>& danger,
                                            const Agent& self,
                                            bool avoidDanger)
{
    int sr = 0, sc = 0;
    map.ndcToGrid(self.position, sr, sc);

    struct Cand { Move m; int r; int c; };
    const Cand cands[] = {
        {MOVE_UP, sr - 1, sc},
        {MOVE_DOWN, sr + 1, sc},
        {MOVE_LEFT, sr, sc - 1},
        {MOVE_RIGHT, sr, sc + 1}
    };

    std::vector<Move> out;
    out.reserve(4);

    const int rows = map.getRows();
    const int cols = map.getCols();

    for (const Cand& c : cands) {
        if (c.r < 0 || c.c < 0 || c.r >= rows || c.c >= cols) continue;
        if (!map.isWalkable(c.r, c.c)) continue;
        if (bombBlocksCellForAgent(c.r, c.c)) continue;
        if (avoidDanger && isDangerousCell(danger, c.r, c.c, cols)) continue;
        out.push_back(c.m);
    }

    return out;
}

static Move bfsNextStepToTargetCellForAgent(const GameMap& map,
                                            const std::vector<uint8_t>& danger,
                                            const Agent& self,
                                            int tr,
                                            int tc,
                                            bool avoidDanger)
{
    const int rows = map.getRows();
    const int cols = map.getCols();
    if (rows <= 0 || cols <= 0) return MOVE_NONE;

    int sr = 0, sc = 0;
    map.ndcToGrid(self.position, sr, sc);
    if (sr == tr && sc == tc) return MOVE_NONE;

    auto idx = [&](int r, int c) { return r * cols + c; };

    std::vector<int> prev((size_t)rows * (size_t)cols, -1);
    std::queue<std::pair<int, int> > q;

    prev[(size_t)idx(sr, sc)] = idx(sr, sc);
    q.push(std::make_pair(sr, sc));

    bool found = false;
    while (!q.empty() && !found) {
        const int r = q.front().first;
        const int c = q.front().second;
        q.pop();

        const int nr[4] = {r - 1, r + 1, r, r};
        const int nc[4] = {c, c, c - 1, c + 1};

        for (int k = 0; k < 4; ++k) {
            const int rr = nr[k];
            const int cc = nc[k];
            if (rr < 0 || cc < 0 || rr >= rows || cc >= cols) continue;
            if (prev[(size_t)idx(rr, cc)] != -1) continue;
            if (!map.isWalkable(rr, cc)) continue;
            if (bombBlocksCellForAgent(rr, cc)) continue;
            if (avoidDanger && isDangerousCell(danger, rr, cc, cols)) continue;

            prev[(size_t)idx(rr, cc)] = idx(r, c);
            if (rr == tr && cc == tc) {
                found = true;
                break;
            }
            q.push(std::make_pair(rr, cc));
        }
    }

    if (!found) return MOVE_NONE;

    int cur = idx(tr, tc);
    const int start = idx(sr, sc);
    while (prev[(size_t)cur] != start && cur != start) {
        cur = prev[(size_t)cur];
    }

    if (cur == start) return MOVE_NONE;

    const int rr = cur / cols;
    const int cc = cur % cols;

    if (rr == sr - 1 && cc == sc) return MOVE_UP;
    if (rr == sr + 1 && cc == sc) return MOVE_DOWN;
    if (rr == sr && cc == sc - 1) return MOVE_LEFT;
    if (rr == sr && cc == sc + 1) return MOVE_RIGHT;
    return MOVE_NONE;
}

static bool canEscapeOwnBombForAgent(const GameMap& map,
                                     const Agent& self,
                                     int bombPower)
{
    static constexpr float kFuseTimeSeconds = 3.0f;

    const int rows = map.getRows();
    const int cols = map.getCols();
    if (rows <= 0 || cols <= 0) return false;

    int sr = 0, sc = 0;
    map.ndcToGrid(self.position, sr, sc);

    std::vector<uint8_t> blast((size_t)rows * (size_t)cols, 0);
    markBlastArea(map, sr, sc, bombPower, blast, rows, cols);

    const float tileSize = map.getTileSize();
    if (tileSize <= 0.0001f) return false;

    const float tilesPerSecond = std::max(0.1f, self.speed / tileSize);
    const int maxSteps = std::max(1, (int)std::floor(tilesPerSecond * kFuseTimeSeconds));

    std::vector<int> dist((size_t)rows * (size_t)cols, -1);
    std::queue<std::pair<int, int> > q;
    auto idx = [&](int r, int c) { return r * cols + c; };

    dist[(size_t)idx(sr, sc)] = 0;
    q.push(std::make_pair(sr, sc));

    while (!q.empty()) {
        const int r = q.front().first;
        const int c = q.front().second;
        q.pop();

        const int d = dist[(size_t)idx(r, c)];
        if (d > maxSteps) continue;
        if (blast[(size_t)idx(r, c)] == 0) return true;

        const int nr[4] = {r - 1, r + 1, r, r};
        const int nc[4] = {c, c, c - 1, c + 1};

        for (int k = 0; k < 4; ++k) {
            const int rr = nr[k];
            const int cc = nc[k];
            if (rr < 0 || cc < 0 || rr >= rows || cc >= cols) continue;
            if (!map.isWalkable(rr, cc)) continue;
            if (bombBlocksCellForAgent(rr, cc)) continue;

            const int ii = idx(rr, cc);
            if (dist[(size_t)ii] != -1) continue;
            dist[(size_t)ii] = d + 1;
            q.push(std::make_pair(rr, cc));
        }
    }

    return false;
}

static bool canHitTargetWithBombForAgent(const GameMap& map,
                                         int bombPower,
                                         int selfRow,
                                         int selfCol,
                                         int targetRow,
                                         int targetCol)
{
    if (selfRow != targetRow && selfCol != targetCol) return false;

    const int dist = (selfRow == targetRow)
        ? std::abs(targetCol - selfCol)
        : std::abs(targetRow - selfRow);
    if (dist <= 0) return false;
    if (dist > bombPower) return false;

    return clearLineWalkable(map, selfRow, selfCol, targetRow, targetCol);
}

static bool isHostileToAgent(const Agent& bot, const Player* player)
{
    if (!player) return false;
    if (!player->isAlive() || player->isGameOver()) return false;
    if (player->invincible) return false;

    // En custom, los humanos son aliados solo para el compañero CPU.
    return bot.isEnemy();
}

static bool isHostileToAgent(const Agent& bot, const Enemy* enemy)
{
    if (!enemy) return false;
    if (enemy == &bot) return false;
    if (enemy->lifeState != EnemyLifeState::Alive) return false;

    const Agent* otherBot = dynamic_cast<const Agent*>(enemy);
    if (otherBot) {
        return otherBot->getAffiliation() != bot.getAffiliation();
    }

    // Los monstruos son hostiles para cualquier Bomberman CPU.
    return true;
}

static bool findNearestHostileTarget(const Agent& bot,
                                     int selfRow,
                                     int selfCol,
                                     int& outTargetRow,
                                     int& outTargetCol)
{
    if (!bot.gameMap) return false;

    float bestDist = 1e9f;
    bool found = false;

    if (bot.playersList) {
        for (Player* p : *bot.playersList) {
            if (!isHostileToAgent(bot, p)) continue;

            int pr = 0, pc = 0;
            bot.gameMap->ndcToGrid(p->position, pr, pc);
            const float manhattan = (float)std::abs(pr - selfRow) + (float)std::abs(pc - selfCol);
            if (manhattan < bestDist) {
                bestDist = manhattan;
                outTargetRow = pr;
                outTargetCol = pc;
                found = true;
            }
        }
    }

    for (Enemy* enemy : gEnemies) {
        if (!isHostileToAgent(bot, enemy)) continue;

        int er = 0, ec = 0;
        bot.gameMap->ndcToGrid(enemy->position, er, ec);
        const float manhattan = (float)std::abs(er - selfRow) + (float)std::abs(ec - selfCol);
        if (manhattan < bestDist) {
            bestDist = manhattan;
            outTargetRow = er;
            outTargetCol = ec;
            found = true;
        }
    }

    return found;
}

Agent::Agent(glm::vec2 pos,
             glm::vec2 size,
             float speed,
             TeamAffiliation inAffiliation,
             Difficulty inDifficulty,
             const std::string& spritePrefix)
    : Enemy(pos, size, speed, /*hp=*/1, /*score=*/1300, /*passSoftBlocks=*/false, /*boss=*/false),
      affiliation(inAffiliation),
      difficulty(inDifficulty),
      botSpritePrefix(spritePrefix)
{
    spriteBaseId = botSpritePrefix;
    currentSpriteName = botSpritePrefix + ".abajo.0";
    facing = EnemyDirection::DOWN;
}

Agent::~Agent() {}

bool Agent::takeDamage(const SpriteAtlas& atlas, int amount)
{
    (void)atlas;
    if (lifeState != EnemyLifeState::Alive) return false;

    hitPoints -= amount;
    if (hitPoints <= 0) {
        hitPoints = 0;
        startDying(gPlayerAtlas);
        return true;
    }
    return false;
}

void Agent::startDying(const SpriteAtlas& atlas)
{
    (void)atlas;
    Enemy::startDying(gPlayerAtlas);
}

void Agent::Update()
{
    if (lifeState != EnemyLifeState::Alive) return;
    if (!gameMap) return;

    moveLockSeconds = std::max(0.0f, moveLockSeconds - deltaTime);
    bombCooldownSeconds = std::max(0.0f, bombCooldownSeconds - deltaTime);
    refreshOwnedBombTiles(ownedBombTiles);

    int sr = 0, sc = 0;
    gameMap->ndcToGrid(position, sr, sc);

    const std::vector<uint8_t> danger = buildDangerMask(*gameMap, difficulty);
    const int cols = gameMap->getCols();
    const bool selfDanger = isDangerousCell(danger, sr, sc, cols);

    std::vector<Move> moves = validMovesForAgent(*gameMap, danger, *this, /*avoidDanger=*/difficulty != Difficulty::Easy);
    if (moves.empty()) {
        moves = validMovesForAgent(*gameMap, danger, *this, /*avoidDanger=*/false);
    }

    int tr = -1;
    int tc = -1;
    bool hasTarget = findNearestHostileTarget(*this, sr, sc, tr, tc);

    auto moveIsStillValid = [&](Move m) {
        if (m == MOVE_NONE) return false;
        return std::find(moves.begin(), moves.end(), m) != moves.end();
    };

    Move desired = MOVE_NONE;
    if (difficulty == Difficulty::Easy) {
        desired = pickRandomMove(moves);
    } else if (selfDanger) {
        desired = pickRandomMove(moves);
    } else if (hasTarget) {
        desired = bfsNextStepToTargetCellForAgent(*gameMap,
                                                  danger,
                                                  *this,
                                                  tr,
                                                  tc,
                                                  /*avoidDanger=*/true);
    }

    if (desired == MOVE_NONE) {
        if (moveLockSeconds <= 0.0f || !moveIsStillValid(currentMove)) {
            desired = pickRandomMove(moves);
            currentMove = desired;
            moveLockSeconds = 0.20f + rand01() * 0.45f;
        } else {
            desired = currentMove;
        }
    }

    if (difficulty == Difficulty::Easy && bombCooldownSeconds <= 0.0f && (int)ownedBombTiles.size() < maxOwnedBombs) {
        const float perSecond = 0.20f;
        if (rand01() < perSecond * std::max(0.0f, deltaTime) && cellHasAnyBomb(sr, sc) == false) {
            Bomb* bomb = new Bomb(gameMap->gridToNDC(sr, sc),
                                  sr,
                                  sc,
                                  /*owner=*/nullptr,
                                  /*power=*/bombPower,
                                  /*remote=*/false);
            bomb->ownerIndex = -1;
            bomb->ownerLeftTile = true;
            gBombs.push_back(bomb);
            ownedBombTiles.push_back(glm::ivec2(sr, sc));
            bombCooldownSeconds = 0.75f;
        }
    }

    if (hasTarget && bombCooldownSeconds <= 0.0f && (int)ownedBombTiles.size() < maxOwnedBombs) {
        const bool inRange = canHitTargetWithBombForAgent(*gameMap, bombPower, sr, sc, tr, tc);
        if (inRange) {
            const float perSecond = isAlly() ? 0.32f : 0.26f;
            const bool trigger = (rand01() < perSecond * std::max(0.0f, deltaTime));
            if (trigger && canEscapeOwnBombForAgent(*gameMap, *this, bombPower)) {
                if (!cellHasAnyBomb(sr, sc)) {
                    Bomb* bomb = new Bomb(gameMap->gridToNDC(sr, sc),
                                          sr,
                                          sc,
                                          /*owner=*/nullptr,
                                          /*power=*/bombPower,
                                          /*remote=*/false);
                    bomb->ownerIndex = -1;
                    bomb->ownerLeftTile = true;
                    gBombs.push_back(bomb);
                    ownedBombTiles.push_back(glm::ivec2(sr, sc));
                    bombCooldownSeconds = 0.75f;
                }
            }
        }
    }

    bool moved = false;
    if (desired != MOVE_NONE) {
        currentMove = desired;
        facing = moveToEnemyDirection(desired);
        moved = tryMove(facing, std::max(0.0f, speed * deltaTime));
    }

    if (moved) {
        animTimer += deltaTime;
        if (animTimer >= 0.12f) {
            animTimer = 0.0f;
            animFrame ^= 1;
        }
    } else {
        animTimer = 0.0f;
        animFrame = 0;
    }

    std::string dirStr = "abajo";
    flipX = 0.0f;
    switch (facing) {
        case EnemyDirection::UP:
            dirStr = "arriba";
            break;
        case EnemyDirection::DOWN:
            dirStr = "abajo";
            break;
        case EnemyDirection::LEFT:
            dirStr = "derecha";
            flipX = 1.0f;
            break;
        case EnemyDirection::RIGHT:
            dirStr = "derecha";
            break;
        default:
            dirStr = "abajo";
            break;
    }
    currentSpriteName = botSpritePrefix + "." + dirStr + "." + std::to_string(animFrame);
}

void Agent::Draw()
{
    if (lifeState == EnemyLifeState::Dead) return;
    if (!gameMap) return;
    if (texture == 0) return;

    glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
    std::string spriteName = currentSpriteName;
    if (!getUvRectForSprite(gPlayerAtlas, spriteName, uvRect)) {
        spriteName = botSpritePrefix + ".abajo.0";
        if (!getUvRectForSprite(gPlayerAtlas, spriteName, uvRect)) {
            return;
        }
    }

    const float enemyScaleFactor = 1.8f;
    const float halfTile = gameMap->getTileSize() / 2.0f;

    float aspect = 1.0f;
    auto it = gPlayerAtlas.sprites.find(spriteName);
    if (it != gPlayerAtlas.sprites.end() && it->second.h > 0) {
        aspect = static_cast<float>(it->second.w) / static_cast<float>(it->second.h);
    }

    glm::mat4 model(1.0f);
    model[3][0] = position.x;
    model[3][1] = position.y + (enemyScaleFactor - 1.0f) * halfTile * 0.8f;
    model[0][0] = halfTile * enemyScaleFactor * aspect;
    model[1][1] = halfTile * enemyScaleFactor;

    glBindTexture(GL_TEXTURE_2D, texture);
    glUniformMatrix4fv(uniformModel, 1, GL_FALSE, &model[0][0]);
    glUniform4fv(uniformUvRect, 1, &uvRect[0]);
    glUniform1f(uniformFlipX, flipX);
    glm::vec4 tint(1.0f, 1.0f, 1.0f, 1.0f);
    glUniform4fv(uniformTintColor, 1, &tint[0]);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    if (enemyTexture != 0) {
        glBindTexture(GL_TEXTURE_2D, enemyTexture);
    }
}

bool isAgent(const Enemy* enemy)
{
    return dynamic_cast<const Agent*>(enemy) != nullptr;
}

bool isAllyAgent(const Enemy* enemy)
{
    const Agent* bot = dynamic_cast<const Agent*>(enemy);
    return bot != nullptr && bot->isAlly();
}

bool isEnemyAgent(const Enemy* enemy)
{
    const Agent* bot = dynamic_cast<const Agent*>(enemy);
    return bot != nullptr && bot->isEnemy();
}

} // namespace CpuBomberman
