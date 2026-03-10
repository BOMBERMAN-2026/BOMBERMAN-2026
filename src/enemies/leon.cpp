#include "enemies/leon.hpp"
#include "game_map.hpp"
#include <cstdlib>

Leon::Leon(glm::vec2 pos, glm::vec2 size, float speed)
    : Enemy(pos, size, speed, /*hp=*/1, /*score=*/100),
      dirChangeTimer(0.0f),
      dirChangeInterval(3.0f),  // Tarda bastante en plantearse cambiar
      dirChangeChance(0.15f)    // Solo 15 % de probabilidad al chocar
{
    facing = randomDirection();
}

Leon::~Leon() {}

void Leon::Update() {
    if (!alive) return;

    float step = speed * deltaTime;

    // Intentar avanzar en la dirección actual
    if (!tryMove(facing, step)) {
        // Chocó: cambiar de dirección con probabilidad baja
        float roll = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
        if (roll < dirChangeChance) {
            facing = randomDirection();
        } else {
            // Si no cambia al azar, simplemente dar la vuelta
            facing = oppositeDirection(facing);
        }
    }

    // Temporizador: ocasionalmente cambia de dirección sin chocar
    dirChangeTimer += deltaTime;
    if (dirChangeTimer >= dirChangeInterval) {
        dirChangeTimer = 0.0f;
        float roll = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
        if (roll < dirChangeChance) {
            facing = randomDirection();
        }
    }
}

void Leon::Draw() {
    // TODO: Renderizar sprite del León usando su atlas/textura correspondiente
}
