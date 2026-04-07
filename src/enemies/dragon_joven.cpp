#include "enemies/dragon_joven.hpp"
#include "game_map.hpp"
#include "player.hpp"
#include "bomb.hpp"
#include "sprite_atlas.hpp"
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cstdlib>
#include <cmath>

extern GLuint VAO;
extern GLuint uniformModel;
extern GLuint uniformUvRect;
extern GLuint uniformFlipX;
extern GLuint uniformTintColor;
extern SpriteAtlas gEnemyAtlas;
extern SpriteAtlas gBombAtlas;
extern GLuint mapTexture;
extern GLuint enemyTexture;

DragonJoven::DragonJoven(glm::vec2 pos, glm::vec2 size, float speed)
    : Enemy(pos, size, speed, /*hp=*/1, /*score=*/400),
      fireCooldown(0.0f),
      fireCooldownMax(3.0f),  // Cada 3 segundos puede escupir fuego
      fireRange(2),
      isCharging(false),
      chargeTimer(0.0f),
      isFiring(false),
      fireAnimTimer(0.0f),
      fireAnimFrame(0)
{
    facing = randomDirection();
}

DragonJoven::~DragonJoven() {}

// Devuelve la dirección de disparo si el jugador está en línea y en rango.
EnemyDirection DragonJoven::checkLineOfSight() const {
    if (!playersList || !gameMap) return EnemyDirection::NONE;
    
    float dist;
    glm::vec2 targetPos = getClosestPlayerPos(dist);
    if (dist >= 99999.0f) return EnemyDirection::NONE;

    int enemyRow, enemyCol, playerRow, playerCol;
    gameMap->ndcToGrid(position, enemyRow, enemyCol);
    gameMap->ndcToGrid(targetPos, playerRow, playerCol);

    float tileSize = gameMap->getTileSize();

    // Misma fila
    if (enemyRow == playerRow) {
        int dist = std::abs(playerCol - enemyCol);
        if (dist > 0 && dist <= fireRange) {
            // Comprobar que no haya bloques entre medias
            int step = (playerCol > enemyCol) ? 1 : -1;
            bool clear = true;
            for (int c = enemyCol + step; c != playerCol; c += step) {
                if (!gameMap->isWalkable(enemyRow, c)) {
                    clear = false;
                    break;
                }
            }
            if (clear) {
                return (playerCol > enemyCol) ? EnemyDirection::RIGHT : EnemyDirection::LEFT;
            }
        }
    }

    // Misma columna
    if (enemyCol == playerCol) {
        int dist = std::abs(playerRow - enemyRow);
        if (dist > 0 && dist <= fireRange) {
            int step = (playerRow > enemyRow) ? 1 : -1;
            bool clear = true;
            for (int r = enemyRow + step; r != playerRow; r += step) {
                if (!gameMap->isWalkable(r, enemyCol)) {
                    clear = false;
                    break;
                }
            }
            if (clear) {
                return (playerRow > enemyRow) ? EnemyDirection::DOWN : EnemyDirection::UP;
            }
        }
    }

    return EnemyDirection::NONE;
}

