#include "enemies/king_bomber.hpp"
#include "enemies/dron_bombardero.hpp"
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
#include <vector>

extern GLuint uniformModel;
extern GLuint uniformUvRect;
extern GLuint uniformFlipX;
extern GLuint uniformTintColor;
extern SpriteAtlas gEnemyAtlas;
extern std::vector<Enemy*> gEnemies;

namespace {
bool gKingPreBattleLockActive = false;
bool gKingPreBattleBlinkVisible = true;
}

bool IsKingPreBattleLockActive() {
    return gKingPreBattleLockActive;
}

bool IsKingPreBattleBlinkVisible() {
    return gKingPreBattleBlinkVisible;
}

KingBomber::KingBomber(glm::vec2 pos, glm::vec2 size, float speed)
    : Enemy(pos, size, speed, /*hp=*/3, /*score=*/77000, /*passSoftBlocks=*/false, /*boss=*/true),
      battleState(BattleState::IntroBlink),
      specialState(SpecialAttackState::Idle),
      introBlinkTimer(0.0f),
      introBlinkDuration(2.4f),
      introGoTimer(0.0f),
      introGoDuration(1.1f),
      blinkToggleTimer(0.0f),
      blinkInterval(0.12f),
      blinkVisible(true),
      shieldActive(true),
      shieldStateTimer(0.0f),
      shieldOnDuration(4.0f),
      shieldOffDuration(3.2f),
      dronesCleared(false),
      plannedDir(EnemyDirection::NONE),
      stepsUntilReevaluate(0),
      bombCooldown(0.0f),
      bombCooldownMax(2.4f),
      maxOwnedBombs(2),
      bombPower(2),
      specialCooldownTimer(0.0f),
      specialCooldown(7.0f),
      specialChargeTimer(0.0f),
      specialChargeDuration(1.8f),
      specialTeleportDelayTimer(0.0f),
      specialTeleportDelay(0.45f),
      specialBlastFrameTimer(0.0f),
      specialBlastFrameInterval(0.12f),
      specialBlastFrame(1),
    damageGraceTimer(0.0f),
    damageGraceDuration(0.45f),
      phaseIndex(1),
      phaseDeathTimer(0.0f),
      phaseDeathFrame(0),
      phaseDeathFrameInterval(0.10f)
{
    spriteBaseId = phaseBase();
    currentSpriteName = spriteBaseId + ".fuego.0";
    gKingPreBattleLockActive = true;
    gKingPreBattleBlinkVisible = true;
}

KingBomber::~KingBomber() {
    gKingPreBattleLockActive = false;
    gKingPreBattleBlinkVisible = true;
}

std::string KingBomber::phaseBase() const {
    return std::string("kingbomber") + std::to_string(std::max(1, std::min(3, phaseIndex)));
}

bool KingBomber::hasAliveDrones() const {
    for (Enemy* e : gEnemies) {
        if (!e) continue;
        if (e == this) continue;
        if (dynamic_cast<DronBombardero*>(e) && e->lifeState != EnemyLifeState::Dead) {
            return true;
        }
    }
    return false;
}

void KingBomber::updatePhaseAggression() {
    spriteBaseId = phaseBase();
    if (phaseIndex == 1) {
        speed = 0.072f;
        bombCooldownMax = 2.4f;
        maxOwnedBombs = 2;
        specialCooldown = 7.0f;
    } else if (phaseIndex == 2) {
        speed = 0.088f;
        bombCooldownMax = 1.75f;
        maxOwnedBombs = 3;
        specialCooldown = 5.6f;
    } else {
        speed = 0.108f;
        bombCooldownMax = 1.15f;
        maxOwnedBombs = 4;
        specialCooldown = 4.4f;
    }
}

void KingBomber::updateBlink(float dt) {
    blinkToggleTimer += dt;
    if (blinkToggleTimer >= blinkInterval) {
        blinkToggleTimer -= blinkInterval;
        blinkVisible = !blinkVisible;
    }
}

void KingBomber::updateShieldCycle(float dt) {
    if (!dronesCleared) {
        shieldActive = true;
        shieldStateTimer = 0.0f;
        return;
    }

    shieldStateTimer += dt;
    if (shieldActive && shieldStateTimer >= shieldOnDuration) {
        shieldActive = false;
        shieldStateTimer = 0.0f;
    } else if (!shieldActive && shieldStateTimer >= shieldOffDuration) {
        shieldActive = true;
        shieldStateTimer = 0.0f;
    }
}

