#include "enemy.hpp"
#include "game_map.hpp"
#include <cstdlib>
#include <cmath>

Enemy::Enemy(glm::vec2 pos, glm::vec2 size, float speed,
             int hp, int score, bool passSoftBlocks, bool boss)
    : Entity(pos, size, speed),
      hitPoints(hp), maxHitPoints(hp), scoreValue(score),
      alive(true), canPassSoftBlocks(passSoftBlocks), isBoss(boss),
      facing(EnemyDirection::LEFT),
      gameMap(nullptr), playerPos(nullptr), deltaTime(0.0f)
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

void Enemy::setContext(const GameMap* map, const glm::vec2* playerPosition) {
    gameMap = map;
    playerPos = playerPosition;
}

void Enemy::setDeltaTime(float dt) {
    deltaTime = dt;
}

// ── Utilidades de IA ──

float Enemy::distanceToPlayer() const {
    if (!playerPos) return 99999.0f;
    glm::vec2 diff = *playerPos - position;
    return std::abs(diff.x) + std::abs(diff.y); // Manhattan
}

EnemyDirection Enemy::directionTowardPlayer() const {
    if (!playerPos) return EnemyDirection::NONE;
    glm::vec2 diff = *playerPos - position;
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
    glm::vec2 delta = dirToVec(dir) * stepSize;
    glm::vec2 newPos = position + delta;
    float halfTile = gameMap->getTileSize() / 2.0f;

    if (gameMap->canMoveTo(newPos, halfTile)) {
        position = newPos;
        facing = dir;
        return true;
    }
    return false;
}

bool Enemy::canMoveInDirection(EnemyDirection dir, float lookAhead) const {
    if (!gameMap) return false;
    glm::vec2 probe = position + dirToVec(dir) * lookAhead;
    float halfTile = gameMap->getTileSize() / 2.0f;
    return gameMap->canMoveTo(probe, halfTile);
}
