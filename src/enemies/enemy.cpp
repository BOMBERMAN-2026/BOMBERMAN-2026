#include "enemy.hpp"
#include "game_map.hpp"
#include "player.hpp"
#include "sprite_atlas.hpp"
#include "bomb.hpp"
#include <cstdlib>
    #include <cmath>

    Enemy::Enemy(glm::vec2 pos, glm::vec2 size, float speed,
                int hp, int score, bool passSoftBlocks, bool boss)
        : Entity(pos, size, speed),
        hitPoints(hp), maxHitPoints(hp), scoreValue(score),
      alive(true), canPassSoftBlocks(passSoftBlocks), isBoss(boss),
    lifeState(EnemyLifeState::Alive),
    facing(EnemyDirection::LEFT),
      gameMap(nullptr), playersList(nullptr), deltaTime(0.0f),
    animTimer(0.0f), animFrame(0), currentSpriteName(""), flipX(0.0f)
{}

Enemy::~Enemy() {}

static std::string baseIdFromSpriteName(const std::string& spriteName) {
    // Extrae "leon" de "leon.derecha.0".
    const std::size_t dot = spriteName.find('.');
    if (dot == std::string::npos) return spriteName;
    return spriteName.substr(0, dot);
}

static int countDeathFrames(const SpriteAtlas& atlas, const std::string& deathPrefix) {
    // Cuenta frames consecutivos desde 0: deathPrefix + "0", "1", ...
    int count = 0;
    while (true) {
        const std::string key = deathPrefix + std::to_string(count);
        if (atlas.sprites.find(key) == atlas.sprites.end()) break;
        ++count;
        if (count > 128) break; // seguridad
    }
    return count;
}

// Devuelve true si pasa de Alive -> Dying.
bool Enemy::takeDamage(const SpriteAtlas& atlas, int amount) {
    if (lifeState != EnemyLifeState::Alive) return false;

    hitPoints -= amount;
    if (hitPoints <= 0) {
        hitPoints = 0;
        startDying(atlas);
        return true;
    }
    return false;
}

// Fuerza estado Dying e inicializa contador de frames.
void Enemy::startDying(const SpriteAtlas& atlas) {
    if (lifeState != EnemyLifeState::Alive) return;

    lifeState = EnemyLifeState::Dying;
    alive = true; // se queda en escena mientras anima

    if (spriteBaseId.empty()) {
        if (!currentSpriteName.empty()) spriteBaseId = baseIdFromSpriteName(currentSpriteName);
    }
    if (spriteBaseId.empty()) spriteBaseId = "enemy";

    deathSpritePrefix = spriteBaseId + ".muerto.";
    deathFrameCount = countDeathFrames(atlas, deathSpritePrefix);
    if (deathFrameCount <= 0) {
        // Fallback: al menos un frame para no desaparecer sin feedback.
        deathFrameCount = 1;
    }

    deathTimer = 0.0f;
    deathFrame = 0;
    flipX = 0.0f;
    currentSpriteName = deathSpritePrefix + "0";
}

// Avanza animación; al terminar marca Dead.
void Enemy::updateDeath(float dt) {
    if (lifeState != EnemyLifeState::Dying) return;

    deathTimer += dt;
    while (deathTimer >= deathFrameInterval) {
        deathTimer -= deathFrameInterval;
        ++deathFrame;
        if (deathFrame >= deathFrameCount) {
            lifeState = EnemyLifeState::Dead;
            alive = false;
            return;
        }
    }

    currentSpriteName = deathSpritePrefix + std::to_string(deathFrame);
}

void Enemy::setContext(const GameMap* map, const std::vector<Player*>* players) {
    gameMap = map;
    playersList = players;
}

// Actualiza el deltaTime (llamar cada frame antes de Update).
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

// Distancia Manhattan al jugador (en coordenadas NDC).
float Enemy::distanceToPlayer() const {
    float d;
    getClosestPlayerPos(d);
    return d;
}

// Dirección general hacia el jugador (eje dominante).
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