void KingBomber::updateBombOwnershipState() {
    for (auto it = ownedBombTiles.begin(); it != ownedBombTiles.end();) {
        if (!isBombStillActiveAt(*it)) {
            it = ownedBombTiles.erase(it);
        } else {
            ++it;
        }
    }
}

bool KingBomber::isBombStillActiveAt(const glm::ivec2& tile) const {
    for (auto* b : gBombs) {
        if (!b || b->state == BombState::DONE) continue;
        if (b->gridRow == tile.x && b->gridCol == tile.y) return true;
    }
    return false;
}

bool KingBomber::hasLineOfFireToBomb(int fromRow, int fromCol, int bombRow, int bombCol, int power) const {
    if (!gameMap) return false;

    if (fromRow == bombRow) {
        const int minC = std::min(fromCol, bombCol);
        const int maxC = std::max(fromCol, bombCol);
        if ((maxC - minC) > power) return false;
        for (int c = minC + 1; c < maxC; ++c) {
            if (!gameMap->isWalkable(fromRow, c)) return false;
        }
        return true;
    }

    if (fromCol == bombCol) {
        const int minR = std::min(fromRow, bombRow);
        const int maxR = std::max(fromRow, bombRow);
        if ((maxR - minR) > power) return false;
        for (int r = minR + 1; r < maxR; ++r) {
            if (!gameMap->isWalkable(r, fromCol)) return false;
        }
        return true;
    }

    return false;
}

bool KingBomber::isTileDangerous(int row, int col) const {
    if (!gameMap) return false;

    for (auto* b : gBombs) {
        if (!b || b->state == BombState::DONE) continue;

        const bool explodingNow = (b->state == BombState::EXPLODING);
        const float remaining = b->fuseTime - b->fuseTimer;
        const bool imminent = (b->state == BombState::FUSE && remaining <= 1.25f);
        if (!explodingNow && !imminent) continue;

        if (hasLineOfFireToBomb(row, col, b->gridRow, b->gridCol, b->power)) {
            return true;
        }
    }

    return false;
}

EnemyDirection KingBomber::chooseSafeDirection() const {
    if (!gameMap) return EnemyDirection::NONE;

    std::vector<EnemyDirection> dirs = {
        EnemyDirection::UP, EnemyDirection::DOWN, EnemyDirection::LEFT, EnemyDirection::RIGHT
    };
    std::random_shuffle(dirs.begin(), dirs.end());

    for (EnemyDirection d : dirs) {
        if (!canMoveInDirection(d, gameMap->getTileSize() * 0.95f)) continue;
        glm::vec2 nextPos = position + dirToVec(d) * (gameMap->getTileSize() * 1.02f);
        int nr = 0, nc = 0;
        gameMap->ndcToGrid(nextPos, nr, nc);
        if (!isTileDangerous(nr, nc)) return d;
    }

    return EnemyDirection::NONE;
}

EnemyDirection KingBomber::chooseExplorationDirection() const {
    if (!gameMap) return EnemyDirection::NONE;

    std::vector<EnemyDirection> dirs = {
        EnemyDirection::UP, EnemyDirection::DOWN, EnemyDirection::LEFT, EnemyDirection::RIGHT
    };
    std::random_shuffle(dirs.begin(), dirs.end());

    for (EnemyDirection d : dirs) {
        if (canMoveInDirection(d, gameMap->getTileSize() * 0.95f)) return d;
    }

    return EnemyDirection::NONE;
}

EnemyDirection KingBomber::chooseBiasedDirectionTowardPlayer(float towardWeight) const {
    EnemyDirection toward = directionTowardPlayer();
    if (toward != EnemyDirection::NONE && ((std::rand() % 100) < static_cast<int>(towardWeight * 100.0f))) {
        if (canMoveInDirection(toward, gameMap->getTileSize() * 0.95f)) {
            return toward;
        }
    }
    return chooseExplorationDirection();
}

