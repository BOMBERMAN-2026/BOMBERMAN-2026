#include "enemies/fantasma_mortal.hpp"
#include "game_map.hpp"
#include <cmath>

FantasmaMortal::FantasmaMortal(glm::vec2 pos, glm::vec2 size, float speed)
    : Enemy(pos, size, speed, /*hp=*/1, /*score=*/1000, /*passSoftBlocks=*/true),
      retreating(false),
      retreatTimer(0.0f),
      retreatSpeed(speed * 3.0f), // Retrocede mucho más rápido
      normalSpeed(speed),
      retreatDir(0.0f)
{
    facing = EnemyDirection::LEFT;
}

FantasmaMortal::~FantasmaMortal() {}

void FantasmaMortal::notifyBombNearby(glm::vec2 bombPos) {
    if (!alive) return;
    // Retroceder en dirección opuesta a la bomba
    glm::vec2 away = position - bombPos;
    float len = std::sqrt(away.x * away.x + away.y * away.y);
    if (len > 0.001f) {
        retreatDir = away / len;
    } else {
        retreatDir = dirToVec(randomDirection());
    }
    retreating = true;
    retreatTimer = 0.8f; // Retrocede durante 0.8 segundos
    speed = retreatSpeed;
}

void FantasmaMortal::Update() {
    if (!alive) return;

    if (retreating) {
        // Retroceso rápido
        retreatTimer -= deltaTime;
        glm::vec2 newPos = position + retreatDir * speed * deltaTime;
        // El fantasma puede atravesar bloques blandos, pero comprobamos
        // que no se salga del mapa (bordes duros)
        if (gameMap) {
            float halfTile = gameMap->getTileSize() / 2.0f;
            if (gameMap->canMoveTo(newPos, halfTile)) {
                position = newPos;
            }
        }
        if (retreatTimer <= 0.0f) {
            retreating = false;
            speed = normalSpeed;
        }
        return;
    }

    // Movimiento lento hacia Bomberman
    float step = speed * deltaTime;
    EnemyDirection toPlayer = directionTowardPlayer();

    if (toPlayer != EnemyDirection::NONE) {
        if (!tryMove(toPlayer, step)) {
            // Intentar dirección secundaria (eje perpendicular)
            float d;
            glm::vec2 targetPos = getClosestPlayerPos(d);
            glm::vec2 diff = (d < 99999.0f) ? (targetPos - position) : glm::vec2(0.0f);
            EnemyDirection alt;
            if (std::abs(diff.x) > std::abs(diff.y)) {
                alt = (diff.y > 0.0f) ? EnemyDirection::UP : EnemyDirection::DOWN;
            } else {
                alt = (diff.x > 0.0f) ? EnemyDirection::RIGHT : EnemyDirection::LEFT;
            }
            tryMove(alt, step);
        }
    }
}

void FantasmaMortal::Draw() {
    // TODO: Renderizar sprite del Fantasma mortal
}
