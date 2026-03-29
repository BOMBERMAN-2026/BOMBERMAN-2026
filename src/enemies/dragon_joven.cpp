#include "enemies/dragon_joven.hpp"
#include "game_map.hpp"
#include <cstdlib>
#include <cmath>

DragonJoven::DragonJoven(glm::vec2 pos, glm::vec2 size, float speed)
    : Enemy(pos, size, speed, /*hp=*/1, /*score=*/400),
      fireCooldown(0.0f),
      fireCooldownMax(3.0f),  // Cada 3 segundos puede escupir fuego
      fireRange(2),
      isFiring(false),
      fireAnimTimer(0.0f)
{
    facing = randomDirection();
}

DragonJoven::~DragonJoven() {}

EnemyDirection DragonJoven::checkLineOfSight() const {
    if (!playersList || !gameMap) return EnemyDirection::NONE;
    
    float dist;
    glm::vec2 targetPos = getClosestPlayerPos(dist);
    if (dist >= 99999.0f) return EnemyDirection::NONE;

    int enemyRow, enemyCol, playerRow, playerCol;
    gameMap->ndcToGrid(position, enemyRow, enemyCol);
    gameMap->ndcToGrid(targetPos, playerRow, playerCol);

    float tileSize = gameMap->getTileSize();

    // Misma fila
    if (enemyRow == playerRow) {
        int dist = std::abs(playerCol - enemyCol);
        if (dist > 0 && dist <= fireRange) {
            // Comprobar que no haya bloques entre medias
            int step = (playerCol > enemyCol) ? 1 : -1;
            bool clear = true;
            for (int c = enemyCol + step; c != playerCol; c += step) {
                if (!gameMap->isWalkable(enemyRow, c)) {
                    clear = false;
                    break;
                }
            }
            if (clear) {
                return (playerCol > enemyCol) ? EnemyDirection::RIGHT : EnemyDirection::LEFT;
            }
        }
    }

    // Misma columna
    if (enemyCol == playerCol) {
        int dist = std::abs(playerRow - enemyRow);
        if (dist > 0 && dist <= fireRange) {
            int step = (playerRow > enemyRow) ? 1 : -1;
            bool clear = true;
            for (int r = enemyRow + step; r != playerRow; r += step) {
                if (!gameMap->isWalkable(r, enemyCol)) {
                    clear = false;
                    break;
                }
            }
            if (clear) {
                return (playerRow > enemyRow) ? EnemyDirection::UP : EnemyDirection::DOWN;
            }
        }
    }

    return EnemyDirection::NONE;
}

void DragonJoven::Update() {
    if (!alive) return;

    float step = speed * deltaTime;

    // Actualizar cooldown de fuego
    if (fireCooldown > 0.0f) {
        fireCooldown -= deltaTime;
    }

    // Si está disparando, esperar a que termine la animación
    if (isFiring) {
        fireAnimTimer -= deltaTime;
        if (fireAnimTimer <= 0.0f) {
            isFiring = false;
        }
        return; // No se mueve mientras escupe fuego
    }

    // Comprobar línea de visión para escupir fuego
    if (fireCooldown <= 0.0f) {
        EnemyDirection fireDir = checkLineOfSight();
        if (fireDir != EnemyDirection::NONE) {
            isFiring = true;
            fireAnimTimer = 0.5f;     // Duración de la animación de fuego
            fireCooldown = fireCooldownMax;
            facing = fireDir;
            // TODO: Crear proyectil/efecto de fuego con alcance de 2 casillas
            return;
        }
    }

    // Movimiento básico de patrulla (similar a los primeros enemigos)
    if (!tryMove(facing, step)) {
        facing = randomDirection();
    }
}

void DragonJoven::Draw() {
    // TODO: Renderizar sprite del Dragón joven (con animación de fuego si isFiring)
}
