#ifndef KING_BOMBER_HPP
#define KING_BOMBER_HPP

#include "enemy.hpp"
#include <vector>

bool IsKingPreBattleLockActive();
bool IsKingPreBattleBlinkVisible();

/*
 * King Bomber – 77000 pts, 3 fases (JEFE)
 *
 * Mecánicas:
 * - Movimiento continuo semi-aleatorio condicionado por colisiones del mapa.
 * - Colocación periódica de bombas con límite de bombas activas propias.
 * - Evasión básica: si su tile entra en peligro por explosiones activas
 *   o inminentes, prioriza moverse a una celda segura.
 * - Escala agresividad por fase (sprites kingbomber1/2/3).
 */

class KingBomber : public Enemy {
public:
    KingBomber(glm::vec2 pos, glm::vec2 size, float speed);
    ~KingBomber() override;

    void Update() override;
    void Draw()   override;
    bool takeDamage(const SpriteAtlas& atlas, int amount = 1) override;
    void startDying(const SpriteAtlas& atlas) override;
    void updateDeath(float dt) override;

    bool isShieldActive() const { return shieldActive; }
    bool isRetreating()   const { return false; }

private:
    enum class BattleState {
        IntroBlink,
        IntroGo,
        ShieldedWithDrones,
        CombatLoop,
        Dying
    };

    enum class SpecialAttackState {
        Idle,
        Charging,
        TeleportDelay,
        BlastSequence
    };

    BattleState battleState;
    SpecialAttackState specialState;

    // Intro de combate
    float introBlinkTimer;
    float introBlinkDuration;
    float introGoTimer;
    float introGoDuration;
    float blinkToggleTimer;
    float blinkInterval;
    bool blinkVisible;

    // Escudo cíclico post-drones
    bool shieldActive;
    float shieldStateTimer;
    float shieldOnDuration;
    float shieldOffDuration;
    float shieldAnimTimer;
    int shieldAnimFrame;
    float shieldAnimFrameInterval;
    bool dronesCleared;

    // Estado y navegación
    EnemyDirection plannedDir;
    int stepsUntilReevaluate;

    // Bombas
    float bombCooldown;
    float bombCooldownMax;
    int maxOwnedBombs;
    int bombPower;
    float ownBombEvadeTimer;
    float ownBombEvadeDuration;

    // Ataque especial
    float specialCooldownTimer;
    float specialCooldown;
    float specialChargeTimer;
    float specialChargeDuration;
    float specialTeleportDelayTimer;
    float specialTeleportDelay;
    float specialBlastFrameTimer;
    float specialBlastFrameInterval;
    int specialBlastFrame;
    float specialBombLockBeforeDuration;
    float specialBombLockAfterTimer;
    float specialBombLockAfterDuration;

    // Evita recibir múltiples impactos por una misma explosión en frames consecutivos.
    float damageGraceTimer;
    float damageGraceDuration;
    float phaseChangeGraceDuration;

    // Seguimiento de bombas propias para respetar límite simultáneo
    std::vector<glm::ivec2> ownedBombTiles;

    // Fase visual por vida (1..3)
    int phaseIndex;

    // Muerte final (usa kingbomber3.muerto.N)
    float phaseDeathTimer;
    int phaseDeathFrame;
    float phaseDeathFrameInterval;
    float phaseDeathHoldTimer;
    float phaseDeathHoldDuration;
    float phaseDeathHoldBlinkTimer;
    float phaseDeathHoldBlinkInterval;

    bool hasAliveDrones() const;
    void updatePhaseAggression();
    void updateBlink(float dt);
    void updateShieldCycle(float dt);
    void updateBombOwnershipState();
    bool isBombStillActiveAt(const glm::ivec2& tile) const;
    bool hasActiveOwnedBombs() const;
    bool canPlaceBombSafelyAt(int row, int col) const;
    bool isTileDangerous(int row, int col) const;
    bool isTileBlockedForMovement(int row, int col) const;
    bool hasLineOfFireToBomb(int fromRow, int fromCol, int bombRow, int bombCol, int power) const;
    EnemyDirection chooseSafeDirection() const;
    EnemyDirection chooseSafeDirectionAvoiding(EnemyDirection blockedDir) const;
    EnemyDirection chooseEscapeDirectionFromBomb(int bombRow, int bombCol, int power) const;
    EnemyDirection chooseExplorationDirection() const;
    EnemyDirection chooseBiasedDirectionTowardPlayer(float towardWeight) const;
    bool tryFindTeleportNearPlayer(glm::vec2& outPos) const;
    void emitSpecialCrossExplosion(int stepDist);
    void updateMovement(float dt, float towardWeight, bool avoidDanger);
    void updateSpecialAttack(float dt);
    void maybePlaceBomb();
    void placeBombOnCurrentTile();
    std::string phaseBase() const;
};

#endif // KING_BOMBER_HPP
