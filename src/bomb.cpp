#include "bomb.hpp"
#include "player.hpp"
#include "game_map.hpp"
#include "sprite_atlas.hpp"

#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Externos definidos en bomberman.cpp
extern GLuint VAO;
extern GLuint uniformModel;
extern GLuint uniformUvRect;
extern GLuint uniformFlipX;
extern GLuint uniformTintColor;
extern GLuint mapTexture;
extern GameMap* gameMap;
extern SpriteAtlas gBombAtlas;

/*
 * bomb.cpp
 * --------
 * Implementación de la bomba con dos fases:
 *   1. FUSE: animación cíclica (1,2,1,0,1,2,1,0) mientras cuenta la mecha.
 *   2. EXPLODING: 4 frames de explosión muy rápidos (explosion.0 -> explosion.3).
 *   3. DONE: lista para eliminar.
 */

// ============================== Ctor / dtor ==============================

Bomb::Bomb(glm::vec2 pos, int row, int col, Player* ownerPlayer, int bombPower, bool remote)
    : position(pos),
      gridRow(row),
      gridCol(col),
      fuseTime(3.0f),
      fuseTimer(0.0f),
      state(BombState::FUSE),
      animTimer(0.0f),
      animFrame(1),
      animStep(0),
      animInterval(0.3f),     // Intervalo constante de la mecha
      explodeInterval(0.045f), // Más rápido que 0.08: 8 frames a 0.045s = 0.36s (termina antes que 0.40s)
      currentSpriteName("bomb.1"),
      ownerIndex(ownerPlayer ? ownerPlayer->playerId : 0),
      ownerLeftTile(false),
      power(bombPower),
      owner(ownerPlayer),
      remoteControlled(remote)
{}

// Ajusta contadores del owner si la bomba se destruye antes de terminar.
Bomb::~Bomb() {
    if (owner && state != BombState::DONE) {
        owner->activeBombs--;
        owner = nullptr;
    }
}

// Devuelve si la bomba bloquea el tile para ese jugador (regla: el dueño puede salir una vez).
bool Bomb::blocksForPlayer(int playerId) const {
    if (state == BombState::DONE) return false;
    if (playerId < 0) return true;

    // Regla del Bomberman clásico:
    // el dueño puede SALIR del tile en el que puso la bomba, pero no puede
    // volver a ENTRAR una vez ha abandonado esa casilla.
    if (ownerIndex == playerId && !ownerLeftTile) {
        return false; // el dueño puede salir de su propia bomba
    }
    return true;
}

// ============================== Lógica ==============================

// Avanza mecha/explosión; devuelve true cuando termina para borrarla.
bool Bomb::Update(float deltaTime) {

    if (state == BombState::FUSE) {
        // --- Mecha ---
        // Si tiene Remote Control, la mecha no avanza sola
        if (!remoteControlled) {
            fuseTimer += deltaTime;
            if (fuseTimer >= fuseTime) {
                detonate();
                return false;
            }
        }

        // Animación de mecha: secuencia 1,2,1,0,1,2,1,0
        animTimer += deltaTime;
        if (animTimer >= animInterval) {
            animTimer -= animInterval;

            static const int sequence[] = {1, 2, 1, 0, 1, 2, 1, 0};
            animStep = (animStep + 1) % 8;
            animFrame = sequence[animStep];
        }
        currentSpriteName = "bomb." + std::to_string(animFrame);

    } else if (state == BombState::EXPLODING) {
        // --- Explosión: 4 hacerse grande y 4 volverse pequeño (total 8) ---
        animTimer += deltaTime;
        if (animTimer >= explodeInterval) {
            animTimer -= explodeInterval;
            animStep++; // Usar animStep para la secuencia entera
            
            // Secuencia de frames: 0, 1, 2, 3, 3, 2, 1, 0 (8 frames)
            const int expSequence[] = {0, 1, 2, 3, 3, 2, 1, 0};
            if (animStep >= 8) {
                // Explosión terminada un poco antes que el destructible
                state = BombState::DONE;
                return true; // Señal para eliminar
            }
            animFrame = expSequence[animStep];
            currentSpriteName = "explosion." + std::to_string(animFrame);
        }
    }

    return false;
}

