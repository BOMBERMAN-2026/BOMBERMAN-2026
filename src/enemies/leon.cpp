#include "enemies/leon.hpp"
#include "game_map.hpp"
#include <cstdlib>
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "sprite_atlas.hpp"

extern GLuint VAO;
extern GLuint uniformModel;
extern GLuint uniformUvRect;
extern GLuint uniformFlipX;
extern GLuint uniformTintColor;
extern SpriteAtlas gEnemyAtlas;

Leon::Leon(glm::vec2 pos, glm::vec2 size, float speed)
    : Enemy(pos, size, speed, /*hp=*/1, /*score=*/100),
      dirChangeTimer(0.0f),
      dirChangeInterval(3.0f),  // Tarda bastante en plantearse cambiar
      dirChangeChance(0.15f)    // Solo 15 % de probabilidad al chocar
{
    facing = randomDirection();
}

Leon::~Leon() {}

// IA: patrulla básica con cambios de dirección poco frecuentes.
void Leon::Update() {
    if (lifeState != EnemyLifeState::Alive) return;

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

    animTimer += deltaTime;
    if (animTimer >= 0.2f) {
        animTimer = 0.0f;
        animFrame ^= 1;
    }

    flipX = 0.0f;
    std::string prefix;
    switch (facing) {
        case EnemyDirection::RIGHT: prefix = "leon.derecha."; break;
        case EnemyDirection::LEFT:  prefix = "leon.derecha."; flipX = 1.0f; break;
        case EnemyDirection::UP:    prefix = "leon.arriba.";  break;
        case EnemyDirection::DOWN:  prefix = "leon.abajo.";   break;
        default:                    prefix = "leon.derecha."; break;
    }
    currentSpriteName = prefix + std::to_string(animFrame);
}

// Render del enemigo (sprite según dirección + flip).
void Leon::Draw() {
    if (lifeState == EnemyLifeState::Dead) return;

    const float enemyScaleFactor = 1.8f;
    float halfTile = gameMap->getTileSize() / 2.0f;

    glm::mat4 model = glm::mat4(1.0f);
    glm::vec3 renderPos = glm::vec3(position.x, position.y + (enemyScaleFactor - 1.0f) * halfTile * 0.8f, 0.0f);
    model = glm::translate(model, renderPos);   

    glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
    getUvRectForSprite(gEnemyAtlas, currentSpriteName, uvRect);       

    model = glm::scale(model, glm::vec3(halfTile * enemyScaleFactor, halfTile * enemyScaleFactor, 1.0f));     

    glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
    glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));
    glUniform1f(uniformFlipX, flipX);
    glm::vec4 tint(1.0f, 1.0f, 1.0f, 1.0f);
    glUniform4fv(uniformTintColor, 1, glm::value_ptr(tint));

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}
