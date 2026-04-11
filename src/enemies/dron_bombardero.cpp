#include "enemies/dron_bombardero.hpp"
#include "enemies/king_bomber.hpp"
#include "game_map.hpp"
#include "player.hpp"
#include "bomb.hpp"
#include "sprite_atlas.hpp"
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <array>

extern GLuint uniformModel;
extern GLuint uniformUvRect;
extern GLuint uniformFlipX;
extern GLuint uniformTintColor;
extern SpriteAtlas gEnemyAtlas;
extern std::vector<Enemy*> gEnemies;

class SlowFireball : public Enemy {
public:
    std::string skin;
    EnemyDirection moveDir;
    float animAccumulator;
    bool useFrame6;
    
    SlowFireball(glm::vec2 pos, EnemyDirection dir, const std::string& skinBase)
                : Enemy(pos, glm::vec2(0.2f, 0.2f), 0.26f, 1, 0, true, false), moveDir(dir), skin(skinBase),
                    animAccumulator(0.0f), useFrame6(true) {
        currentSpriteName = skin + ".fuego.6";
        lifeState = EnemyLifeState::Alive;
    }
    
    ~SlowFireball() override {}

    void Update() override {
        if (lifeState != EnemyLifeState::Alive) return;
        if (!gameMap) {
            lifeState = EnemyLifeState::Dead;
            return;
        }

        // Animacion continua 6 <-> 7 mientras la bola se mueve.
        animAccumulator += deltaTime;
        if (animAccumulator >= 0.10f) {
            animAccumulator -= 0.10f;
            useFrame6 = !useFrame6;
            currentSpriteName = skin + (useFrame6 ? ".fuego.6" : ".fuego.7");
        }

        glm::vec2 dirVec = dirToVec(moveDir);
        if (dirVec == glm::vec2(0.0f, 0.0f)) {
            lifeState = EnemyLifeState::Dead;
            return;
        }

        // Movimiento suave (sin teletransporte entre tiles).
        const float travelSpeed = speed * 3.6f;
        glm::vec2 nextPos = position + dirVec * (travelSpeed * deltaTime);

        int nr = 0, nc = 0;
        gameMap->ndcToGrid(nextPos, nr, nc);
        if (!gameMap->isWalkable(nr, nc)) {
            lifeState = EnemyLifeState::Dead;
            return;
        }

        position = nextPos;

        // Colisión por casilla: evita matar en tiles adyacentes por tamaño visual del sprite.
        if (playersList) {
            for (auto* p : *playersList) {
                if (p && p->isAlive()) {
                    int pr = 0, pc = 0;
                    gameMap->ndcToGrid(p->position, pr, pc);
                    if (pr == nr && pc == nc) {
                        p->killByExplosion();
                        lifeState = EnemyLifeState::Dead;
                        return;
                    }
                }
            }
        }

        for (auto* b : gBombs) {
            if (!b || b->state != BombState::FUSE) continue;
            if (b->gridRow == nr && b->gridCol == nc) {
                b->detonate();
                lifeState = EnemyLifeState::Dead;
                return;
            }
        }
    }
    
    void Draw() override {
        if (lifeState == EnemyLifeState::Dead || !gameMap) return;
        
        const float halfTile = gameMap->getTileSize() / 2.0f;
        glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
        getUvRectForSprite(gEnemyAtlas, currentSpriteName, uvRect);

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(position.x, position.y + halfTile * 0.95f, 0.0f));
        model = glm::scale(model, glm::vec3(halfTile * 1.5f, halfTile * 1.5f, 1.0f));

        glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
        glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));
        glUniform1f(uniformFlipX, flipX);

        glm::vec4 tint(1.0f);
        glUniform4fv(uniformTintColor, 1, glm::value_ptr(tint));
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }
    
    bool takeDamage(const SpriteAtlas&, int) override { return false; }
};

DronBombardero::DronBombardero(glm::vec2 pos, glm::vec2 size, float speed, const std::string& skinBase)
    : Enemy(pos, size, speed, 3, 2000),
      spriteSkinBase(skinBase),
      state(DronState::Normal),
      stateTimer(0.0f),
      fireCycleCooldown(5.0f),
      fireCycleTimer(5.0f),
      transformFrame(0),
      transformTimer(0.0f),
    fireBurstShotThisTransform(false)
{
    facing = randomDirection();
    spriteBaseId = spriteSkinBase;
    currentSpriteName = spriteSkinBase + ".abajo.0";
}

DronBombardero::~DronBombardero() {}

void DronBombardero::syncSkinFromCurrentSprite() {
    if (currentSpriteName.empty()) return;
    const std::size_t dot = currentSpriteName.find('.');
    if (dot == std::string::npos) return;
    const std::string parsed = currentSpriteName.substr(0, dot);
    if (parsed.rfind("dron", 0) == 0) {
        spriteSkinBase = parsed;
        spriteBaseId = parsed;
    }
}