void KingBomber::updateMovement(float dt, float towardWeight, bool avoidDanger) {
    if (!gameMap) return;

    const float step = speed * dt;
    EnemyDirection moveDir = EnemyDirection::NONE;

    int row = 0, col = 0;
    gameMap->ndcToGrid(position, row, col);

    if (avoidDanger && isTileDangerous(row, col)) {
        moveDir = chooseSafeDirection();
        if (moveDir != EnemyDirection::NONE) {
            plannedDir = moveDir;
            stepsUntilReevaluate = 2 + (std::rand() % 3);
        }
    }

    if (moveDir == EnemyDirection::NONE) {
        if (stepsUntilReevaluate <= 0 || plannedDir == EnemyDirection::NONE ||
            !canMoveInDirection(plannedDir, gameMap->getTileSize() * 0.95f)) {
            plannedDir = chooseBiasedDirectionTowardPlayer(towardWeight);
            stepsUntilReevaluate = 3 + (std::rand() % 6);
        }
        moveDir = plannedDir;
    }

    bool moved = false;
    if (moveDir != EnemyDirection::NONE) {
        moved = tryMove(moveDir, step);
    }

    if (!moved) {
        plannedDir = chooseExplorationDirection();
        if (plannedDir != EnemyDirection::NONE) {
            tryMove(plannedDir, step);
        }
        stepsUntilReevaluate = 1;
    } else {
        --stepsUntilReevaluate;
    }
}

void KingBomber::placeBombOnCurrentTile() {
    if (!gameMap) return;

    int r = 0, c = 0;
    gameMap->ndcToGrid(position, r, c);

    for (auto* b : gBombs) {
        if (!b || b->state == BombState::DONE) continue;
        if (b->gridRow == r && b->gridCol == c) {
            bombCooldown = 0.30f;
            return;
        }
    }

    auto* bomb = new Bomb(gameMap->gridToNDC(r, c), r, c, /*owner=*/nullptr, bombPower, /*remote=*/false);
    bomb->fuseTime = 2.0f;
    bomb->fuseTimer = 0.0f;
    gBombs.push_back(bomb);
    ownedBombTiles.push_back(glm::ivec2(r, c));
    bombCooldown = bombCooldownMax;
}

void KingBomber::maybePlaceBomb() {
    if (bombCooldown > 0.0f) return;
    if ((int)ownedBombTiles.size() >= maxOwnedBombs) return;
    placeBombOnCurrentTile();
}

bool KingBomber::tryFindTeleportNearPlayer(glm::vec2& outPos) const {
    if (!gameMap || !playersList || playersList->empty()) return false;

    float dist = 0.0f;
    glm::vec2 ppos = getClosestPlayerPos(dist);
    if (dist >= 99999.0f) return false;

    std::vector<glm::ivec2> offsets = {
        {0, 2}, {0, -2}, {2, 0}, {-2, 0},
        {1, 2}, {-1, 2}, {1, -2}, {-1, -2},
        {2, 1}, {2, -1}, {-2, 1}, {-2, -1}
    };
    std::random_shuffle(offsets.begin(), offsets.end());

    int pr = 0, pc = 0;
    gameMap->ndcToGrid(ppos, pr, pc);

    for (const auto& of : offsets) {
        int tr = pr + of.x;
        int tc = pc + of.y;
        if (!gameMap->isWalkable(tr, tc)) continue;
        glm::vec2 pos = gameMap->gridToNDC(tr, tc);
        if (glm::distance(pos, ppos) < gameMap->getTileSize() * 1.4f) continue;
        outPos = pos;
        return true;
    }

    return false;
}

void KingBomber::emitSpecialCrossExplosion(int stepDist) {
    if (!gameMap || !playersList) return;

    int r = 0, c = 0;
    gameMap->ndcToGrid(position, r, c);

    const int dr[4] = {-1, 1, 0, 0};
    const int dc[4] = {0, 0, -1, 1};

    // Draw center tile on step 1
    if (stepDist == 1) {
        auto* centerFire = new Bomb(gameMap->gridToNDC(r, c), r, c, nullptr, 0, false);
        centerFire->fuseTime = 0.0f;
        centerFire->detonate();
        gBombs.push_back(centerFire);
    }

    for (int d = 0; d < 4; ++d) {
        bool blocked = false;
        for (int i = 1; i <= stepDist; ++i) {
            int checkR = r + dr[d] * i;
            int checkC = c + dc[d] * i;
            if (!gameMap->isWalkable(checkR, checkC)) {
                blocked = true;
                break;
            }
        }
        if (blocked) continue;

        int nr = r + dr[d] * stepDist;
        int nc = c + dc[d] * stepDist;

        for (auto* p : *playersList) {
            if (!p || !p->isAlive()) continue;
            int pr = 0, pc = 0;
            gameMap->ndcToGrid(p->position, pr, pc);
            if (pr == nr && pc == nc) {
                p->killByExplosion();
            }
        }

        for (auto* b : gBombs) {
            if (!b || b->state != BombState::FUSE) continue;
            if (b->gridRow == nr && b->gridCol == nc) {
                b->detonate();
            }
        }

        // Crear bomba fake que detone inmediatamente para mostrar fuego visual
        auto* visualFire = new Bomb(gameMap->gridToNDC(nr, nc), nr, nc, nullptr, 0, false);
        visualFire->fuseTime = 0.0f;
        visualFire->detonate();
        gBombs.push_back(visualFire);
    }
}

