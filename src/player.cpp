
#include "player.hpp"
#include "game_map.hpp"
#include <algorithm>
#include <cmath>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "sprite_atlas.hpp"
#include "bomb.hpp"
#include <iostream>

extern GLuint VAO;
extern GLuint uniformModel;
extern GLuint uniformUvRect;
extern GLuint uniformFlipX;
extern GLuint uniformTintColor;
extern SpriteAtlas gPlayerAtlas;
extern GLuint texture;
extern GameMap* gameMap;

/*
 * player.cpp
 * ----------
 * Implementación del `Player`.
 *
 * Nota:
 * - El movimiento real se aplica en `UpdateSprite` (según una dirección) y se valida
 *   contra el `GameMap` usando una combinación de sondas + `canMoveTo`.
 * - `Update`/`Draw` están como placeholders (la renderización se realiza desde Game).
 */

// ============================== Ctor / dtor ==============================

Player::Player(glm::vec2 pos, glm::vec2 size, GLfloat velocity, int playerId, const std::string& prefix)
    : Entity(pos, size, velocity),
      spritePrefix(prefix),
      currentSpriteName(prefix + ".abajo.0"),
      flipX(0.0f),
      facingDirKey(GLFW_KEY_DOWN),
      isWalking(false),
      walkTimer(0.0f),
      walkPhase(0),
      deltaTime(0.0f)
{
    this->playerId = playerId;
    spawnPosition = pos;
    baseSpeed = velocity;
    lifeState = PlayerLifeState::Alive;
    deathTimer = 0.0f;
    deathFrame = 0;
    pendingRespawn = false;
}

// Colisión con bombas (tile-based): el dueño puede salir una vez; luego también bloquea para él.
static bool bombBlocksCellForPlayer(int row, int col, int playerId) {
    for (auto* b : gBombs) {
        if (!b) continue;
        if (b->state == BombState::DONE) continue;
        if (b->gridRow == row && b->gridCol == col) {
            return b->blocksForPlayer(playerId);
        }
    }
    return false;
}

Player::~Player() {}

// ============================== Animación ==============================

static int walkPhaseToFrameIndex(int phase)
{
    switch (phase & 3) {
        case 0: return 0;
        case 1: return 1;
        case 2: return 0;
        case 3: return 2;
    }
    return 0;
}

void Player::setSpriteFromDirAndFrame(GLint dirKey, int frameIndex)
{
    std::string dirStr = "abajo";
    this->flipX = 0.0f; // Por defecto no espejar

    switch (dirKey) {
        case GLFW_KEY_UP:
            dirStr = "arriba"; break;
        case GLFW_KEY_DOWN:
            dirStr = "abajo"; break;
        case GLFW_KEY_LEFT:
            dirStr = "derecha";
            this->flipX = 1.0f; // Espejar horizontalmente para mirar a la izquierda
            break;
        case GLFW_KEY_RIGHT:
        default:
            dirStr = "derecha"; break;
    }
    this->currentSpriteName = this->spritePrefix + "." + dirStr + "." + std::to_string(frameIndex);
}

void Player::updateAnimation() {
    static constexpr float gWalkFrameInterval = 0.12f; // segundos por cambio de fase

    if (this->isWalking) {
        this->walkTimer += this->deltaTime;
        while (this->walkTimer >= gWalkFrameInterval) {
            this->walkTimer -= gWalkFrameInterval;
            this->walkPhase = (this->walkPhase + 1) & 3;
        }
        setSpriteFromDirAndFrame(this->facingDirKey, walkPhaseToFrameIndex(this->walkPhase));
    } else {
        this->walkTimer = 0.0f;
        this->walkPhase = 0;
        setSpriteFromDirAndFrame(this->facingDirKey, 0);
    }
}

void Player::updateDeathAnimation() {
    // Avanza frames de muerte según causa; al terminar respawnea.
    static constexpr float deathFrameInterval = 0.10f;

    int lastFrame = 0;
    const char* prefix = nullptr;
    if (lifeState == PlayerLifeState::DyingByEnemy) {
        prefix = nullptr; // usa spritePrefix
        lastFrame = 7;    // jugador(color).muerto.0..7
        flipX = 0.0f;
    } else if (lifeState == PlayerLifeState::DyingByExplosion) {
        prefix = "jugador.muerto.quemado.";
        lastFrame = 10;   // jugador.muerto.quemado.0..10
        flipX = 0.0f;
    } else {
        return;
    }

    deathTimer += deltaTime;
    while (deathTimer >= deathFrameInterval) {
        deathTimer -= deathFrameInterval;
        deathFrame++;
        if (deathFrame > lastFrame) {
            pendingRespawn = true;
            break;
        }
    }

    if (pendingRespawn) {
        respawn();
        return;
    }

    if (prefix) {
        currentSpriteName = std::string(prefix) + std::to_string(deathFrame);
    } else {
        currentSpriteName = spritePrefix + ".muerto." + std::to_string(deathFrame);
    }
}

