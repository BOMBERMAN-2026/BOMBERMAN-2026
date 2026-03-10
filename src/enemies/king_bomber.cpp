#include "enemies/king_bomber.hpp"
#include "game_map.hpp"
#include <cstdlib>
#include <cmath>

KingBomber::KingBomber(glm::vec2 pos, glm::vec2 size, float speed)
    : Enemy(pos, size, speed, /*hp=*/5, /*score=*/77000, /*passSoftBlocks=*/false, /*boss=*/true),
      shieldActive(true),
      shieldRecoverTimer(0.0f),
      shieldRecoverTime(5.0f),
      retreating(false),
      retreatTimer(0.0f),
      retreatDuration(3.0f),
      chainBombCooldown(0.0f),
      chainBombCooldownMax(6.0f),
      chainBombCount(3),
      droneFireCycle(10.0f),
      droneFireTimer(10.0f),
      droneFireDuration(4.0f)
{
    // Inicializar los 4 drones alrededor de la posición del jefe
    float offset = gameMap ? gameMap->getTileSize() * 0.5f : 0.1f;
    glm::vec2 offsets[4] = {
        { -offset, -offset }, {  offset, -offset },
        { -offset,  offset }, {  offset,  offset }
    };
    for (int i = 0; i < 4; ++i) {
        drones[i].position = pos + offsets[i];
        drones[i].speed = speed * 1.5f;
        drones[i].alive = true;
        drones[i].onFire = false;
        drones[i].fireTimer = 0.0f;
    }
}

KingBomber::~KingBomber() {}

void KingBomber::updateDrones() {
    if (!playerPos) return;

    droneFireTimer -= deltaTime;

    for (auto& drone : drones) {
        if (!drone.alive) continue;

        // Perseguir al jugador
        glm::vec2 diff = *playerPos - drone.position;
        float len = std::sqrt(diff.x * diff.x + diff.y * diff.y);
        if (len > 0.001f) {
            glm::vec2 moveDir = diff / len;
            glm::vec2 newPos = drone.position + moveDir * drone.speed * deltaTime;
            if (gameMap) {
                float halfTile = gameMap->getTileSize() / 2.0f;
                if (gameMap->canMoveTo(newPos, halfTile)) {
                    drone.position = newPos;
                }
            }
        }

        // Estado de fuego del dron
        if (drone.onFire) {
            drone.fireTimer -= deltaTime;
            if (drone.fireTimer <= 0.0f) {
                drone.onFire = false;
            }
        }
    }

    // Activar ciclo de fuego de todos los drones simultáneamente
    if (droneFireTimer <= 0.0f) {
        for (auto& drone : drones) {
            if (drone.alive) {
                drone.onFire = true;
                drone.fireTimer = droneFireDuration;
                // TODO: Enviar bolas de fuego en las 4 direcciones desde drone.position
            }
        }
        droneFireTimer = droneFireCycle;
    }
}

void KingBomber::updateShield() {
    if (shieldActive) return;

    shieldRecoverTimer -= deltaTime;
    if (shieldRecoverTimer <= 0.0f) {
        shieldActive = true;
        retreating = false;
    }
}

void KingBomber::placeChainBombs() {
    // TODO: Colocar una cadena de bombas en la dirección general del jugador
    chainBombCooldown = chainBombCooldownMax;
}

void KingBomber::onBombsExploded() {
    // Cuando sus propias bombas explotan, el escudo cae
    if (shieldActive) {
        shieldActive = false;
        startRetreat();
    }
}

void KingBomber::startRetreat() {
    retreating = true;
    retreatTimer = retreatDuration;
    shieldRecoverTimer = retreatDuration + shieldRecoverTime;
}

void KingBomber::Update() {
    if (!alive) return;

    float step = speed * deltaTime;

    // Actualizar subsistemas
    updateDrones();
    updateShield();

    if (chainBombCooldown > 0.0f) chainBombCooldown -= deltaTime;
    if (retreatTimer > 0.0f) retreatTimer -= deltaTime;

    if (retreating) {
        // Retirarse: moverse en dirección opuesta al jugador
        EnemyDirection away = oppositeDirection(directionTowardPlayer());
        if (!tryMove(away, step * 1.5f)) {
            tryMove(randomDirection(), step);
        }
        if (retreatTimer <= 0.0f) {
            retreating = false;
        }
    } else {
        // Moverse hacia el jugador colocando bombas en cadena
        EnemyDirection toPlayer = directionTowardPlayer();
        if (!tryMove(toPlayer, step)) {
            tryMove(randomDirection(), step);
        }

        // Colocar bombas periódicamente
        if (chainBombCooldown <= 0.0f) {
            placeChainBombs();
        }
    }
}

void KingBomber::Draw() {
    // TODO: Renderizar sprite de King Bomber (con efecto de escudo si shieldActive)
    // TODO: Renderizar los 4 drones también
}
