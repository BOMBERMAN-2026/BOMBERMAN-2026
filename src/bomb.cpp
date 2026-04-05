#include "bomb.hpp"
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

Bomb::Bomb(glm::vec2 pos, int row, int col, int owner)
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
      explodeInterval(0.08f), // Muy rápido: cada 80ms cambia frame de explosión
      currentSpriteName("bomb.1"),
      ownerIndex(owner),
            ownerLeftTile(false),
      power(3) // Longitud 3: 1 sitio (centro) + 2 tiles extendidos (mid y end)
{}

Bomb::~Bomb() {}

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
        fuseTimer += deltaTime;
        if (fuseTimer >= fuseTime) {
            // Explosión: resetear animación
            state = BombState::EXPLODING;
            animTimer = 0.0f;
            animFrame = 0;

            explosionSegments.clear();
            // 1. Centro
            explosionSegments.push_back({ position, "explosion", 0.0f });

            // 2. Expandir en 4 direcciones
            // Asumimos que los sprites ext/mid apuntan hacia ARRIBA por defecto (según las pruebas).
            // Direcciones: 0=DERECHA, 1=ARRIBA, 2=IZQUIERDA, 3=ABAJO
            int dr[] = {0, -1, 0, 1};
            int dc[] = {1, 0, -1, 0};
            // Rotaciones necesarias para un sprite que apunta hacia ARRIBA:
            // DERECHA: -90, ARRIBA: 0, IZQUIERDA: 90, ABAJO: 180
            float angles[] = {glm::radians(-90.0f), 0.0f, glm::radians(90.0f), glm::radians(180.0f)};

            for (int i = 0; i < 4; i++) {
                for (int d = 1; d < power; d++) {
                    int r = gridRow + dr[i] * d;
                    int c = gridCol + dc[i] * d;

                    if (!gameMap->isWalkable(r, c)) {
                        // Choca con un obstáculo.
                        // Si es destructible, se destruirá, pero la explosión gráfica no avanza más allá ni pinta encima.
                        gameMap->destroyTile(r, c);
                        break;
                    }

                    // Si la celda es caminable, es un segmento de la explosión.
                    // Si es el final del recorrido (d == power - 1) o la siguiente celda es muro, usamos "end".
                    bool isLast = (d == power - 1);
                    if (!isLast && !gameMap->isWalkable(r + dr[i], c + dc[i])) {
                        isLast = true;
                    }

                    std::string baseName = isLast ? "explosion_end" : "explosion_mid";
                    explosionSegments.push_back({ gameMap->gridToNDC(r, c), baseName, angles[i] });

                    if (isLast) break; // Terminar esta dirección si forzamos el 'end'
                }
            }

            return false;
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
        // --- Explosión: 4 frames muy rápidos ---
        animTimer += deltaTime;
        if (animTimer >= explodeInterval) {
            animTimer -= explodeInterval;
            animFrame++;
            if (animFrame >= 4) {
                // Explosión terminada
                state = BombState::DONE;
                return true; // Señal para eliminar
            }
            currentSpriteName = "explosion." + std::to_string(animFrame);
        }
    }

    return false;
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