// ============================== API base ==============================

// Tick de lógica: animación de caminar o de muerte + invincibilidad.
void Player::Update() {
    if (lifeState == PlayerLifeState::Alive) {
        // Invincibilidad: decrementar temporizador
        if (invincible) {
            invincibilityTimer -= deltaTime;
            if (invincibilityTimer <= 0.0f) {
                invincible = false;
                invincibilityTimer = 0.0f;
            }
        }
        updateAnimation();
    } else {
        updateDeathAnimation();
    }
}

// Render del sprite actual (el orden de pintado lo decide `bomberman.cpp`).
void Player::Draw() {
    const float playerScaleFactor = 1.8f;
    float halfTile = gameMap->getTileSize() / 2.0f;

    glm::mat4 model = glm::mat4(1.0f);
    glm::vec3 renderPos = glm::vec3(position.x, position.y + (playerScaleFactor - 1.0f) * halfTile * 0.8f, 0.0f);
    model = glm::translate(model, renderPos);

    glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
    bool okUv = getUvRectForSprite(gPlayerAtlas, currentSpriteName, uvRect);
    if (!okUv) {
        static bool warnedMissingSprite = false;
        if (!warnedMissingSprite) {
            warnedMissingSprite = true;
            std::cerr << "[SpriteAtlas] Sprite no encontrado: '" << currentSpriteName
                      << "'. Usando fallback.\n";
        }

        const std::string fb0 = spritePrefix + ".abajo.0";
        if (!getUvRectForSprite(gPlayerAtlas, fb0, uvRect)) {
            static bool warnedBase = false;
            if (!warnedBase) {
                warnedBase = true;
                std::cerr << "[SpriteAtlas] Falta sprite fallback '" << fb0 << "'.\n";
            }
        }
    }

    model = glm::scale(model, glm::vec3(halfTile * playerScaleFactor, halfTile * playerScaleFactor, 1.0f));

    glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
    glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));
    glUniform1f(uniformFlipX, flipX);

    glm::vec4 tint(1.0f, 1.0f, 1.0f, 1.0f);

    // Parpadeo visual cuando la invincibilidad está a punto de acabar (< 3s)
    if (invincible && invincibilityTimer < 3.0f) {
        // Parpadeo rápido: invisible en frames alternos
        int flickerPhase = (int)(invincibilityTimer * 10.0f);
        if (flickerPhase % 2 == 0) {
            tint.a = 0.3f;
        }
    } else if (invincible) {
        // Efecto de brillo sutil mientras es invencible
        tint = glm::vec4(1.2f, 1.2f, 1.0f, 1.0f);
    }

    glUniform4fv(uniformTintColor, 1, glm::value_ptr(tint));

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

// Aplica un paso de movimiento con colisión (mapa + bombas) y “snap” al centro del tile.
void Player::UpdateSprite(Move mov, const GameMap* map, float deltaTime) {
    if (!map) return;
    if (lifeState != PlayerLifeState::Alive) return;

    const float step     = this->speed * deltaTime;
    const float halfTile = map->getTileSize() / 2.0f;

    // --- Snap perpendicular al movimiento hacia el centro del tile actual ---
    // Impide que el jugador se cuele por las esquinas en pasillos de 1 tile.
    {
        const float snapStrength = 0.25f;         // fracción de corrección por frame
        const float snapMaxDist  = halfTile * 0.45f; // umbral máximo para aplicar
        int tr, tc;
        map->ndcToGrid(this->position, tr, tc);
        glm::vec2 tileCenter = map->gridToNDC(tr, tc);

        if (mov == MOVE_LEFT || mov == MOVE_RIGHT) {
            float dy = tileCenter.y - this->position.y;
            if (std::abs(dy) <= snapMaxDist)
                this->position.y += dy * snapStrength;
        } else {
            float dx = tileCenter.x - this->position.x;
            if (std::abs(dx) <= snapMaxDist)
                this->position.x += dx * snapStrength;
        }
    }

    glm::vec2 newPos = this->position;
    switch (mov) {
        case MOVE_UP:    newPos.y += step; break;
        case MOVE_DOWN:  newPos.y -= step; break;
        case MOVE_LEFT:  newPos.x -= step; break;
        case MOVE_RIGHT: newPos.x += step; break;
        default: return;
    }

    // --- Sondas de colisión: dos esquinas en el borde frontal de la dirección ---
    // Cada dirección comprueba ambas esquinas del lado que avanza.
    {
        int r, c;
        const float eFront = halfTile;          // siempre cae en el tile vecino
        const float eSide  = halfTile * 0.60f; // semiancho del hitbox

        if (mov == MOVE_UP) {
            map->ndcToGrid({newPos.x - eSide, newPos.y + eFront}, r, c);
            if (!map->isWalkable(r, c)) return;
            if (bombBlocksCellForPlayer(r, c, this->playerId)) return;
            map->ndcToGrid({newPos.x + eSide, newPos.y + eFront}, r, c);
            if (!map->isWalkable(r, c)) return;
            if (bombBlocksCellForPlayer(r, c, this->playerId)) return;
        }
        if (mov == MOVE_DOWN) {
            map->ndcToGrid({newPos.x - eSide, newPos.y - eFront}, r, c);
            if (!map->isWalkable(r, c)) return;
            if (bombBlocksCellForPlayer(r, c, this->playerId)) return;
            map->ndcToGrid({newPos.x + eSide, newPos.y - eFront}, r, c);
            if (!map->isWalkable(r, c)) return;
            if (bombBlocksCellForPlayer(r, c, this->playerId)) return;
        }
        if (mov == MOVE_LEFT) {
            map->ndcToGrid({newPos.x - eFront, newPos.y - eSide}, r, c);
            if (!map->isWalkable(r, c)) return;
            if (bombBlocksCellForPlayer(r, c, this->playerId)) return;
            map->ndcToGrid({newPos.x - eFront, newPos.y + eSide}, r, c);
            if (!map->isWalkable(r, c)) return;
            if (bombBlocksCellForPlayer(r, c, this->playerId)) return;
        }
        if (mov == MOVE_RIGHT) {
            map->ndcToGrid({newPos.x + eFront, newPos.y - eSide}, r, c);
            if (!map->isWalkable(r, c)) return;
            if (bombBlocksCellForPlayer(r, c, this->playerId)) return;
            map->ndcToGrid({newPos.x + eFront, newPos.y + eSide}, r, c);
            if (!map->isWalkable(r, c)) return;
            if (bombBlocksCellForPlayer(r, c, this->playerId)) return;
        }
    }

    this->position = newPos;
}

