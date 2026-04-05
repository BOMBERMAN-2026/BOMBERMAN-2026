#include "enemies/fantasma_mortal.hpp"
#include "game_map.hpp"
#include <cmath>
#include <queue>
#include <vector>
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

EnemyDirection FantasmaMortal::findPathToPlayer() const {
    if (!gameMap) return EnemyDirection::NONE;

    float dist;
    glm::vec2 targetPos = getClosestPlayerPos(dist);
    if (dist >= 99999.0f) return EnemyDirection::NONE;

    int startR, startC, targetR, targetC;
    gameMap->ndcToGrid(this->position, startR, startC);
    gameMap->ndcToGrid(targetPos, targetR, targetC);

    if (startR == targetR && startC == targetC) {
        return directionTowardPlayer();
    }

    int rows = gameMap->getRows();
    int cols = gameMap->getCols();
    std::vector<std::vector<bool>> visited(rows, std::vector<bool>(cols, false));

    struct Node {
        int r, c;
        EnemyDirection initialDir;
    };
    std::queue<Node> q;

    visited[startR][startC] = true;

    // Expandir vecinos del punto inicial
    int dr[] = {-1, 1, 0, 0}; // UP (-row), DOWN (+row)
    int dc[] = {0, 0, -1, 1}; // LEFT (-col), RIGHT (+col)
    EnemyDirection dirs[] = {EnemyDirection::UP, EnemyDirection::DOWN, EnemyDirection::LEFT, EnemyDirection::RIGHT};

    for (int i = 0; i < 4; ++i) {
        int nr = startR + dr[i];
        int nc = startC + dc[i];
        if (nr >= 0 && nr < rows && nc >= 0 && nc < cols) {
            if (gameMap->isWalkable(nr, nc) || gameMap->isDestructible(nr, nc)) {
                visited[nr][nc] = true;
                q.push({nr, nc, dirs[i]});
            }
        }
    }

    while (!q.empty()) {
        Node curr = q.front();
        q.pop();

        if (curr.r == targetR && curr.c == targetC) {
            return curr.initialDir;
        }

        for (int i = 0; i < 4; ++i) {
            int nr = curr.r + dr[i];
            int nc = curr.c + dc[i];
            if (nr >= 0 && nr < rows && nc >= 0 && nc < cols && !visited[nr][nc]) {
                if (gameMap->isWalkable(nr, nc) || gameMap->isDestructible(nr, nc)) {
                    visited[nr][nc] = true;
                    q.push({nr, nc, curr.initialDir});
                }
            }
        }
    }

    // Si no hay camino (ej. jugador completamente encerrado en un muro duro), volver a fallback direccional
    return directionTowardPlayer();
}

void FantasmaMortal::notifyBombNearby(glm::vec2 bombPos) {
    if (!alive || retreating) return;
    // Retroceder en dirección opuesta a la bomba
    glm::vec2 away = position - bombPos;
    float len = std::sqrt(away.x * away.x + away.y * away.y);
    if (len > 0.001f) {
        retreatDir = away / len;
    } else {
        retreatDir = dirToVec(randomDirection());
    }
    retreating = true;
    retreatTimer = 1.2f; // Retrocede durante 1.2 segundos
    speed = retreatSpeed;
}

void FantasmaMortal::Update() {
    if (!alive) return;

    if (isDying) {
        deathTimer -= deltaTime;
        animTimer += deltaTime;
        if (animTimer >= 0.1f) {
            animTimer = 0.0f;
            if (animFrame < 6) animFrame++; // Fantasma tiene 7 frames de muerte (0 al 6)
        }
        currentSpriteName = "fantasma.muerto." + std::to_string(animFrame);
        if (deathTimer <= 0.0f) {
            alive = false;
        }
        return;
    }

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

    float step = speed * deltaTime;
    // Pedir camino inteligente calculado por BFS (Atraviesa bloques destructibles)
    EnemyDirection toPlayer = findPathToPlayer();

    if (toPlayer != EnemyDirection::NONE) {
        if (!tryMove(toPlayer, step)) {
            // Si no puede moverse en la dirección óptima (ej. atascado en una esquina), 
            // forzar la alineación con el centro de la celda actual
            int tr, tc;
            gameMap->ndcToGrid(position, tr, tc);
            glm::vec2 center = gameMap->gridToNDC(tr, tc);

            bool moved = false;
            // Tolerancia aumentada para que pueda deslizarse bien
            float margin = 0.02f; 
            
            if (toPlayer == EnemyDirection::UP || toPlayer == EnemyDirection::DOWN) {
                if (position.x < center.x - margin) { 
                    moved = tryMove(EnemyDirection::RIGHT, step); 
                    facing = EnemyDirection::RIGHT; 
                } else if (position.x > center.x + margin) { 
                    moved = tryMove(EnemyDirection::LEFT, step); 
                    facing = EnemyDirection::LEFT; 
                }
            } else {
                if (position.y < center.y - margin) { 
                    moved = tryMove(EnemyDirection::UP, step); 
                    facing = EnemyDirection::UP; 
                } else if (position.y > center.y + margin) { 
                    moved = tryMove(EnemyDirection::DOWN, step); 
                    facing = EnemyDirection::DOWN; 
                }
            }

            if (!moved) {
                // Si ya está alineado y no puede avanzar, probablemente está contra una 
                // pared dura o en una colisión atípica. Intentar dirección aleatoria 
                // para desatascarse momentáneamente.
                if (!tryMove(facing, step)) {
                    facing = randomDirection();
                }
            }
        } else {
            facing = toPlayer;
        }
    } else {
        if (!tryMove(facing, step)) {
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
