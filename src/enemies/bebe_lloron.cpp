#include "enemies/bebe_lloron.hpp"
#include "game_map.hpp"
#include <cstdlib>
#include <cmath>
#include <algorithm>
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

bool BebeLloron::hasLineOfSightToPlayer() const {
    if (!gameMap) return false;
    
    float dist;
    glm::vec2 targetPos = getClosestPlayerPos(dist);
    if (dist >= 99999.0f) return false;

    int r1, c1, r2, c2;
    gameMap->ndcToGrid(this->position, r1, c1);
    gameMap->ndcToGrid(targetPos, r2, c2);

    // Debe estar en la misma fila o misma columna para verse directo
    if (r1 != r2 && c1 != c2) return false;

    if (r1 == r2) {
        int cMin = std::min(c1, c2);
        int cMax = std::max(c1, c2);
        for (int c = cMin + 1; c < cMax; ++c) {
            if (!gameMap->isWalkable(r1, c)) return false;
        }
    } else {
        int rMin = std::min(r1, r2);
        int rMax = std::max(r1, r2);
        for (int r = rMin + 1; r < rMax; ++r) {
            if (!gameMap->isWalkable(r, c1)) return false;
        }
    }
    return true;
}

void BebeLloron::Update() {
    if (!alive) return;

    if (isDying) {
        deathTimer -= deltaTime;
        animTimer += deltaTime;
        if (animTimer >= 0.15f) {
            animTimer = 0.0f;
            if (animFrame < 4) animFrame++; // Bebé tiene 5 frames de muerte (0 al 4)
        }
        currentSpriteName = "bebe.muerto." + std::to_string(animFrame);
        if (deathTimer <= 0.0f) {
            alive = false;
        }
        return;
    }

    float dist = distanceToPlayer();
    float step = speed * deltaTime;

    // Transición entre patrulla y persecución
    if (!pursuing && dist < pursuitRange && hasLineOfSightToPlayer()) {
        pursuing = true;
    } else if (pursuing && (dist > pursuitGiveUpRange || !hasLineOfSightToPlayer())) {
        pursuing = false;
    }

    if (pursuing) {
        float d;
        glm::vec2 targetPos = getClosestPlayerPos(d);
        glm::vec2 diff = targetPos - position;

        EnemyDirection primary, secondary;
        if (std::abs(diff.x) > std::abs(diff.y)) {
            primary = (diff.x > 0.0f) ? EnemyDirection::RIGHT : EnemyDirection::LEFT;
            secondary = (diff.y > 0.0f) ? EnemyDirection::UP : EnemyDirection::DOWN;
        } else {
            primary = (diff.y > 0.0f) ? EnemyDirection::UP : EnemyDirection::DOWN;
            secondary = (diff.x > 0.0f) ? EnemyDirection::RIGHT : EnemyDirection::LEFT;
        }

        // 1. Intentar moverse en el eje principal (línea recta hacia el jugador).
        if (!tryMove(primary, step)) {
            // 2. Si choca contra una esquina o un bloque por no estar alienado,
            // moverse en el eje secundario para "centrarse" en el pasillo.
            if (!tryMove(secondary, step)) {
                // 3. Si aún así está completamente atascado en ambas direcciones,
                // forzar su alineación estricta hacia el centro de su propia casilla.
                int tr, tc;
                gameMap->ndcToGrid(position, tr, tc);
                glm::vec2 center = gameMap->gridToNDC(tr, tc);

                if (primary == EnemyDirection::UP || primary == EnemyDirection::DOWN) {
                    if (position.x < center.x - 0.02f) tryMove(EnemyDirection::RIGHT, step);
                    else if (position.x > center.x + 0.02f) tryMove(EnemyDirection::LEFT, step);
                } else {
                    if (position.y < center.y - 0.02f) tryMove(EnemyDirection::UP, step);
                    else if (position.y > center.y + 0.02f) tryMove(EnemyDirection::DOWN, step);
                }
            }
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

    if (!gameMap) return;

    // Tamaño visual (en tiles) del enemigo.
    // El sprite del bebé en el atlas es 16x32, así que con ~2 tiles de alto se verá
    // "alto y delgado" como en el arcade.
    const float enemyHeightInTiles = 1.95f;
    float halfTile = gameMap->getTileSize() / 2.0f;

    glm::vec3 renderPos = glm::vec3(position.x, position.y, 0.0f);

    glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
    getUvRectForSprite(gEnemyAtlas, currentSpriteName, uvRect);

    // Respetar el aspect ratio real del sprite del atlas (p.ej. bebé: 16x32).
    const float targetHeightScale = halfTile * enemyHeightInTiles;
    float scaleX = targetHeightScale;
    float scaleY = targetHeightScale;
    {
        auto it = gEnemyAtlas.sprites.find(currentSpriteName);
        if (it != gEnemyAtlas.sprites.end()) {
            const SpriteFrame& frame = it->second;
            if (frame.h > 0) {
                scaleX = (targetHeightScale * (float)frame.w) / (float)frame.h;
                scaleY = targetHeightScale;
            }
        }
    }

    // Ajuste vertical según el tamaño real del sprite (ancla aproximada al suelo).
    renderPos.y = position.y + (scaleY - halfTile) * 0.8f;
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, renderPos);
    model = glm::scale(model, glm::vec3(scaleX, scaleY, 1.0f));

    glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
    glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));
    glUniform1f(uniformFlipX, flipX);
    glm::vec4 tint(1.0f, 1.0f, 1.0f, 1.0f);
    glUniform4fv(uniformTintColor, 1, glm::value_ptr(tint));

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}