// Dirección opuesta.
EnemyDirection Enemy::oppositeDirection(EnemyDirection dir) {
    switch (dir) {
        case EnemyDirection::UP:    return EnemyDirection::DOWN;
        case EnemyDirection::DOWN:  return EnemyDirection::UP;
        case EnemyDirection::LEFT:  return EnemyDirection::RIGHT;
        case EnemyDirection::RIGHT: return EnemyDirection::LEFT;
        default: return EnemyDirection::NONE;
    }
}

// Convierte EnemyDirection a un vector unitario.
glm::vec2 Enemy::dirToVec(EnemyDirection dir) {
    switch (dir) {
        case EnemyDirection::UP:    return glm::vec2( 0.0f,  1.0f);
        case EnemyDirection::DOWN:  return glm::vec2( 0.0f, -1.0f);
        case EnemyDirection::LEFT:  return glm::vec2(-1.0f,  0.0f);
        case EnemyDirection::RIGHT: return glm::vec2( 1.0f,  0.0f);
        default: return glm::vec2(0.0f);
    }
}

// Elige una dirección aleatoria entre las 4 cardinales.
EnemyDirection Enemy::randomDirection() {
    int r = std::rand() % 4;
    switch (r) {
        case 0: return EnemyDirection::UP;
        case 1: return EnemyDirection::DOWN;
        case 2: return EnemyDirection::LEFT;
        default: return EnemyDirection::RIGHT;
    }
}

// Intenta moverse en la dirección dada; devuelve true si pudo avanzar.
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

        // Las bombas activas ocupan tile completo y bloquean a los enemigos.
        auto bombBlocks = [&](int rr, int cc) {
            for (auto* b : gBombs) {
                if (!b) continue;
                if (b->state == BombState::DONE) continue;
                if (b->gridRow == rr && b->gridCol == cc) return true;
            }
            return false;
        };

        auto tileBlocks = [&](int rr, int cc) {
            const bool blockedByMap =
                !gameMap->isWalkable(rr, cc) && !(canPassSoftBlocks && gameMap->isDestructible(rr, cc));
            return blockedByMap || bombBlocks(rr, cc);
        };

        if (dir == EnemyDirection::UP) {
            gameMap->ndcToGrid({newPos.x - eSide, newPos.y + eFront}, r, c);
            if (tileBlocks(r, c)) return false;
            gameMap->ndcToGrid({newPos.x + eSide, newPos.y + eFront}, r, c);
            if (tileBlocks(r, c)) return false;
        }
        if (dir == EnemyDirection::DOWN) {
            gameMap->ndcToGrid({newPos.x - eSide, newPos.y - eFront}, r, c);
            if (tileBlocks(r, c)) return false;
            gameMap->ndcToGrid({newPos.x + eSide, newPos.y - eFront}, r, c);
            if (tileBlocks(r, c)) return false;
        }
        if (dir == EnemyDirection::LEFT) {
            gameMap->ndcToGrid({newPos.x - eFront, newPos.y - eSide}, r, c);
            if (tileBlocks(r, c)) return false;
            gameMap->ndcToGrid({newPos.x - eFront, newPos.y + eSide}, r, c);
            if (tileBlocks(r, c)) return false;
        }
        if (dir == EnemyDirection::RIGHT) {
            gameMap->ndcToGrid({newPos.x + eFront, newPos.y - eSide}, r, c);
            if (tileBlocks(r, c)) return false;
            gameMap->ndcToGrid({newPos.x + eFront, newPos.y + eSide}, r, c);
            if (tileBlocks(r, c)) return false;
        }
    }

    position = newPos;
    facing = dir;
    return true;
}

// Comprueba si la casilla en esa dirección es transitable.
bool Enemy::canMoveInDirection(EnemyDirection dir, float lookAhead) const {
    if (!gameMap) return false;
    glm::vec2 probe = position + dirToVec(dir) * lookAhead;

    int r, c;
    gameMap->ndcToGrid(probe, r, c);

    if (!gameMap->isWalkable(r, c) && !(canPassSoftBlocks && gameMap->isDestructible(r, c))) return false;

    for (auto* b : gBombs) {
        if (!b) continue;
        if (b->state == BombState::DONE) continue;
        if (b->gridRow == r && b->gridCol == c) return false;
    }

    return true;
}