// Mata al jugador por contacto con enemigo (usa "jugador(color).muerto.N").
void Player::killByEnemy() {
    if (lifeState != PlayerLifeState::Alive) return;
    if (invincible) return; // Invencible: ignorar muerte
    lives--;
    lifeState = PlayerLifeState::DyingByEnemy;
    isWalking = false;
    walkTimer = 0.0f;
    walkPhase = 0;
    deathTimer = 0.0f;
    deathFrame = 0;
    pendingRespawn = false;
    flipX = 0.0f;
    currentSpriteName = spritePrefix + ".muerto.0";
}

// Mata al jugador por explosión (usa "jugador.muerto.quemado.N").
void Player::killByExplosion() {
    if (lifeState != PlayerLifeState::Alive) return;
    if (invincible) return; // Invencible: ignorar muerte
    lives--;
    lifeState = PlayerLifeState::DyingByExplosion;
    isWalking = false;
    walkTimer = 0.0f;
    walkPhase = 0;
    deathTimer = 0.0f;
    deathFrame = 0;
    pendingRespawn = false;
    flipX = 0.0f;
    currentSpriteName = "jugador.muerto.quemado.0";
}

// Vuelve al spawn y restaura estado. Castigo Arcade: pierde TODOS los power-ups.
void Player::respawn() {
    if (lives <= 0) {
        // Game Over: no respawnear
        return;
    }

    position = spawnPosition;
    lifeState = PlayerLifeState::Alive;

    facingDirKey = GLFW_KEY_DOWN;
    isWalking = false;
    walkTimer = 0.0f;
    walkPhase = 0;

    deathTimer = 0.0f;
    deathFrame = 0;
    pendingRespawn = false;

    flipX = 0.0f;
    currentSpriteName = spritePrefix + ".abajo.0";

    // === Castigo Arcade: reset total de stats ===
    maxBombs = 1;
    explosionPower = 2;
    speed = 0.4f;
    baseSpeed = 0.4f;
    hasRemoteControl = false;
    invincible = false;
    invincibilityTimer = 0.0f;
    // activeBombs no se resetea: las bombas en el mapa siguen existiendo
    // y decrementarán activeBombs cuando exploten.
}

// Aplica un power-up al jugador (respeta ArcadeCaps).
void Player::applyPowerUp(PowerUpType type) {
    switch (type) {
        case PowerUpType::ExtraLife:
            lives += 1;
            break;

        case PowerUpType::BombUp:
            maxBombs = std::min(maxBombs + 1, ArcadeCaps::MAX_BOMBS);
            break;

        case PowerUpType::FireUp:
            explosionPower = std::min(explosionPower + 1, ArcadeCaps::MAX_FIRE_POWER);
            break;

        case PowerUpType::SpeedUp:
            baseSpeed = std::min(baseSpeed + ArcadeCaps::SPEED_INCREMENT, ArcadeCaps::MAX_SPEED);
            speed = baseSpeed;
            break;

        case PowerUpType::Invincibility:
            invincible = true;
            invincibilityTimer = ArcadeCaps::INVINCIBILITY_TIME;
            break;

        case PowerUpType::RemoteControl:
            hasRemoteControl = true;
            break;
    }
}