// IA: patrulla; si ve al jugador en línea, intenta escupir fuego (utilizando segmentos de explosión).
void DragonJoven::Update() {
    if (lifeState != EnemyLifeState::Alive) return;

    float step = speed * deltaTime;

    // Actualizar cooldown de fuego
    if (fireCooldown > 0.0f) {
        fireCooldown -= deltaTime;
    }

    // Si está cargando, parpadear antes de disparar
    if (isCharging) {
        chargeTimer -= deltaTime;
        if (chargeTimer <= 0.0f) {
            isCharging = false;
            isFiring = true;
            fireAnimTimer = 0.5f; // Medio segundo de duración para el fuego real
            fireAnimFrame = 0;
            // Ojo, hemos inicializado fireSegments abajo, aquí solo empezamos la fase 2
        }
        return; // No se mueve mientras carga
    }

    // Si está disparando, esperar a que termine la animación
    if (isFiring) {
        fireAnimTimer -= deltaTime;
        
        // Cambiar de frame cada 0.08s para la animación base (explosion.0..3)
        int newFrame = (int)((0.5f - fireAnimTimer) / 0.08f);
        if (newFrame > 3) newFrame = 3;
        fireAnimFrame = newFrame;

        // El fuego se expande "poco a poco" con un retraso inicial para cuadrar con el sprite
        float elapsedTime = 0.5f - fireAnimTimer;
        int activeSegments = 0;
        if (elapsedTime >= 0.15f) { // Sale después de 0.15s (cuando el dragón abre la boca a máxima potencia)
            activeSegments = (int)((elapsedTime - 0.15f) / 0.1f) + 1;
        }
        if (activeSegments > fireSegments.size()) activeSegments = fireSegments.size();
        
        // Comprobar daño a jugadores y detonar bombas en el área del fuego (solo los segmentos activos)
        for (int i = 0; i < activeSegments; ++i) {
            const auto& seg = fireSegments[i];
            int fr, fc;
            gameMap->ndcToGrid(seg.pos, fr, fc);
            
            // Daño a jugadores
            if (playersList) {
                for (auto* p : *playersList) {
                    if (!p || !p->isAlive()) continue;
                    int pr, pc;
                    gameMap->ndcToGrid(p->position, pr, pc);
                    if (fr == pr && fc == pc) {
                        p->killByExplosion();
                    }
                }
            }
            // Detonar bombas
            for (auto* b : gBombs) {
                if (b && b->state == BombState::FUSE) {
                    int br, bc;
                    gameMap->ndcToGrid(b->position, br, bc);
                    if (fr == br && fc == bc) {
                        b->detonate();
                    }
                }
            }
        }
        
        if (fireAnimTimer <= 0.0f) {
            isFiring = false;
        }
        return; // No se mueve mientras escupe fuego
    }

    // Comprobar línea de visión para escupir fuego
    if (fireCooldown <= 0.0f) {
        EnemyDirection fireDir = checkLineOfSight();
        if (fireDir != EnemyDirection::NONE) {
            isCharging = true;
            chargeTimer = 0.5f; // Medio segundo de parpadeo rojo como advertencia
            fireCooldown = fireCooldownMax;
            facing = fireDir;
            
            // Centrar el dragón en la celda actual para alinear el fuego perfectamente
            int centerR, centerC;
            gameMap->ndcToGrid(position, centerR, centerC);
            position = gameMap->gridToNDC(centerR, centerC);

            // Pre-generar fireSegments (alcance fireRange) para saber por dónde irá
            fireSegments.clear();
            int dr = 0, dc = 0;
            float angle = 0.0f;
            
            // Corregimos los ejes y las rotaciones de los sprites:
            // dr = -1 va hacia arriba (hacia menor row en el mapa)
            // dr = 1 va hacia abajo (mayor row)
            // dc = 1 va hacia la derecha
            // dc = -1 va hacia la izquierda
            // angle de Bomb::detonate(): 
            // dr = -1 -> UP (angle = 0.0f)
            // dr =  1 -> DOWN (angle = 180.0f = pi)
            // dc = -1 -> LEFT (angle = 90.0f = pi/2)
            // dc =  1 -> RIGHT (angle = -90.0f = -pi/2)
            if (fireDir == EnemyDirection::UP) { 
                dr = -1; dc = 0; angle = 0.0f; 
            } else if (fireDir == EnemyDirection::DOWN) { 
                dr = 1; dc = 0; angle = glm::radians(180.0f); 
            } else if (fireDir == EnemyDirection::LEFT) { 
                dr = 0; dc = -1; angle = glm::radians(90.0f); 
            } else if (fireDir == EnemyDirection::RIGHT) { 
                dr = 0; dc = 1; angle = glm::radians(-90.0f); 
            }

            int r, c;
            gameMap->ndcToGrid(position, r, c);
            for (int d = 1; d <= fireRange; d++) {
                int nr = r + dr * d;
                int nc = c + dc * d;
                
                // Si encontramos pared o algo no caminable, detenemos el fuego o comprobamos si es bloque destructible
                if (!gameMap->isWalkable(nr, nc)) {
                     break; 
                }
                
                bool isLast = (d == fireRange);
                if (!isLast) {
                    if (!gameMap->isWalkable(nr + dr, nc + dc)) {
                        isLast = true;
                    }
                }
                std::string base = isLast ? "explosion_end" : "explosion_mid";
                fireSegments.push_back({ gameMap->gridToNDC(nr, nc), base, angle });
                if (isLast) break;
            }
            
            return;
        }
    }

    // Movimiento básico de patrulla
    if (!tryMove(facing, step)) {
        facing = randomDirection();
    }
    
    // Animación de caminar
    animTimer += deltaTime;
    if (animTimer >= 0.2f) {
        animTimer = 0.0f;
        animFrame ^= 1;
    }
}

