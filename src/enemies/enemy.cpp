#include "enemy.hpp"
#include "game_map.hpp"
#include "player.hpp"
#include <cstdlib>
#include <cmath>

Enemy::Enemy(glm::vec2 pos, glm::vec2 size, float speed,
             int hp, int score, bool passSoftBlocks, bool boss)
    : Entity(pos, size, speed),
      hitPoints(hp), maxHitPoints(hp), scoreValue(score),
      alive(true), canPassSoftBlocks(passSoftBlocks), isBoss(boss),
      facing(EnemyDirection::LEFT),
      gameMap(nullptr), playersList(nullptr), deltaTime(0.0f),
      animTimer(0.0f), animFrame(0), currentSpriteName(""), flipX(0.0f)
{}

Enemy::~Enemy() {}

bool Enemy::takeDamage(int amount) {
    if (!alive) return false;
    hitPoints -= amount;
    if (hitPoints <= 0) {
        hitPoints = 0;
        alive = false;
        return true; // murió
    }
    return false;
}

void Enemy::setContext(const GameMap* map, const std::vector<Player*>* players) {
    gameMap = map;
    playersList = players;
}

void Enemy::setDeltaTime(float dt) {
    deltaTime = dt;
}

// ── Utilidades de IA ──

glm::vec2 Enemy::getClosestPlayerPos(float& out_dist) const {
    glm::vec2 bestPos = position;
    out_dist = 99999.0f;
    if (!playersList) return bestPos;

    for (Player* p : *playersList) {
        if (!p) continue;
        glm::vec2 diff = p->position - position;
        float d = std::abs(diff.x) + std::abs(diff.y);
        if (d < out_dist) {
            out_dist = d;
            bestPos = p->position;
        }
    }
    return bestPos;
}

float Enemy::distanceToPlayer() const {
    float d;
    getClosestPlayerPos(d);
    return d;
}

EnemyDirection Enemy::directionTowardPlayer() const {
    float dist;
    glm::vec2 target = getClosestPlayerPos(dist);
    if (dist >= 99999.0f) return EnemyDirection::NONE;
    
    glm::vec2 diff = target - position;
    if (std::abs(diff.x) > std::abs(diff.y)) {
        return (diff.x > 0.0f) ? EnemyDirection::RIGHT : EnemyDirection::LEFT;
    } else {
        return (diff.y > 0.0f) ? EnemyDirection::UP : EnemyDirection::DOWN;
    }
}

EnemyDirection Enemy::oppositeDirection(EnemyDirection dir) {
    switch (dir) {
        case EnemyDirection::UP:    return EnemyDirection::DOWN;
        case EnemyDirection::DOWN:  return EnemyDirection::UP;
        case EnemyDirection::LEFT:  return EnemyDirection::RIGHT;
        case EnemyDirection::RIGHT: return EnemyDirection::LEFT;
        default: return EnemyDirection::NONE;
    }
}

glm::vec2 Enemy::dirToVec(EnemyDirection dir) {
    switch (dir) {
        case EnemyDirection::UP:    return glm::vec2( 0.0f,  1.0f);
        case EnemyDirection::DOWN:  return glm::vec2( 0.0f, -1.0f);
        case EnemyDirection::LEFT:  return glm::vec2(-1.0f,  0.0f);
        case EnemyDirection::RIGHT: return glm::vec2( 1.0f,  0.0f);
        default: return glm::vec2(0.0f);
    }
}

EnemyDirection Enemy::randomDirection() {
    int r = std::rand() % 4;
    switch (r) {
        case 0: return EnemyDirection::UP;
        case 1: return EnemyDirection::DOWN;
        case 2: return EnemyDirection::LEFT;
        default: return EnemyDirection::RIGHT;
    }
}

bool Enemy::tryMove(EnemyDirection dir, float stepSize) {
    if (!gameMap) return false;
    const float halfTile = gameMap->getTileSize() / 2.0f;

    // Snap perpendicular al movimiento hacia el centro del tile actual.
    {
        const float snapStrength = 0.2f;
        const float snapMaxDist  = halfTile * 0.45f;
        int tr, tc;
        gameMap->ndcToGrid(position, tr, tc);
        glm::vec2 tileCenter = gameMap->gridToNDC(tr, tc);

        if (dir == EnemyDirection::LEFT || dir == EnemyDirection::RIGHT) {
            float dy = tileCenter.y - position.y;
            if (std::abs(dy) <= snapMaxDist)
                position.y += dy * snapStrength;
        } else if (dir == EnemyDirection::UP || dir == EnemyDirection::DOWN) {
            float dx = tileCenter.x - position.x;
            if (std::abs(dx) <= snapMaxDist)
                position.x += dx * snapStrength;
        }
    }

    glm::vec2 newPos = position + dirToVec(dir) * stepSize;

    // Sondas de colisión: dos esquinas en el borde frontal.
    {
        int r, c;
        const float eFront = halfTile;          // siempre cae en el tile vecino
        const float eSide  = halfTile * 0.60f;

        if (dir == EnemyDirection::UP) {
            gameMap->ndcToGrid({newPos.x - eSide, newPos.y + eFront}, r, c);
            if (!gameMap->isWalkable(r, c) && !(canPassSoftBlocks && gameMap->isDestructible(r, c))) return false;
            gameMap->ndcToGrid({newPos.x + eSide, newPos.y + eFront}, r, c);
            if (!gameMap->isWalkable(r, c) && !(canPassSoftBlocks && gameMap->isDestructible(r, c))) return false;
        }
        if (dir == EnemyDirection::DOWN) {
            gameMap->ndcToGrid({newPos.x - eSide, newPos.y - eFront}, r, c);
            if (!gameMap->isWalkable(r, c) && !(canPassSoftBlocks && gameMap->isDestructible(r, c))) return false;
            gameMap->ndcToGrid({newPos.x + eSide, newPos.y - eFront}, r, c);
            if (!gameMap->isWalkable(r, c) && !(canPassSoftBlocks && gameMap->isDestructible(r, c))) return false;
        }
        if (dir == EnemyDirection::LEFT) {
            gameMap->ndcToGrid({newPos.x - eFront, newPos.y - eSide}, r, c);
            if (!gameMap->isWalkable(r, c) && !(canPassSoftBlocks && gameMap->isDestructible(r, c))) return false;
            gameMap->ndcToGrid({newPos.x - eFront, newPos.y + eSide}, r, c);
            if (!gameMap->isWalkable(r, c) && !(canPassSoftBlocks && gameMap->isDestructible(r, c))) return false;
        }
        if (dir == EnemyDirection::RIGHT) {
            gameMap->ndcToGrid({newPos.x + eFront, newPos.y - eSide}, r, c);
            if (!gameMap->isWalkable(r, c) && !(canPassSoftBlocks && gameMap->isDestructible(r, c))) return false;
            gameMap->ndcToGrid({newPos.x + eFront, newPos.y + eSide}, r, c);
            if (!gameMap->isWalkable(r, c) && !(canPassSoftBlocks && gameMap->isDestructible(r, c))) return false;
        }
    }

    position = newPos;
    facing = dir;
    return true;
}

bool Enemy::canMoveInDirection(EnemyDirection dir, float lookAhead) const {
    if (!gameMap) return false;
    glm::vec2 probe = position + dirToVec(dir) * lookAhead;
    float halfTile = gameMap->getTileSize() / 2.0f;
    return gameMap->canMoveTo(probe, halfTile);
}
