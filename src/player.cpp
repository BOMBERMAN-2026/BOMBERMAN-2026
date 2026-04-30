
#include "player.hpp"
#include "game_map.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "sprite_atlas.hpp"
#include "bomb.hpp"
#include "bomberman.hpp"
#include "versus_mode.hpp"
#include "score_popup.hpp"
#include <iostream>

extern class Game* bomberman;

extern GLuint VAO;
extern GLuint uniformModel;
extern GLuint uniformUvRect;
extern GLuint uniformFlipX;
extern GLuint uniformTintColor;
extern GLuint uniformWhiteFlash;
extern SpriteAtlas gPlayerAtlas;
extern SpriteAtlas gNextLevelAtlas;
extern GLuint texture;
extern GLuint gNextLevelTexture;
extern GameMap* gameMap;

namespace {

int scoreValueForItem(PowerUpType type) {
    switch (type) {
        case PowerUpType::Matches: return 5000;
        case PowerUpType::Can: return 10000;
        case PowerUpType::Lighter: return 30000;
        case PowerUpType::Battery: return 40000;
        case PowerUpType::Dragonfly: return 50000;
        case PowerUpType::HudsonBee: return 77000;
        default: return 0;
    }
}

}

/*
 * player.cpp
 * ----------
 * Implementación del `Player`.
 *
 * Nota:
 * - El movimiento real se aplica en `UpdateSprite` (según una dirección) y se valida
 *   contra el `GameMap` usando una combinación de sondas + `canMoveTo`.
 * - El render del jugador se hace desde `Game::render()` llamando a `Player::Draw()`.
 */

static constexpr float kSpawnInvulnerabilitySeconds = 2.5f;
static constexpr float kPowerUpInvulnerabilitySeconds = ArcadeCaps::INVINCIBILITY_TIME;

static float invulnBlinkHz(const Player& p)
{
    if (!p.invincible) return 0.0f;

    // Frecuencia de parpadeo (Hz) para el feedback visual.
    // Ciclo "Armadura" (power-up, 16s):
    // - Segundos 1..12: parpadeo moderado
    // - Segundos 13..16 (fase crítica): parpadeo muy rápido
    // Como `invincibilityTimer` es cuenta atrás:
    // - remaining > 4s  => moderado (primeros 12s)
    // - remaining <= 4s => frenético (últimos 4s)
    if (p.invincibilityFromPowerUp) {
        return (p.invincibilityTimer > 4.0f) ? 6.0f : 18.0f;
    }

    // Spawn/respawn: parpadeo moderado constante (ventana corta para reaparecer).
    return 8.0f;
}

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

    // Invulnerabilidad breve al aparecer/reaparecer.
    invincible = true;
    invincibilityTimer = kSpawnInvulnerabilitySeconds;
    invincibilityTotalSeconds = kSpawnInvulnerabilitySeconds;
    invincibilityFromPowerUp = false;
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

