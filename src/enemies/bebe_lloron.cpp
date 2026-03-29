#include "enemies/bebe_lloron.hpp"
#include "game_map.hpp"
#include <cstdlib>
#include <cmath>
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

    animTimer += deltaTime;
    if (animTimer >= 0.2f) {
        animTimer = 0.0f;
        animFrame ^= 1;
    }

    flipX = 0.0f;
    std::string prefix;
    switch (facing) {
        case EnemyDirection::LEFT:  prefix = "bebe.derecha."; flipX = 1.0f; break;
        default:                    prefix = "bebe.derecha."; break;
    }
    currentSpriteName = prefix + std::to_string(animFrame);
}

void BebeLloron::Draw() {
    if (!alive) return;

    // Tamaño más pequeño como soliticado
    const float enemyScaleFactor = 1.15f; 
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
