#include "enemies/fantasma_mortal.hpp"
#include "game_map.hpp"
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
            facing = alt;
        } else {
            facing = toPlayer;
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
        case EnemyDirection::RIGHT: prefix = "fantasma.derecha."; break;
        case EnemyDirection::LEFT:  prefix = "fantasma.derecha."; flipX = 1.0f; break;
        case EnemyDirection::UP:    prefix = "fantasma.derecha."; break; // No tiene sprites arriba/abajo específicos
        case EnemyDirection::DOWN:  prefix = "fantasma.derecha."; break;
        default:                    prefix = "fantasma.derecha."; break;
    }
    currentSpriteName = prefix + std::to_string(animFrame);
}

void FantasmaMortal::Draw() {
    if (!alive) return;

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
    
    // Tint rojizo si está huyendo (retreating)
    glm::vec4 tint = retreating ? glm::vec4(1.0f, 0.5f, 0.5f, 1.0f) : glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    glUniform4fv(uniformTintColor, 1, glm::value_ptr(tint));

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}