// Avanza frames de muerte según causa; al terminar respawnea.
void Player::updateDeathAnimation() {
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

    // ---- Efectos visuales 3D en paralelo (no bloquean el respawn) ----
    if (deathUse3DCelebration) {
        death3DTotalTimer += deltaTime;
        
        // Fase 0: caída, giro y vuelco (primeros ~0.9s)
        const float spinDuration = 0.80f;
        const float sinkDuration = 0.60f;
        const float glitterDelay = 0.20f;
        const float glitterDuration = 0.85f;

        if (death3DGlitterTimer < 0.0f) {
            const float t = std::min(1.0f, death3DTotalTimer / spinDuration);
            const float brakeT = 1.0f - t;
            const float spinRate = 60.0f * brakeT * brakeT;
            death3DSpin += deltaTime * spinRate;

            // Inclinación hacia adelante/suelo (vuelco)
            death3DTilt = std::min(1.57f, death3DTotalTimer * 2.5f); // 90 grados en ~0.6s

            // Hundimiento suave hacia el suelo
            const float sinkT = std::min(1.0f, death3DTotalTimer / sinkDuration);
            death3DHeight = -0.50f * sinkT;  // baja un poco para estar a ras de suelo tumbado

            // Escala
            death3DScale = 1.0f;

            // Arrancar glitter tras el delay
            if (death3DTotalTimer >= glitterDelay) {
                death3DGlitterBurst = true;
                death3DGlitterTimer = 0.0f;
            }
        } else {
            // Fase glitter: se queda en el suelo rojo (el color se aplica en el render)
            death3DGlitterTimer += deltaTime;
            const float t = std::min(1.0f, death3DGlitterTimer / glitterDuration);
            
            // Se queda tumbado
            death3DTilt = 1.57f;
            death3DHeight = -0.50f;
            
            // Pulso de escala leve mientras desaparece
            float pulse = 1.0f + 0.05f * std::sin(death3DGlitterTimer * 10.0f);
            death3DScale = pulse * (1.0f - t * 0.2f);

            if (death3DGlitterTimer >= glitterDuration) {
                deathUse3DCelebration = false;
            }
        }
    }
    // -----------------------------------------------------------------

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
        // Si no quedan vidas, nunca se respawnea.
        // Importante: fijar el sprite al último frame de muerte para no caer en el
        // sprite anterior (p.ej. mirando abajo) si el deltaTime hace saltar frames.
        const bool disableRespawnForVs =
            (bomberman != nullptr) && VersusMode::isVersusMode(bomberman->mode);
        if (lives <= 0 || disableRespawnForVs) {
            pendingRespawn = false;
            deathFrame = lastFrame;
            if (prefix) {
                currentSpriteName = std::string(prefix) + std::to_string(lastFrame);
            } else {
                currentSpriteName = spritePrefix + ".muerto." + std::to_string(lastFrame);
            }
            return;
        }

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


void Player::startWinning() {
    lifeState = PlayerLifeState::Winning;
    winStartPosition = position;
    isWalking = false;
    winTimer = 0.0f;
    winPhase = 0;
    winScale = 1.8f;
    hasFinishedWinning = false;
    invincible = true; // Para que no muera por explosiones tardías
    winUse3DCelebration = (bomberman != nullptr && bomberman->viewMode == ViewMode::Mode3D);
    winAnchorPosition = position;
    win3DHeight = 0.0f;
    win3DSpin = 0.0f;
    win3DScale = 1.0f;
    win3DGlitterTimer = -1.0f;
    win3DGlitterBurst = false;

    // Aleatorizar dirección para huir (diagonal hacia cualquier lado, evitando rectas)
    float dx = (std::rand() % 100 / 100.0f) * 2.0f - 1.0f; // -1 a 1
    float dy = (std::rand() % 100 / 100.0f) * 2.0f - 1.0f; // -1 a 1
    if (std::abs(dx) < 0.3f) dx = (dx < 0) ? -0.3f : 0.3f;
    if (std::abs(dy) < 0.3f) dy = (dy < 0) ? -0.3f : 0.3f;
    winVelocity = glm::normalize(glm::vec2(dx, dy)) * (baseSpeed * 2.5f);
}

void Player::updateWinningAnimation() {
    winTimer += deltaTime;

    if (winUse3DCelebration) {
        const float ascendDuration = 0.95f;
        const float hardStopDuration = 0.16f;
        const float glitterDuration = 0.95f;
        const float kPi = 3.14159265359f;

        // Fase 0 (3D): asciende y gira a toda velocidad.
        if (winPhase == 0) {
            const float tRaw = winTimer / ascendDuration;
            const float t = std::max(0.0f, std::min(1.0f, tRaw));
            const float inv = 1.0f - t;
            const float easeOut = 1.0f - (inv * inv * inv);

            currentSpriteName = spritePrefix + ".victoria.dedos.0";
            flipX = 0.0f;

            win3DHeight = 2.45f * easeOut;
            win3DScale = 1.05f + 0.55f * easeOut;
            winScale = 1.8f + 0.2f * easeOut;
            win3DSpin += deltaTime * (42.0f + 24.0f * easeOut);

            if (winTimer >= ascendDuration) {
                winPhase = 1;
                winTimer = 0.0f;
            }
            return;
        }

        // Fase 1 (3D): frenazo seco antes del burst.
        if (winPhase == 1) {
            const float tRaw = winTimer / hardStopDuration;
            const float t = std::max(0.0f, std::min(1.0f, tRaw));
            const float t2 = t * t;
            const float t4 = t2 * t2;
            const float brakeFactor = 1.0f - t4;

            currentSpriteName = spritePrefix + ".victoria.recto.0";
            flipX = 0.0f;

            win3DHeight = 2.45f + 0.12f * std::sin(t * kPi);
            win3DScale = 1.60f + 0.06f * (1.0f - t);
            winScale = 2.0f;
            win3DSpin += deltaTime * (46.0f * brakeFactor);

            if (winTimer >= hardStopDuration) {
                winPhase = 2;
                winTimer = 0.0f;
                win3DGlitterBurst = true;
                win3DGlitterTimer = 0.0f;
            }
            return;
        }

        // Fase 2 (3D): explosión glitter rosa y fin de celebración.
        win3DGlitterTimer = std::max(0.0f, win3DGlitterTimer + deltaTime);
        const float tRaw = win3DGlitterTimer / glitterDuration;
        const float t = std::max(0.0f, std::min(1.0f, tRaw));
        const float pulse = 0.65f + 0.35f * std::sin((1.0f - t) * 18.0f);

        currentSpriteName = spritePrefix + ".victoria.recto.0";
        flipX = 0.0f;
        win3DHeight = 2.45f + 0.07f * std::sin(win3DGlitterTimer * 5.0f);
        win3DScale = 1.55f + 0.10f * pulse;
        winScale = 1.95f;

        // Frenazo total: mantiene el ángulo fijo durante el burst.
        if (win3DGlitterTimer >= glitterDuration) {
            hasFinishedWinning = true;
            win3DGlitterBurst = false;
        }
        return;
    }

    // Fase 0: Pose de victoria (agrandarse)
    if (winPhase == 0) {
        if (winTimer < 0.5f) {
            // Pose inicial de paz normal
            currentSpriteName = spritePrefix + ".victoria.recto.0";
            winScale = 1.8f;
        } else if (winTimer < 2.7f) {
            // Pose gigante
            currentSpriteName = spritePrefix + ".victoria.dedos.0"; 
            winScale = 2.8f; // Crecimiento
        } else {
            // Transición a la fase de escape girando
            winPhase = 1;
            winTimer = 0.0f;
            winScale = 1.8f; // Vuelve al tamaño normal para girar
        }
    } 
    // Fase 1: Girar sobre si mismo y salir del mapa (diagonal aleatoria)
    else if (winPhase == 1) {
        int giroFrame = (int)(winTimer * 20.0f) % 4; // x20.0f para girar más rápido
        if (giroFrame == 0 || giroFrame == 2) currentSpriteName = spritePrefix + ".victoria.delado.0";
        else if (giroFrame == 1) currentSpriteName = spritePrefix + ".victoria.bocabajo.0";
        else currentSpriteName = spritePrefix + ".victoria.recto.0"; // Vuelve a ponerse recto para completar el giro 360
        
        flipX = (giroFrame == 2) ? 1.0f : 0.0f;

        position += winVelocity * deltaTime; // Sube fuera de la cámara en su vec2 diagonal
        
        // Verifica límites de la pantalla (NDC usualmente de -1 a 1 o un poco más con aspect ratio)
        if (position.y > 1.5f || position.y < -1.5f || position.x > 2.0f || position.x < -2.0f) { 
            hasFinishedWinning = true;
        }
    }
}

// Tick de lógica: animación de caminar o de muerte + invincibilidad.
void Player::Update() {
    if (lifeState == PlayerLifeState::Winning) {
        updateWinningAnimation();
        return;
    } else if (lifeState == PlayerLifeState::Alive) {
        // Invulnerabilidad: decrementar temporizador (sin "frames extra" al acabar).
        if (invincible) {
            invincibilityTimer -= deltaTime;
            if (invincibilityTimer <= 0.0f) {
                invincible = false;
                invincibilityTimer = 0.0f;
                invincibilityTotalSeconds = 0.0f;
                invincibilityFromPowerUp = false;
            }
        }
        updateAnimation();
    } else {
        updateDeathAnimation();
    }
}

// Render del sprite actual (el orden de pintado lo decide `bomberman.cpp`).
void Player::Draw() {
    float playerScaleFactor = (lifeState == PlayerLifeState::Winning) ? winScale : 1.8f;
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
    float whiteFlash = 0.0f;

    // Parpadeo blanco retro durante la invulnerabilidad (spawn/respawn o Armadura).
    // La fase crítica (últimos 4s de Armadura) incrementa la frecuencia.
    if (invincible) {
        const float hz = invulnBlinkHz(*this);
        const float t = (float)glfwGetTime();
        const int phase = (int)(t * hz);
        whiteFlash = (phase % 2 == 0) ? 1.0f : 0.0f;
    }

    glUniform4fv(uniformTintColor, 1, glm::value_ptr(tint));
    glUniform1f(uniformWhiteFlash, whiteFlash);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    // --- 3. DIBUJAR LA BOMBA/EFECTO (LO NUEVO) ENCIMA ---
    if (lifeState == PlayerLifeState::Winning) {
        int bombFrame = 0;
        bool drawBomb = true;

        if (winPhase == 0) {
            bombFrame = ((int)(winTimer / 0.3f)) % 2; // Misma velocidad que la mecha de una bomba colocada (0.3f)
        } else {
            bombFrame = 2 + (int)(winTimer / 0.05f);
            if (bombFrame > 7) {
                drawBomb = false; // Desaparecer cuando acaben los sprites
            }
        }

        if (drawBomb) {
            std::string bombSprite = "next_lvl_bomb." + std::to_string(bombFrame);
            glm::vec4 bombUv(0.0f);
            if (getUvRectForSprite(gNextLevelAtlas, bombSprite, bombUv)) {
                glm::mat4 bombModel = glm::mat4(1.0f);
                // Usar winStartPosition para que se quede fijo, con un ligero ajuste de 10px a la derecha (5px originales + 5px nuevos)
                bombModel = glm::translate(bombModel, glm::vec3(winStartPosition.x + (10.0f/24.0f)*halfTile, winStartPosition.y - 2.0f * halfTile, 0.0f));
                // Escala 3.465f (+5% de 3.3f)
                bombModel = glm::scale(bombModel, glm::vec3(halfTile * 3.465f, halfTile * 3.465f, 1.0f));

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, gNextLevelTexture);
                glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(bombModel));
                glUniform4fv(uniformUvRect, 1, glm::value_ptr(bombUv));
                glUniform1f(uniformFlipX, 0.0f);
                glUniform4fv(uniformTintColor, 1, glm::value_ptr(glm::vec4(1.0f)));
                glUniform1f(uniformWhiteFlash, 0.0f);
                glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            }
        }
    }
}

