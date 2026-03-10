#include "enemies/bebe_lloron.hpp"
#include "game_map.hpp"
#include <cstdlib>
#include <cmath>

BebeLloron::BebeLloron(glm::vec2 pos, glm::vec2 size, float speed)
    : Enemy(pos, size, speed, /*hp=*/1, /*score=*/800),
      pursuitRange(0.4f),       // Empieza a perseguir si el jugador está cerca
      pursuitGiveUpRange(0.7f), // Deja de perseguir si se aleja demasiado
      pursuing(false),
      patrolTimer(0.0f),
      patrolInterval(2.0f)
{
    facing = randomDirection();
}

BebeLloron::~BebeLloron() {}

void BebeLloron::Update() {
    if (!alive) return;

    float dist = distanceToPlayer();
    float step = speed * deltaTime;

    // Transición entre patrulla y persecución
    if (!pursuing && dist < pursuitRange) {
        pursuing = true;
    } else if (pursuing && dist > pursuitGiveUpRange) {
        pursuing = false;
    }

    if (pursuing) {
        // Persecución ligera: se mueve hacia el jugador pero sin insistir mucho
        EnemyDirection toPlayer = directionTowardPlayer();
        if (!tryMove(toPlayer, step)) {
            // Si no puede ir directo, intenta una dirección perpendicular
            EnemyDirection alt = randomDirection();
            tryMove(alt, step);
        }
    } else {
        // Patrulla: camina en una dirección y cambia periódicamente
        if (!tryMove(facing, step)) {
            facing = randomDirection();
        }

        patrolTimer += deltaTime;
        if (patrolTimer >= patrolInterval) {
            patrolTimer = 0.0f;
            facing = randomDirection();
        }
    }
}

void BebeLloron::Draw() {
    // TODO: Renderizar sprite del Bebé llorón
}
