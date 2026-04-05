#include "enemies/sol_pervertido.hpp"
#include "game_map.hpp"
#include <cstdlib>
#include <cmath>

static float randomFloat(float lo, float hi) {
    return lo + static_cast<float>(std::rand()) / (static_cast<float>(RAND_MAX) / (hi - lo));
}

SolPervertido::SolPervertido(glm::vec2 pos, glm::vec2 size, float speed, Phase phase)
    : Enemy(pos, size, speed, /*hp=*/3, /*score=*/5000, /*passSoftBlocks=*/false, /*boss=*/true),
      currentPhase(phase)
{
    // Velocidad de rebote inicial en diagonal (estilo pinball)
    float angle = randomFloat(0.3f, 1.2f); // ángulo aleatorio para que no sea trivial
    velocity = glm::vec2(std::cos(angle), std::sin(angle)) * speed;

    // Ajustar HP y velocidad según la fase
    switch (currentPhase) {
        case Phase::FULL:
            hitPoints = 3;
            break;
        case Phase::HALF:
            hitPoints = 2;
            velocity *= 1.3f;  // Más rápido
            break;
        case Phase::QUARTER:
            hitPoints = 1;
            velocity *= 1.6f;  // Aún más rápido
            break;
    }
    maxHitPoints = hitPoints;
}

SolPervertido::~SolPervertido() {}

// Rebote tipo pinball contra los límites del mapa.
void SolPervertido::bounce() {
    if (!gameMap) return;
    float halfTile = gameMap->getTileSize() / 2.0f;

    // Probar desplazamiento en X
    glm::vec2 probeX = position + glm::vec2(velocity.x * deltaTime, 0.0f);
    if (!gameMap->canMoveTo(probeX, halfTile)) {
        velocity.x = -velocity.x;
    }

    // Probar desplazamiento en Y
    glm::vec2 probeY = position + glm::vec2(0.0f, velocity.y * deltaTime);
    if (!gameMap->canMoveTo(probeY, halfTile)) {
        velocity.y = -velocity.y;
    }
}

// Tick de IA/movimiento (rebote + avance).
void SolPervertido::Update() {
    if (lifeState != EnemyLifeState::Alive) return;

    bounce();
    position += velocity * deltaTime;
}

// Render del enemigo (pendiente).
void SolPervertido::Draw() {
    // TODO: Renderizar sprite del Sol pervertido según la fase actual
}

// Divide el boss en fases menores y devuelve los hijos.
std::vector<std::unique_ptr<SolPervertido>> SolPervertido::split() {
    std::vector<std::unique_ptr<SolPervertido>> children;

    if (currentPhase == Phase::FULL) {
        // Se divide en 2 mitades
        float offset = gameMap ? gameMap->getTileSize() * 0.3f : 0.05f;
        auto half1 = std::unique_ptr<SolPervertido>(
            new SolPervertido(position + glm::vec2(-offset, 0.0f), size * 0.7f, speed * 1.3f, Phase::HALF));
        auto half2 = std::unique_ptr<SolPervertido>(
            new SolPervertido(position + glm::vec2( offset, 0.0f), size * 0.7f, speed * 1.3f, Phase::HALF));
        if (gameMap && playersList) {
            half1->setContext(gameMap, playersList);
            half2->setContext(gameMap, playersList);
        }
        children.push_back(std::move(half1));
        children.push_back(std::move(half2));

    } else if (currentPhase == Phase::HALF) {
        // Se divide en 4 cuartos (2 por mitad → 4 total entre las 2 mitades = 8)
        float offset = gameMap ? gameMap->getTileSize() * 0.2f : 0.03f;
        glm::vec2 offsets[4] = {
            { -offset, -offset }, {  offset, -offset },
            { -offset,  offset }, {  offset,  offset }
        };
        for (int i = 0; i < 4; ++i) {
            auto quarter = std::unique_ptr<SolPervertido>(
                new SolPervertido(position + offsets[i], size * 0.5f, speed * 1.6f, Phase::QUARTER));
            if (gameMap && playersList) {
                quarter->setContext(gameMap, playersList);
            }
            children.push_back(std::move(quarter));
        }
    }
    // Phase::QUARTER → no se subdivide más, simplemente muere

    lifeState = EnemyLifeState::Dead;
    alive = false;
    return children;
}
