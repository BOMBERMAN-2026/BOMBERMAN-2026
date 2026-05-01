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

bool DragonJoven::isFiringAttack() const {
    return isFiring;
}

int DragonJoven::getActiveFireSegmentCount() const {
    if (!isFiring || fireSegments.empty()) return 0;

    // Todos de golpe, no gradual.
    return (int)fireSegments.size();
}

const std::vector<ExplosionSegment>& DragonJoven::getFireSegments() const {
    return fireSegments;
}

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
            fireAnimTimer = 10 * 0.04f; // 10 frames de fuego a 0.04s
            fireAnimFrame = 0;
            // Ojo, hemos inicializado fireSegments abajo, aquí solo empezamos la fase 2
        }
        return; // No se mueve mientras carga
    }

    // Si está disparando, esperar a que termine la animación
    if (isFiring) {
        fireAnimTimer -= deltaTime;
        
        // El sprite de la explosión empieza a la vez que el dragón abre la boca (2º sprite)
        int step = (int)((10 * 0.04f - fireAnimTimer) / 0.04f);
        if (step > 9) step = 9;
        
        const int expSequence[10] = {-1, 0, 1, 2, 3, 3, 2, 1, 0, -1};
        fireAnimFrame = expSequence[step];

        // El fuego se expande "poco a poco" con un retraso inicial para cuadrar con el sprite
        const int activeSegments = getActiveFireSegmentCount();
        
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
            chargeTimer = 0.9f; // 0.9 segundos de parpadeo rojo como advertencia antes de disparar
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
            int dirInd = -1;
            if (fireDir == EnemyDirection::UP) { 
                dr = -1; dc = 0; angle = 0.0f; dirInd = 1;
            } else if (fireDir == EnemyDirection::DOWN) { 
                dr = 1; dc = 0; angle = glm::radians(180.0f); dirInd = 3;
            } else if (fireDir == EnemyDirection::LEFT) { 
                dr = 0; dc = -1; angle = glm::radians(90.0f); dirInd = 2;
            } else if (fireDir == EnemyDirection::RIGHT) { 
                dr = 0; dc = 1; angle = glm::radians(-90.0f); dirInd = 0;
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
                fireSegments.push_back({ gameMap->gridToNDC(nr, nc), base, angle, dirInd });
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

    if (lifeState == EnemyLifeState::Dying) {
        const float enemyScaleFactor = 2.0f;
        glm::mat4 model = glm::mat4(1.0f);

        float baseHeight = 1.0f;
        float baseWidth = 1.0f;
        auto itBase = gEnemyAtlas.sprites.find("dragon.abajo.0");
        if (itBase != gEnemyAtlas.sprites.end() && itBase->second.h > 0) {
            baseHeight = static_cast<float>(itBase->second.h);
            baseWidth = static_cast<float>(itBase->second.w);
        }

        float scaleX = 1.0f;
        float scaleY = 1.0f;
        float currentWidth = baseWidth;
        auto it = gEnemyAtlas.sprites.find(currentSpriteName);
        if (it != gEnemyAtlas.sprites.end() && it->second.h > 0) {
            currentWidth = static_cast<float>(it->second.w);
            scaleX = currentWidth / baseHeight; // Mantener la misma altura de ref para aspecto real
            scaleY = static_cast<float>(it->second.h) / baseHeight;
        }

        glm::vec3 renderPos = glm::vec3(position.x, position.y + (enemyScaleFactor - 1.0f) * halfTile * 0.8f, 0.0f);

        // Compensar corrimiento visual a la izquierda si el sprite de humo es más ancho
        if (currentWidth > baseWidth) {
            renderPos.x += (halfTile * enemyScaleFactor) * ((currentWidth - baseWidth) / baseWidth) * 0.35f;
        }

        // Ajustar posición en Y para que la base de la animación no flote
        renderPos.y -= (halfTile * enemyScaleFactor) * (1.0f - scaleY);
        
        model = glm::translate(model, renderPos);   

        glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
        getUvRectForSprite(gEnemyAtlas, currentSpriteName, uvRect);

        model = glm::scale(model, glm::vec3(halfTile * enemyScaleFactor * scaleX, halfTile * enemyScaleFactor * scaleY, 1.0f));

        glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
        glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));
        glUniform1f(uniformFlipX, flipX); // mantener el último flipX
        
        glm::vec4 tint(1.0f, 1.0f, 1.0f, 1.0f);
        glUniform4fv(uniformTintColor, 1, glm::value_ptr(tint));
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        return;
    }

    // 1. Renderizar fuego (si está disparando) PRIMERO, para que quede por debajo del dragón
    if (isFiring && !fireSegments.empty()) {
        // Guardar textura actual y bindear la textura del mapa (bombas/explosiones)
        glBindTexture(GL_TEXTURE_2D, mapTexture);
        glUniform1f(uniformFlipX, 0.0f); // Explosiones no se voltean horizontalmente
        glm::vec4 tint(1.0f, 1.0f, 1.0f, 1.0f);
        glUniform4fv(uniformTintColor, 1, glm::value_ptr(tint));

        // Solo dibujamos los tramos que se han ido activando ("poco a poco" con retraso para la animación)
        const int activeSegments = getActiveFireSegmentCount();

        if (activeSegments > 0) {
            for (int i = 0; i < activeSegments; ++i) {
                const auto& seg = fireSegments[i];
                
                std::string dirSuffix = "";
                if (seg.dirIndex == 0) dirSuffix = "_right";
                else if (seg.dirIndex == 1) dirSuffix = "_up";
                else if (seg.dirIndex == 2) dirSuffix = "_left";
                else if (seg.dirIndex == 3) dirSuffix = "_down";

                std::string segSpriteNameDir = seg.baseName + dirSuffix + "." + std::to_string(fireAnimFrame);
                std::string segSpriteNameGen = seg.baseName + "." + std::to_string(fireAnimFrame);
                glm::vec4 segUvRect(0.0f, 0.0f, 1.0f, 1.0f);
                
                float renderRotation = seg.rotation;
                bool foundSprite = false;

                if (seg.dirIndex != -1 && getUvRectForSprite(gBombAtlas, segSpriteNameDir, segUvRect)) {
                    foundSprite = true;
                    renderRotation = 0.0f;
                } else {
                    if (getUvRectForSprite(gBombAtlas, segSpriteNameGen, segUvRect)) {
                        foundSprite = true;
                    }
                }

                if (foundSprite) {
                    glm::mat4 segModel = glm::mat4(1.0f);
                    
                    // Ajustar escala y posición basándonos en el tamaño del sprite (ej. 24x48 o 48x24) en el atlas
                    float scaleX = 1.0f;
                    float scaleY = 1.0f;
                    std::string actualSpriteName = (seg.dirIndex != -1 && foundSprite && renderRotation == 0.0f) ? segSpriteNameDir : segSpriteNameGen;
                    auto itSprite = gBombAtlas.sprites.find(actualSpriteName);
                    if (itSprite != gBombAtlas.sprites.end()) {
                        scaleX = static_cast<float>(itSprite->second.w) / 48.0f;
                        scaleY = static_cast<float>(itSprite->second.h) / 48.0f;
                    }

                    float offsetX = 0.0f;
                    float offsetY = 0.0f;

                    // Calcular offsets ANTES del estrechamiento, basados en las dimensiones reales del sprite.
                    // Así el estrechamiento voluntario no genera offsets espurios.
                    if (scaleX < 1.0f) {
                        if (seg.dirIndex == 0) offsetX = halfTile * (1.0f - scaleX);       // RIGHT -> borde derecho
                        else if (seg.dirIndex == 2) offsetX = -halfTile * (1.0f - scaleX); // LEFT  -> borde izquierdo
                    }
                    if (scaleY < 1.0f) {
                        if (seg.dirIndex == 1) offsetY = halfTile * (1.0f - scaleY);       // UP   -> borde superior
                        else if (seg.dirIndex == 3) offsetY = -halfTile * (1.0f - scaleY); // DOWN -> borde inferior
                    }

                    // Ajustar escala para que el fuego sea más estrecho perpendicular a su dirección
                    // de forma exclusiva para el dragón.
                    // Cuando se usan sprites genéricos con rotación ±90° (ej. Stage 1),
                    // los ejes se intercambian, así que estrechamos el otro eje.
                    bool axesSwapped = (std::abs(std::abs(renderRotation) - glm::radians(90.0f)) < 0.01f);
                    if (seg.dirIndex == 0 || seg.dirIndex == 2) {
                        if (axesSwapped)
                            scaleX *= 0.9f; // Tras rotar ±90°, scaleX es la altura visual
                        else
                            scaleY *= 0.9f; // Sin rotación, scaleY es la altura
                    } else if (seg.dirIndex == 1 || seg.dirIndex == 3) {
                        scaleX *= 0.9f; // UP/DOWN: rotación 0 o 180, no intercambia ejes
                    }
                    
                    // Si explota hacia los lados, bajar la explosión 5 píxeles
                    if (seg.dirIndex == 0 || seg.dirIndex == 2) {
                         offsetY -= (halfTile / 24.0f) * 5.0f;
                    }

                    segModel = glm::translate(segModel, glm::vec3(seg.pos.x + offsetX, seg.pos.y + offsetY, 0.0f));
                    segModel = glm::rotate(segModel, renderRotation, glm::vec3(0.0f, 0.0f, 1.0f));
                    segModel = glm::scale(segModel, glm::vec3(halfTile * scaleX, halfTile * scaleY, 1.0f));

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
        // Desplazar visualmente al escupir
        if (facing == EnemyDirection::RIGHT) renderPos.x -= halfTile;
        else if (facing == EnemyDirection::LEFT) renderPos.x += halfTile;
        // Si va a explotar hacia arriba, el sprite se mueve 5 píxeles hacia abajo
        else if (facing == EnemyDirection::UP) renderPos.y -= (halfTile / 24.0f) * 5.0f;
        // Si va a explotar hacia abajo, se queda en el centro (no altera renderPos.y)

        int fireFrame = 0;
        if (isCharging) {
            fireFrame = 0;
        } else {
            int step = (int)((10 * 0.04f - fireAnimTimer) / 0.04f);
            if (step < 0) step = 0;
            if (step > 9) step = 9;
            
            const int dragonSeq[10] = {0, 1, 2, 3, 4, 4, 3, 2, 1, 0};
            fireFrame = dragonSeq[step];
        }
        spriteName = firePrefix + std::to_string(fireFrame);
        if (gEnemyAtlas.sprites.find(spriteName) == gEnemyAtlas.sprites.end()) {
             spriteName = prefix + "0"; // fallback
        }
    }
    currentSpriteName = spriteName;

    glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
    getUvRectForSprite(gEnemyAtlas, currentSpriteName, uvRect);

    float baseHeight = 1.0f;
    auto itBase = gEnemyAtlas.sprites.find("dragon.abajo.0");
    if (itBase != gEnemyAtlas.sprites.end() && itBase->second.h > 0) {
        baseHeight = static_cast<float>(itBase->second.h);
    }

    float scaleX = 1.0f;
    float scaleY = 1.0f;
    auto it = gEnemyAtlas.sprites.find(currentSpriteName);
    if (it != gEnemyAtlas.sprites.end() && it->second.h > 0) {
        scaleX = static_cast<float>(it->second.w) / baseHeight;
        scaleY = static_cast<float>(it->second.h) / baseHeight;
    }

    renderPos.y -= (halfTile * enemyScaleFactor) * (1.0f - scaleY);
    model = glm::translate(model, renderPos);

    model = glm::scale(model, glm::vec3(halfTile * enemyScaleFactor * scaleX, halfTile * enemyScaleFactor * scaleY, 1.0f));

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
