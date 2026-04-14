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
      bombTurnCooldown(0.0f),
      bombEscapeDir(EnemyDirection::NONE),
      bombAvoidTimer(0.0f),
      lastBombPos(pos)
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

    const int rows = gameMap->getRows();
    const int cols = gameMap->getCols();
    if (rows <= 0 || cols <= 0) return directionTowardPlayer();
    if (startR < 0 || startR >= rows || startC < 0 || startC >= cols) return directionTowardPlayer();
    if (targetR < 0 || targetR >= rows || targetC < 0 || targetC >= cols) return directionTowardPlayer();

    if (startR == targetR && startC == targetC) {
        return directionTowardPlayer();
    }

    std::vector<std::vector<bool>> visited(rows, std::vector<bool>(cols, false));

    struct Node {
        int r, c;
        EnemyDirection initialDir;
    };
    std::queue<Node> q;

    visited[startR][startC] = true;

    // Expandir vecinos del punto inicial.
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

    // Si no hay camino (p. ej., jugador completamente encerrado), usar un fallback direccional.
    return directionTowardPlayer();
}

// Respuesta ante una bomba cercana: fija una dirección de escape temporal, sin buff de velocidad.
void FantasmaMortal::notifyBombNearby(glm::vec2 bombPos) {
    if (lifeState != EnemyLifeState::Alive) return;
    if (bombTurnCooldown > 0.0f) return;

    lastBombPos = bombPos;
    bombAvoidTimer = 1.35f;

    // Elegir una dirección cardinal estable alejándose de la bomba.
    glm::vec2 away = position - bombPos;
    EnemyDirection awayDir = EnemyDirection::NONE;
    if (std::abs(away.x) > std::abs(away.y)) {
        awayDir = (away.x >= 0.0f) ? EnemyDirection::RIGHT : EnemyDirection::LEFT;
    } else {
        awayDir = (away.y >= 0.0f) ? EnemyDirection::UP : EnemyDirection::DOWN;
    }

    // Si no puede ir en la dirección ideal, probar alternativas estables (sin flip-flop).
    EnemyDirection selected = EnemyDirection::NONE;
    const float lookAhead = gameMap ? (gameMap->getTileSize() * 0.95f) : 0.1f;
    if (awayDir != EnemyDirection::NONE && canMoveInDirection(awayDir, lookAhead)) {
        selected = awayDir;
    } else {
        const EnemyDirection opposite = oppositeDirection(awayDir);
        EnemyDirection perp1 = EnemyDirection::LEFT;
        EnemyDirection perp2 = EnemyDirection::RIGHT;
        if (awayDir == EnemyDirection::LEFT || awayDir == EnemyDirection::RIGHT) {
            perp1 = EnemyDirection::UP;
            perp2 = EnemyDirection::DOWN;
        }

        if (opposite != EnemyDirection::NONE && canMoveInDirection(opposite, lookAhead)) {
            selected = opposite;
        } else if (canMoveInDirection(perp1, lookAhead)) {
            selected = perp1;
        } else if (canMoveInDirection(perp2, lookAhead)) {
            selected = perp2;
        }
    }

    if (selected == EnemyDirection::NONE) {
        selected = (facing != EnemyDirection::NONE) ? oppositeDirection(facing) : randomDirection();
    }

    bombEscapeDir = selected;
    facing = bombEscapeDir;

    // Cancelar movimiento al target actual para que el giro tenga efecto inmediato.
    movingToTarget = false;
    bombTurnCooldown = 0.50f;
}

