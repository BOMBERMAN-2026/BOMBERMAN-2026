#include "enemies/sol_pervertido.hpp"
#include "game_map.hpp"
#include <cstdlib>
#include <cmath>
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "sprite_atlas.hpp"

extern GLuint uniformModel;
extern GLuint uniformUvRect;
extern GLuint uniformFlipX;
extern GLuint uniformTintColor;
extern SpriteAtlas gEnemyAtlas;

static float randomFloat(float lo, float hi) {
    return lo + static_cast<float>(std::rand()) / (static_cast<float>(RAND_MAX) / (hi - lo));
}

// Velocidad de rebote inicial en diagonal (estilo pinball).
SolPervertido::SolPervertido(glm::vec2 pos, glm::vec2 size, float speed, Phase phase)
    : Enemy(pos, size, speed, /*hp=*/3, /*score=*/5000, /*passSoftBlocks=*/false, /*boss=*/true),
        currentPhase(phase), invulnerableTimer(phase != Phase::FULL ? 0.8f : 0.0f)
{
    float angle = randomFloat(0.1f, 1.4f); // angulo aleatorio
    
    // Incremento de velocidad principal
    float actualSpeed = speed * (phase == Phase::FULL ? 3.0f : 1.0f);
    velocity = glm::vec2(std::cos(angle), std::sin(angle)) * actualSpeed;

    // Ajustar propiedades y velocidad según la fase
    hitPoints = 1;
    maxHitPoints = 1;
    
    std::string prefix;
    switch (currentPhase) {
        case Phase::FULL:
            prefix = "sol.grande.";
            break;
        case Phase::HALF:
            velocity *= 1.5f;  // Más rápido
            prefix = "sol.mediano.";
            break;
        case Phase::QUARTER:
            velocity *= 2.0f;  // Aún más rápido
            prefix = "sol.pequeño.";
            break;
    }
    currentSpriteName = prefix + "0";
}

SolPervertido::~SolPervertido() {}

// Rebote tipo pinball contra los límites del mapa.
void SolPervertido::bounce() {
    if (!gameMap) return;
    float radius = gameMap->getTileSize() * 0.45f;

    // Probar desplazamiento en X
    glm::vec2 probeX = position + glm::vec2(velocity.x * deltaTime, 0.0f);
    int r, c;
    bool hitX = false;
    for (float dx : {-radius, radius}) {
        for (float dy : {-radius, radius}) {
            gameMap->ndcToGrid(probeX + glm::vec2(dx, dy), r, c);
            // Solo colisionar con los bordes (fila 0/N, col 0/N)
            if (r <= 0 || r >= gameMap->getRows() - 1 || 
                c <= 0 || c >= gameMap->getCols() - 1) {
                hitX = true;
                break;
            }
        }
        if (hitX) break;
    }
    if (hitX) {
        velocity.x = -velocity.x;
    }

    // Probar desplazamiento en Y
    glm::vec2 probeY = position + glm::vec2(0.0f, velocity.y * deltaTime);
    bool hitY = false;
    for (float dx : {-radius, radius}) {
        for (float dy : {-radius, radius}) {
            gameMap->ndcToGrid(probeY + glm::vec2(dx, dy), r, c);
            if (r <= 0 || r >= gameMap->getRows() - 1 || 
                c <= 0 || c >= gameMap->getCols() - 1) {
                hitY = true;
                break;
            }
        }
        if (hitY) break;
    }
    if (hitY) {
        velocity.y = -velocity.y;
    }
}

bool SolPervertido::takeDamage(const SpriteAtlas& atlas, int amount) {
    if (lifeState != EnemyLifeState::Alive) return false;
    if (invulnerableTimer > 0.0f) return false; // Inmune temporalmente después de dividirse

    hitPoints -= 1;

    if (hitPoints <= 0) {
        hitPoints = 0;

        // Al morir, generamos hijos (subdivisiones)
        auto children = split();
        if (!children.empty()) {
            extern std::vector<Enemy*> gEnemies;
            for (auto& child : children) {
                gEnemies.push_back(child.release());
            }
            // Hacemos que muera instantáneamente para no hacer animación y ser recolectado
            lifeState = EnemyLifeState::Dead;
        } else {
            // Fase final: no se divide más, empezamos animación de muerte
            lifeState = EnemyLifeState::Dying;
            alive = true;    // Asegurar que siga visible durante la animación
            animFrame = 0;   // Empezamos en el frame 0 de la secuencia "muerto"
            animTimer = 0.0f;
            currentSpriteName = "sol.pequeño.muerto.0";
        }

        return true;
    }

    return false; // Sobrevivió el impacto en esta fase
}

// Muerte del sol pequeño
void SolPervertido::updateDeath(float dt) {
    if (lifeState != EnemyLifeState::Dying) return;

    animTimer += dt;
    if (animTimer >= 0.08f) { // Animación de muerte rápida
        animTimer = 0.0f;
        animFrame++;
        
        if (animFrame > 10) { // El último frame de muerto es el 10 (11 frames en total)
            lifeState = EnemyLifeState::Dead;
            alive = false;
            return;
        }
    }
    currentSpriteName = "sol.pequeño.muerto." + std::to_string(animFrame);
}

// Tick de IA/movimiento (rebote + avance).
void SolPervertido::Update() {
    if (lifeState != EnemyLifeState::Alive) return;

    if (invulnerableTimer > 0.0f) {
        invulnerableTimer -= deltaTime;
    }

    bounce();
    position += velocity * deltaTime;

    animTimer += deltaTime;
    if (animTimer >= 0.15f) {
        animTimer = 0.0f;
        int maxFrames = (currentPhase == Phase::QUARTER) ? 3 : 4;
        animFrame = (animFrame + 1) % maxFrames; // 3 frames si es pequeño, 4 si mediano/grande
    }

    std::string prefix;
    switch (currentPhase) {
        case Phase::FULL:    prefix = "sol.grande."; break;
        case Phase::HALF:    prefix = "sol.mediano."; break;
        case Phase::QUARTER: prefix = "sol.pequeño."; break;
    }
    currentSpriteName = prefix + std::to_string(animFrame);
}

// Render del enemigo (pendiente).
void SolPervertido::Draw() {
    if (!alive) return;

    float baseScale = 3.0f; 
    if (currentPhase == Phase::HALF) baseScale = 2.1f;
    if (currentPhase == Phase::QUARTER) baseScale = 1.5f;

    float halfTile = gameMap->getTileSize() / 2.0f;

    glm::mat4 model = glm::mat4(1.0f);
    glm::vec3 renderPos = glm::vec3(position.x, position.y + (baseScale - 1.0f) * halfTile * 0.5f, 0.0f);
    model = glm::translate(model, renderPos);

    glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
    getUvRectForSprite(gEnemyAtlas, currentSpriteName, uvRect);

    model = glm::scale(model, glm::vec3(halfTile * baseScale, halfTile * baseScale, 1.0f));

    glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
    glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));
    glUniform1f(uniformFlipX, 0.0f); // Sol no lo giramos
    glm::vec4 tint(1.0f, 1.0f, 1.0f, 1.0f);
    glUniform4fv(uniformTintColor, 1, glm::value_ptr(tint));

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
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
