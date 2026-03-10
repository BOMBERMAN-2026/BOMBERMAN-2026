#include "enemies/babosa.hpp"
#include "game_map.hpp"
#include <cstdlib>

Babosa::Babosa(glm::vec2 pos, glm::vec2 size, float speed)
    : Enemy(pos, size, speed, /*hp=*/1, /*score=*/400),
      dirChangeTimer(0.0f),
      dirChangeInterval(1.0f) // Cambia de dirección frecuentemente
{
    facing = randomDirection();
}

Babosa::~Babosa() {}

void Babosa::Update() {
    if (!alive) return;

    float step = speed * deltaTime;

    if (!tryMove(facing, step)) {
        // Chocó: elige nueva dirección al azar
        facing = randomDirection();
    }

    // Cambio frecuente de dirección incluso sin chocar
    dirChangeTimer += deltaTime;
    if (dirChangeTimer >= dirChangeInterval) {
        dirChangeTimer = 0.0f;
        facing = randomDirection();
    }
}

void Babosa::Draw() {
    // TODO: Renderizar sprite de la Babosa
}