// Lógica de IA: persigue al jugador; ante bomba solo gira.
void FantasmaMortal::Update() {
    if (lifeState != EnemyLifeState::Alive) return;

    if (bombTurnCooldown > 0.0f) {
        bombTurnCooldown -= deltaTime;
        if (bombTurnCooldown < 0.0f) bombTurnCooldown = 0.0f;
    }
    if (bombAvoidTimer > 0.0f) {
        bombAvoidTimer -= deltaTime;
        if (bombAvoidTimer < 0.0f) bombAvoidTimer = 0.0f;
    }

    // Animación de movimiento: alterna entre frames 0 y 1.
    animTimer += deltaTime;
    if (animTimer >= 0.20f) {
        animTimer = 0.0f;
        animFrame ^= 1;
    }

    float step = speed * deltaTime;

    // Durante una ventana corta tras ver bomba, mantener la dirección elegida
    // para evitar oscilación izquierda-derecha.
    if (bombTurnCooldown > 0.0f && bombEscapeDir != EnemyDirection::NONE) {
        facing = bombEscapeDir;
        // Durante la ventana de reacción no se recalcula ruta, solo intenta avanzar en esa dirección.
        tryMove(facing, step);

        switch (facing) {
            case EnemyDirection::LEFT:
            case EnemyDirection::UP:
                flipX = 1.0f;
                break;
            case EnemyDirection::RIGHT:
            case EnemyDirection::DOWN:
                flipX = 0.0f;
                break;
            default:
                break;
        }
        currentSpriteName = std::string("fantasma.derecha.") + std::to_string(animFrame);
        return;
    }

    bombEscapeDir = EnemyDirection::NONE;

    // Ventana de evitación: seguir alejándose de la última bomba vista,
    // evitando volver hacia ella inmediatamente al salir del rango.
    if (bombAvoidTimer > 0.0f) {
        const float currentDist2 = glm::dot(position - lastBombPos, position - lastBombPos);
        EnemyDirection bestDir = EnemyDirection::NONE;
        float bestGain = -1e9f;

        const EnemyDirection candidates[4] = {
            EnemyDirection::UP, EnemyDirection::DOWN, EnemyDirection::LEFT, EnemyDirection::RIGHT
        };

        for (EnemyDirection d : candidates) {
            if (!canMoveInDirection(d, gameMap->getTileSize() * 0.95f)) continue;
            glm::vec2 probe = position + dirToVec(d) * gameMap->getTileSize();
            const float nextDist2 = glm::dot(probe - lastBombPos, probe - lastBombPos);
            const float gain = nextDist2 - currentDist2;
            if (gain > bestGain) {
                bestGain = gain;
                bestDir = d;
            }
        }

        if (bestDir != EnemyDirection::NONE && bestGain >= -0.0001f) {
            facing = bestDir;
            tryMove(facing, step);

            switch (facing) {
                case EnemyDirection::LEFT:
                case EnemyDirection::UP:
                    flipX = 1.0f;
                    break;
                case EnemyDirection::RIGHT:
                case EnemyDirection::DOWN:
                    flipX = 0.0f;
                    break;
                default:
                    break;
            }
            currentSpriteName = std::string("fantasma.derecha.") + std::to_string(animFrame);
            return;
        }
    }

    // Camino calculado por BFS (permite atravesar bloques destructibles).
    EnemyDirection toPlayer = findPathToPlayer();

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
        }
    }

    // Actualizar sprite (en el atlas solo hay "fantasma.derecha.N").
    // Política de orientación: al subir mira a la izquierda; al bajar mira a la derecha.
    switch (facing) {
        case EnemyDirection::LEFT:
        case EnemyDirection::UP:
            flipX = 1.0f;
            break;
        case EnemyDirection::RIGHT:
        case EnemyDirection::DOWN:
            flipX = 0.0f;
            break;
        default:
            break;
    }
    currentSpriteName = std::string("fantasma.derecha.") + std::to_string(animFrame);
}

// Renderiza el enemigo.
void FantasmaMortal::Draw() {
    if (!alive) return;

    const float enemyScaleFactor = 1.8f;
    float halfTile = gameMap->getTileSize() / 2.0f;

    glm::mat4 model = glm::mat4(1.0f);
    glm::vec3 renderPos = glm::vec3(position.x, position.y + (enemyScaleFactor - 1.0f) * halfTile * 0.8f, 0.0f);
    model = glm::translate(model, renderPos);   

    glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
    getUvRectForSprite(gEnemyAtlas, currentSpriteName, uvRect);       

    float aspect = 1.0f;
    auto it = gEnemyAtlas.sprites.find(currentSpriteName);
    if (it != gEnemyAtlas.sprites.end() && it->second.h > 0) {
        aspect = static_cast<float>(it->second.w) / static_cast<float>(it->second.h);
    }

    model = glm::scale(model, glm::vec3(halfTile * enemyScaleFactor * aspect, halfTile * enemyScaleFactor, 1.0f));

    glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
    glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));
    glUniform1f(uniformFlipX, flipX);
    
    glm::vec4 tint(1.0f, 1.0f, 1.0f, 1.0f);
    glUniform4fv(uniformTintColor, 1, glm::value_ptr(tint));

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}