void KingBomber::updateSpecialAttack(float dt) {
    if (specialState == SpecialAttackState::Idle) {
        specialCooldownTimer -= dt;
        if (specialCooldownTimer <= 0.0f) {
            specialCooldownTimer = specialCooldown;
            specialState = SpecialAttackState::Charging;
            specialChargeTimer = specialChargeDuration;
            currentSpriteName = phaseBase() + ".fuego.0";
        }
        return;
    }

    if (specialState == SpecialAttackState::Charging) {
        updateBlink(dt);
        specialChargeTimer -= dt;
        currentSpriteName = phaseBase() + ".fuego.0";
        if (specialChargeTimer <= 0.0f) {
            glm::vec2 tp;
            if (tryFindTeleportNearPlayer(tp)) {
                position = tp;
                this->movingToTarget = false; // Resetear movimiento por tiles despues de teleport
            }
            specialState = SpecialAttackState::TeleportDelay;
            specialTeleportDelayTimer = specialTeleportDelay;
        }
        return;
    }

    if (specialState == SpecialAttackState::TeleportDelay) {
        updateBlink(dt);
        specialTeleportDelayTimer -= dt;
        currentSpriteName = phaseBase() + ".fuego.0";
        if (specialTeleportDelayTimer <= 0.0f) {
            blinkVisible = true;
            specialState = SpecialAttackState::BlastSequence;
            specialBlastFrame = 1;
            specialBlastFrameTimer = 0.0f;
            currentSpriteName = phaseBase() + ".fuego.1";
        }
        return;
    }

    if (specialState == SpecialAttackState::BlastSequence) {
        specialBlastFrameTimer += dt;
        while (specialBlastFrameTimer >= specialBlastFrameInterval) {
            specialBlastFrameTimer -= specialBlastFrameInterval;
            
            // Generate fire outward step by step
            emitSpecialCrossExplosion(specialBlastFrame);
            
            ++specialBlastFrame;
            if (specialBlastFrame > 4) {
                specialState = SpecialAttackState::Idle;
                return;
            }
        }
        currentSpriteName = phaseBase() + ".fuego." + std::to_string(std::max(1, std::min(4, specialBlastFrame)));
    }
}

void KingBomber::Update() {
    if (lifeState != EnemyLifeState::Alive) return;
    if (!gameMap) return;

    if (damageGraceTimer > 0.0f) {
        damageGraceTimer -= deltaTime;
        if (damageGraceTimer < 0.0f) damageGraceTimer = 0.0f;
    }

    updatePhaseAggression();
    updateBombOwnershipState();

    if (!dronesCleared && !hasAliveDrones()) {
        dronesCleared = true;
        shieldActive = false;
        shieldStateTimer = 0.0f;
        specialCooldownTimer = std::min(specialCooldownTimer, 2.0f);
    }

    switch (battleState) {
        case BattleState::IntroBlink:
            gKingPreBattleLockActive = true;
            updateBlink(deltaTime);
            gKingPreBattleBlinkVisible = blinkVisible;
            currentSpriteName = phaseBase() + ".fuego.0";
            introBlinkTimer += deltaTime;
            if (introBlinkTimer >= introBlinkDuration) {
                battleState = BattleState::IntroGo;
                blinkVisible = true;
                gKingPreBattleBlinkVisible = true;
                introGoTimer = 0.0f;
                currentSpriteName = phaseBase() + ".go.0";
            }
            return;

        case BattleState::IntroGo:
            gKingPreBattleLockActive = true;
            gKingPreBattleBlinkVisible = true;
            currentSpriteName = phaseBase() + ".go.0";
            introGoTimer += deltaTime;
            if (introGoTimer >= introGoDuration) {
                battleState = BattleState::ShieldedWithDrones;
                gKingPreBattleLockActive = false;
                gKingPreBattleBlinkVisible = true;
                plannedDir = chooseExplorationDirection();
                stepsUntilReevaluate = 4;
            }
            return;

        case BattleState::ShieldedWithDrones:
            shieldActive = true;
            updateMovement(deltaTime, 0.38f, false);
            if (dronesCleared) {
                battleState = BattleState::CombatLoop;
                specialState = SpecialAttackState::Idle;
                specialCooldownTimer = 2.4f;
            }
            break;

        case BattleState::CombatLoop:
            if (specialState == SpecialAttackState::Idle) {
                updateShieldCycle(deltaTime);
                bombCooldown -= deltaTime;
                if (bombCooldown < 0.0f) bombCooldown = 0.0f;

                if (shieldActive) {
                    updateMovement(deltaTime, 0.30f, false);
                } else {
                    updateMovement(deltaTime, 0.45f, true);
                    maybePlaceBomb();
                }
            }
            updateSpecialAttack(deltaTime);
            break;

        case BattleState::Dying:
            return;
    }

    if (specialState == SpecialAttackState::Idle) {
        animTimer += deltaTime;
        if (animTimer >= 0.2f) {
            animTimer = 0.0f;
            animFrame = (animFrame + 1) % 3;
        }

        std::string prefix;
        flipX = 0.0f;
        switch (facing) {
            case EnemyDirection::RIGHT: prefix = phaseBase() + ".derecha."; break;
            case EnemyDirection::LEFT:  prefix = phaseBase() + ".derecha."; flipX = 1.0f; break;
            case EnemyDirection::UP:    prefix = phaseBase() + ".arriba."; break;
            case EnemyDirection::DOWN:  prefix = phaseBase() + ".abajo."; break;
            default:                    prefix = phaseBase() + ".abajo."; break;
        }
        currentSpriteName = prefix + std::to_string(animFrame);
    }
}