// Render del enemigo y de su fuego
void DragonJoven::Draw() {
    if (!alive) return;

    float halfTile = gameMap->getTileSize() / 2.0f;

    // 1. Renderizar fuego (si está disparando) PRIMERO, para que quede por debajo del dragón
    if (isFiring && !fireSegments.empty()) {
        // Guardar textura actual y bindear la textura del mapa (bombas/explosiones)
        glBindTexture(GL_TEXTURE_2D, mapTexture);
        glUniform1f(uniformFlipX, 0.0f); // Explosiones no se voltean horizontalmente
        glm::vec4 tint(1.0f, 1.0f, 1.0f, 1.0f);
        glUniform4fv(uniformTintColor, 1, glm::value_ptr(tint));

        // Solo dibujamos los tramos que se han ido activando ("poco a poco" con retraso para la animación)
        float elapsedTime = 0.5f - fireAnimTimer;
        int activeSegments = 0;
        if (elapsedTime >= 0.15f) { // Sale después de 0.15s (cuando el dragón abre la boca a máxima potencia)
            activeSegments = (int)((elapsedTime - 0.15f) / 0.1f) + 1;
        }
        if (activeSegments > fireSegments.size()) activeSegments = fireSegments.size();

        if (activeSegments > 0) {
            for (int i = 0; i < activeSegments; ++i) {
                const auto& seg = fireSegments[i];
                std::string segSpriteName = seg.baseName + "." + std::to_string(fireAnimFrame);
                glm::vec4 segUvRect(0.0f, 0.0f, 1.0f, 1.0f);
                
                if (getUvRectForSprite(gBombAtlas, segSpriteName, segUvRect)) {
                    glm::mat4 segModel = glm::mat4(1.0f);
                    
                    // El fuego se dibuja exactamente en las tiles pre-calculadas (que empiezan desde la siguiente al dragón)
                    segModel = glm::translate(segModel, glm::vec3(seg.pos.x, seg.pos.y, 0.0f));
                    segModel = glm::rotate(segModel, seg.rotation, glm::vec3(0.0f, 0.0f, 1.0f));
                    
                    // Dejar un pequeño espacio entre la boca del dragón y el primer segmento de fuego
                    float gap = 0.0f;
                    if (i == 0) {
                        if (facing == EnemyDirection::LEFT || facing == EnemyDirection::RIGHT) {
                            gap = halfTile * 1.0f; // Margen para horizontal
                        } else {
                            gap = halfTile * 0.2f; // Margen menor para vertical
                        }
                    }
                    segModel = glm::translate(segModel, glm::vec3(0.0f, gap / 2.0f, 0.0f));
                    segModel = glm::scale(segModel, glm::vec3(halfTile, halfTile - (gap / 2.0f), 1.0f));

                    glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(segModel));
                    glUniform4fv(uniformUvRect, 1, glm::value_ptr(segUvRect));
                    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
                }
            }
        }

        // Restaurar textura de enemigos
        glBindTexture(GL_TEXTURE_2D, enemyTexture);
    }

    // 2. Renderizar dragón (encima del fuego)
    const float enemyScaleFactor = 2.0f; // Tamaño relativo
    glm::mat4 model = glm::mat4(1.0f);
    glm::vec3 renderPos = glm::vec3(position.x, position.y + (enemyScaleFactor - 1.0f) * halfTile * 0.8f, 0.0f);
    model = glm::translate(model, renderPos);   

    std::string prefix;
    std::string firePrefix;
    if (facing == EnemyDirection::RIGHT || facing == EnemyDirection::LEFT) {
        prefix = "dragon.derecha.";
        firePrefix = "dragon.fuego.derecha.";
        flipX = (facing == EnemyDirection::LEFT) ? 1.0f : 0.0f;
    } else if (facing == EnemyDirection::UP) {
        prefix = "dragon.arriba.";
        firePrefix = "dragon.fuego.arriba.";
        flipX = 0.0f;
    } else { // DOWN
        prefix = "dragon.abajo.";
        firePrefix = "dragon.fuego.abajo.";
        flipX = 0.0f;
    }
    
    std::string spriteName = prefix + std::to_string(animFrame);
    if (isFiring || isCharging) {
        // La animación de escupir fuego dura 0.5s en total y tiene 5 frames (0 a 4)
        int fireFrame = 0;
        if (isCharging) {
            // Si está cargando, usar el frame 0 con boca abierta o el que sea (0 de fuego típicamente)
            fireFrame = 0;
        } else {
            fireFrame = (int)((0.5f - fireAnimTimer) / 0.1f);
            if (fireFrame < 0) fireFrame = 0;
            if (fireFrame > 4) fireFrame = 4;
        }
        spriteName = firePrefix + std::to_string(fireFrame);
        if (gEnemyAtlas.sprites.find(spriteName) == gEnemyAtlas.sprites.end()) {
             spriteName = prefix + "0"; // fallback
        }
    }
    currentSpriteName = spriteName;

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
    
    // Si está cargando el ataque, parpadea en rojo
    glm::vec4 tint(1.0f, 1.0f, 1.0f, 1.0f);
    if (isCharging) {
        // Parpadear cada 0.1 segundos
        if (((int)(chargeTimer / 0.1f)) % 2 == 0) {
            tint = glm::vec4(1.0f, 0.3f, 0.3f, 1.0f); // Tinte rojo claro
        }
    }
    glUniform4fv(uniformTintColor, 1, glm::value_ptr(tint));
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}