// Fuerza la explosión inmediata (Remote Control o temporizador agotado).
void Bomb::detonate() {
    if (state != BombState::FUSE) return;

    // Decrementar contador de bombas activas del dueño
    if (owner) {
        owner->activeBombs--;
        owner = nullptr; // Evitar doble decremento
    }

    state = BombState::EXPLODING;
    animTimer = 0.0f;
    animStep = 0;
    animFrame = 0;

    explosionSegments.clear();
    // 1. Centro
    explosionSegments.push_back({ position, "explosion", 0.0f });
    // Regla: si una explosión alcanza un ítem ya suelto en el suelo, desaparece.
    gameMap->destroyExposedPowerUp(gridRow, gridCol);

    // 2. Expandir en 4 direcciones
    int dr[] = {0, -1, 0, 1}; // RIGHT, UP, LEFT, DOWN
    int dc[] = {1, 0, -1, 0};
    // Ajuste solicitado: intercambiar orientacion vertical/horizontal de la llama.
    float angles[] = {0.0f, glm::radians(90.0f), glm::radians(180.0f), glm::radians(-90.0f)};

    for (int i = 0; i < 4; i++) {
        for (int d = 1; d <= power; d++) {
            int r = gridRow + dr[i] * d;
            int c = gridCol + dc[i] * d;

            if (!gameMap->isWalkable(r, c)) {
                // Muro: destruirlo si es destructible (su propia animación se encarga),
                // pero NO dibujar fuego encima del bloque.
                gameMap->destroyTile(r, c);
                break;
            }

            // Regla: si hay un power-up suelto en el suelo, la explosión lo destruye.
            gameMap->destroyExposedPowerUp(r, c);

            // Tile libre: determinar si es el último segmento visible
            bool isLast = (d == power);
            if (!isLast) {
                // Lookahead: si la siguiente celda es muro, este es el "end" visual
                int nr = r + dr[i];
                int nc = c + dc[i];
                if (!gameMap->isWalkable(nr, nc)) {
                    isLast = true;
                }
            }

            std::string baseName = isLast ? "explosion_end" : "explosion_mid";
            explosionSegments.push_back({ gameMap->gridToNDC(r, c), baseName, angles[i] });
        }
    }
}

// ============================== Render ==============================

// Renderiza la bomba o sus segmentos de explosión (usa textura del mapa/atlas de stage).
void Bomb::Draw() {
    if (state == BombState::DONE || !gameMap) return;

    float halfTile = gameMap->getTileSize() / 2.0f;

    glUniform1f(uniformFlipX, 0.0f);
    glm::vec4 tint(1.0f, 1.0f, 1.0f, 1.0f);
    glUniform4fv(uniformTintColor, 1, glm::value_ptr(tint));

    if (state == BombState::FUSE) {
        // Renderizar la bomba normal
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(position.x, position.y, 0.0f));
        model = glm::scale(model, glm::vec3(halfTile, halfTile, 1.0f));

        glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
        getUvRectForSprite(gBombAtlas, currentSpriteName, uvRect);

        glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
        glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    } else if (state == BombState::EXPLODING) {
        // Renderizar todos los segmentos de la explosión
        for (const auto& seg : explosionSegments) {
            std::string spriteName = seg.baseName + "." + std::to_string(animFrame);
            
            glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
            if (!getUvRectForSprite(gBombAtlas, spriteName, uvRect)) {
                continue; // Si el usuario puso "XXXX" y no carga, saltar este segmento en lugar de colgarse
            }

            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(seg.pos.x, seg.pos.y, 0.0f));
            model = glm::rotate(model, seg.rotation, glm::vec3(0.0f, 0.0f, 1.0f));
            model = glm::scale(model, glm::vec3(halfTile, halfTile, 1.0f));

            glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
            glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        }
    }
}