bool KingBomber::takeDamage(const SpriteAtlas& atlas, int amount) {
    (void)amount;
    if (lifeState != EnemyLifeState::Alive) return false;
    if (!dronesCleared) return false;
    if (shieldActive) return false;
    if (battleState != BattleState::CombatLoop) return false;
    if (damageGraceTimer > 0.0f) return false;
    if (specialState != SpecialAttackState::Idle) return false;

    if (phaseIndex < 3) {
        ++phaseIndex;
        updatePhaseAggression();
        damageGraceTimer = damageGraceDuration;
        return false;
    }

    damageGraceTimer = damageGraceDuration;
    startDying(atlas);
    return true;
}

void KingBomber::startDying(const SpriteAtlas& atlas) {
    (void)atlas;
    if (lifeState != EnemyLifeState::Alive) return;

    battleState = BattleState::Dying;
    specialState = SpecialAttackState::Idle;
    gKingPreBattleLockActive = false;
    gKingPreBattleBlinkVisible = true;

    lifeState = EnemyLifeState::Dying;
    alive = true;

    phaseDeathTimer = 0.0f;
    phaseDeathFrame = 0;
    blinkVisible = true;
    flipX = 0.0f;
    currentSpriteName = phaseBase() + ".fuego.0";
}

void KingBomber::updateDeath(float dt) {
    if (lifeState != EnemyLifeState::Dying) return;

    phaseDeathTimer += dt;
    while (phaseDeathTimer >= phaseDeathFrameInterval) {
        phaseDeathTimer -= phaseDeathFrameInterval;
        ++phaseDeathFrame;

        if (phaseDeathFrame > 4) {
            lifeState = EnemyLifeState::Dead;
            alive = false;
            gKingPreBattleLockActive = false;
            gKingPreBattleBlinkVisible = true;
            return;
        }
    }

    currentSpriteName = phaseBase() + ".fuego." + std::to_string(std::max(0, std::min(4, phaseDeathFrame)));
}

void KingBomber::Draw() {
    if (lifeState == EnemyLifeState::Dead || !gameMap) return;

    const bool hideByBlink = ((battleState == BattleState::IntroBlink ||
                               specialState == SpecialAttackState::Charging ||
                               specialState == SpecialAttackState::TeleportDelay) && !blinkVisible);
    if (hideByBlink) return;

    const float halfTile = gameMap->getTileSize() * 0.5f;

    glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
    getUvRectForSprite(gEnemyAtlas, currentSpriteName, uvRect);

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(position.x, position.y + halfTile * 0.95f, 0.0f));
    model = glm::scale(model, glm::vec3(halfTile * 2.2f, halfTile * 2.2f, 1.0f));

    glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
    glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));
    glUniform1f(uniformFlipX, flipX);

    glm::vec4 tint(1.0f);
    if (shieldActive && battleState != BattleState::IntroBlink && battleState != BattleState::IntroGo) {
        tint = glm::vec4(0.72f, 0.92f, 1.0f, 1.0f);
    }
    glUniform4fv(uniformTintColor, 1, glm::value_ptr(tint));

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}