void DronBombardero::updateWalkAnimation(bool isFireMode) {
    animTimer += deltaTime;
    if (animTimer >= 0.15f) {
        animTimer -= 0.15f;
        animFrame = (animFrame + 1) % 3;
    }

    if (isFireMode) {
        currentSpriteName = spriteSkinBase + ".fuego." + (animFrame % 2 == 0 ? "4" : "5");
        flipX = 0.0f;
        return;
    }

    std::string prefix;
    flipX = 0.0f;
    switch (facing) {
        case EnemyDirection::RIGHT: prefix = spriteSkinBase + ".derecha."; break;
        case EnemyDirection::LEFT:  prefix = spriteSkinBase + ".derecha."; flipX = 1.0f; break;
        case EnemyDirection::UP:    prefix = spriteSkinBase + ".arriba.";  break;
        case EnemyDirection::DOWN:  prefix = spriteSkinBase + ".abajo.";   break;
        default:                    prefix = spriteSkinBase + ".abajo.";   break;
    }
    currentSpriteName = prefix + std::to_string(animFrame);
}

void DronBombardero::shootSlowFireball(EnemyDirection dir) {
    SlowFireball* fireball = new SlowFireball(position, dir, spriteSkinBase);
    fireball->setContext(gameMap, playersList);
    gEnemies.push_back(fireball);
}

void DronBombardero::shootBurstFireballs() {
    const std::array<EnemyDirection, 4> dirs = {
        EnemyDirection::UP,
        EnemyDirection::LEFT,
        EnemyDirection::DOWN,
        EnemyDirection::RIGHT
    };
    for (EnemyDirection dir : dirs) {
        shootSlowFireball(dir);
    }
}

void DronBombardero::updateNormal(float dist, float step) {
    fireCycleTimer -= deltaTime;
    if (fireCycleTimer <= 0.0f) {
        state = DronState::TransformingToFire;
        transformFrame = 0;
        transformTimer = 0.0f;
        fireBurstShotThisTransform = false;
        return;
    }

    if (dist > 0.1f) {
        EnemyDirection toPlayer = directionTowardPlayer();
        if (!tryMove(toPlayer, step)) {
            EnemyDirection alt = randomDirection();
            tryMove(alt, step);
        } else {
            facing = toPlayer;
        }
    }
    updateWalkAnimation(false);
}

void DronBombardero::updateTransformingToFire() {
    transformTimer += deltaTime;
    if (transformTimer >= 0.1f) {
        transformTimer -= 0.1f;
        transformFrame++;
        if (transformFrame > 4) {
            state = DronState::FireMode;
            stateTimer = 5.0f;
            return;
        }
    }

    if (transformFrame == 4 && !fireBurstShotThisTransform) {
        shootBurstFireballs();
        fireBurstShotThisTransform = true;
    }

    currentSpriteName = spriteSkinBase + ".fuego." + std::to_string(transformFrame);
}

void DronBombardero::updateFireMode(float dist, float step) {
    stateTimer -= deltaTime;
    if (stateTimer <= 0.0f) {
        state = DronState::TransformingToNormal;
        transformFrame = 4;
        transformTimer = 0.0f;
        return;
    }

    if (dist > 0.1f) {
        float fastStep = step * 1.4f;
        EnemyDirection toPlayer = directionTowardPlayer();
        if (!tryMove(toPlayer, fastStep)) {
            EnemyDirection alt = randomDirection();
            tryMove(alt, fastStep);
        } else {
            facing = toPlayer;
        }
    }
    updateWalkAnimation(true);
}

void DronBombardero::updateTransformingToNormal() {
    transformTimer += deltaTime;
    if (transformTimer >= 0.1f) {
        transformTimer -= 0.1f;
        transformFrame--;
        if (transformFrame < 0) {
            state = DronState::Normal;
            fireCycleTimer = fireCycleCooldown;
            return;
        }
    }
    currentSpriteName = spriteSkinBase + ".fuego." + std::to_string(transformFrame);
}

void DronBombardero::Update() {
    if (lifeState != EnemyLifeState::Alive) return;

    if (spriteSkinBase.empty()) {
        spriteSkinBase = "dronrosa";
        spriteBaseId = spriteSkinBase;
    }
    syncSkinFromCurrentSprite();

    if (IsKingPreBattleLockActive()) {
        flipX = 0.0f;
        currentSpriteName = spriteSkinBase + ".fuego.0";
        return;
    }

    float step = speed * deltaTime;
    float dist = distanceToPlayer();

    switch (state) {
        case DronState::Normal:               updateNormal(dist, step); break;
        case DronState::TransformingToFire:   updateTransformingToFire(); break;
        case DronState::FireMode:             updateFireMode(dist, step); break;
        case DronState::TransformingToNormal: updateTransformingToNormal(); break;
    }
}

bool DronBombardero::takeDamage(const SpriteAtlas& atlas, int amount) {
    if (state != DronState::Normal && lifeState == EnemyLifeState::Alive) {
        return false;
    }
    return Enemy::takeDamage(atlas, amount);
}

void DronBombardero::Draw() {
    if (lifeState == EnemyLifeState::Dead || !gameMap) return;
    if (IsKingPreBattleLockActive() && !IsKingPreBattleBlinkVisible()) return;

    const float enemyScaleFactor = 1.8f;
    const float halfTile = gameMap->getTileSize() / 2.0f;

    glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
    getUvRectForSprite(gEnemyAtlas, currentSpriteName, uvRect);

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(position.x, position.y + halfTile * 0.95f, 0.0f));
    model = glm::scale(model, glm::vec3(halfTile * enemyScaleFactor, halfTile * enemyScaleFactor, 1.0f));

    glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
    glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));
    glUniform1f(uniformFlipX, flipX);

    glm::vec4 tint(1.0f);
    glUniform4fv(uniformTintColor, 1, glm::value_ptr(tint));

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}