// Aplica un paso de movimiento con colisión (mapa + bombas) y “snap” al centro del tile.
void Player::UpdateSprite(Move mov, const GameMap* map, float deltaTime) {
    if (!map) return;
    if (lifeState != PlayerLifeState::Alive) return;

    const float halfTile = map->getTileSize() / 2.0f;
    const float rawStep  = std::max(0.0f, this->speed * deltaTime);
    const float step     = std::min(rawStep, halfTile * 0.45f);

    // --- Snap perpendicular al movimiento hacia el centro del tile actual ---
    {
        const float snapSpeed = 11.0f; // corrección por segundo, estable entre FPS
        const float snapAlpha = std::min(1.0f, std::max(0.0f, snapSpeed * deltaTime));
        const float snapMaxDist  = halfTile * 0.40f; // umbral máximo para aplicar
        const float snapEpsilon = halfTile * 0.03f;  // snap final para evitar micro-jitter
        int tr, tc;
        map->ndcToGrid(this->position, tr, tc);
        glm::vec2 tileCenter = map->gridToNDC(tr, tc);

        if (mov == MOVE_LEFT || mov == MOVE_RIGHT) {
            float dy = tileCenter.y - this->position.y;
            if (std::abs(dy) <= snapMaxDist) {
                this->position.y += dy * snapAlpha;
                if (std::abs(tileCenter.y - this->position.y) <= snapEpsilon) {
                    this->position.y = tileCenter.y;
                }
            }
        } else {
            float dx = tileCenter.x - this->position.x;
            if (std::abs(dx) <= snapMaxDist) {
                this->position.x += dx * snapAlpha;
                if (std::abs(tileCenter.x - this->position.x) <= snapEpsilon) {
                    this->position.x = tileCenter.x;
                }
            }
        }
    }

    glm::vec2 newPos = this->position;

    // --- Sondas de colisión ---
    {
        int r, c;
        const float eFront = halfTile;
        const float eSide  = halfTile * 0.60f;

        if (mov == MOVE_UP) {
            bool hitLeft = false;
            bool hitRight = false;
            map->ndcToGrid({newPos.x - eSide, newPos.y + eFront}, r, c);
            if (!map->isWalkable(r, c) || bombBlocksCellForPlayer(r, c, this->playerId)) hitLeft = true;
            map->ndcToGrid({newPos.x + eSide, newPos.y + eFront}, r, c);
            if (!map->isWalkable(r, c) || bombBlocksCellForPlayer(r, c, this->playerId)) hitRight = true;

            if (hitLeft && hitRight) {} // No hacemos nada
            else if (hitLeft && !hitRight) newPos.x += step * 0.75f;
            else if (!hitLeft && hitRight) newPos.x -= step * 0.75f;
            else newPos.y += step;

        }
        if (mov == MOVE_DOWN) {
            bool hitLeft = false;
            bool hitRight = false;
            map->ndcToGrid({newPos.x - eSide, newPos.y - eFront}, r, c);
            if (!map->isWalkable(r, c) || bombBlocksCellForPlayer(r, c, this->playerId)) hitLeft = true;
            map->ndcToGrid({newPos.x + eSide, newPos.y - eFront}, r, c);
            if (!map->isWalkable(r, c) || bombBlocksCellForPlayer(r, c, this->playerId)) hitRight = true;

            if (hitLeft && hitRight) {} // No hacemos nada
            else if (hitLeft && !hitRight) newPos.x += step * 0.75f;
            else if (!hitLeft && hitRight) newPos.x -= step * 0.75f;
            else newPos.y -= step;
        }
        if (mov == MOVE_LEFT) {
            bool hitDown = false;
            bool hitUp = false;
            map->ndcToGrid({newPos.x - eFront, newPos.y - eSide}, r, c);
            if (!map->isWalkable(r, c) || bombBlocksCellForPlayer(r, c, this->playerId)) hitDown = true;
            map->ndcToGrid({newPos.x - eFront, newPos.y + eSide}, r, c);
            if (!map->isWalkable(r, c) || bombBlocksCellForPlayer(r, c, this->playerId)) hitUp = true;

            if (hitDown && hitUp) {} // No hacemos nada
            else if (hitDown && !hitUp) newPos.y += step * 0.75f;
            else if (!hitDown && hitUp) newPos.y -= step * 0.75f;
            else newPos.x -= step;
        }
        if (mov == MOVE_RIGHT) {
            bool hitDown = false;
            bool hitUp = false;
            map->ndcToGrid({newPos.x + eFront, newPos.y - eSide}, r, c);
            if (!map->isWalkable(r, c) || bombBlocksCellForPlayer(r, c, this->playerId)) hitDown = true;
            map->ndcToGrid({newPos.x + eFront, newPos.y + eSide}, r, c);
            if (!map->isWalkable(r, c) || bombBlocksCellForPlayer(r, c, this->playerId)) hitUp = true;

            if (hitDown && hitUp) {} // No hacemos nada
            else if (hitDown && !hitUp) newPos.y += step * 0.75f;
            else if (!hitDown && hitUp) newPos.y -= step * 0.75f;
            else newPos.x += step;
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
    // Inicializar efectos visuales 3D
    deathUse3DCelebration = (bomberman != nullptr && bomberman->viewMode == ViewMode::Mode3D);
    deathPhase = 0;
    death3DHeight = 0.0f;
    death3DSpin = 0.0f;
    death3DTilt = 0.0f;
    death3DScale = 1.0f;
    death3DGlitterTimer = -1.0f;
    death3DGlitterBurst = false;
    death3DTotalTimer = 0.0f;
    deathStartPosition = position;
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
    // Inicializar efectos visuales 3D
    deathUse3DCelebration = (bomberman != nullptr && bomberman->viewMode == ViewMode::Mode3D);
    deathPhase = 0;
    death3DHeight = 0.0f;
    death3DSpin = 0.0f;
    death3DTilt = 0.0f;
    death3DScale = 1.0f;
    death3DGlitterTimer = -1.0f;
    death3DGlitterBurst = false;
    death3DTotalTimer = 0.0f;
    deathStartPosition = position;
}

// Vuelve al spawn y restaura estado. Nota: el jugador NO pierde power-ups acumulados durante el nivel.
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

    // Reset 3D death animation state
    deathUse3DCelebration = false;
    deathPhase = 0;
    death3DHeight = 0.0f;
    death3DSpin = 0.0f;
    death3DTilt = 0.0f;
    death3DScale = 1.0f;
    death3DGlitterTimer = -1.0f;
    death3DGlitterBurst = false;
    death3DTotalTimer = 0.0f;
    deathStartPosition = glm::vec2(0.0f);

    flipX = 0.0f;
    currentSpriteName = spritePrefix + ".abajo.0";

    // Regla General: mantener stats/power-ups acumulados (bombs/fire/speed), incluso si pierde vidas.
    // Excepción: el Detonator (Remote Control) se pierde al morir.
    hasRemoteControl = false;

    // Solo se reaplica invulnerabilidad de respawn.
    invincible = true;
    invincibilityTimer = kSpawnInvulnerabilitySeconds;
    invincibilityTotalSeconds = kSpawnInvulnerabilitySeconds;
    invincibilityFromPowerUp = false;

    // Limpiar cualquier estado residual de celebración.
    hasFinishedWinning = false;
    winTimer = 0.0f;
    winPhase = 0;
    winScale = 1.8f;
    winUse3DCelebration = false;
    winAnchorPosition = position;
    win3DHeight = 0.0f;
    win3DSpin = 0.0f;
    win3DScale = 1.0f;
    win3DGlitterTimer = -1.0f;
    win3DGlitterBurst = false;

    // activeBombs no se resetea: las bombas en el mapa siguen existiendo
    // y decrementarán activeBombs cuando exploten.
}

// Aplica un power-up al jugador (respeta ArcadeCaps).
void Player::applyPowerUp(PowerUpType type) {
    // Guardar en el inventario para mostrar en el HUD
    collectedItems.push_back(type);

    if (type == PowerUpType::Matches || type == PowerUpType::Can || type == PowerUpType::Lighter || 
        type == PowerUpType::Battery || type == PowerUpType::Dragonfly || type == PowerUpType::HudsonBee) {
        
        // Sumar al score por items
        const int itemScore = scoreValueForItem(type);
        if (bomberman && playerId >= 0 && playerId < (int)bomberman->playerScores.size()) {
            bomberman->playerScores[playerId] += itemScore;
        }
        if (itemScore > 0) {
            ScorePopup::spawn(position, itemScore, 1, true);
        }
        return; // Los items no otorgan buffs jugables 
    }

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
            invincibilityTimer = kPowerUpInvulnerabilitySeconds;
            invincibilityTotalSeconds = kPowerUpInvulnerabilitySeconds;
            invincibilityFromPowerUp = true;
            break;

        case PowerUpType::RemoteControl:
            hasRemoteControl = true;
            break;
    }
}

