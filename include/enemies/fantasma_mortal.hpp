#ifndef FANTASMA_MORTAL_HPP
#define FANTASMA_MORTAL_HPP

#include "enemy.hpp"

/*
 * Fantasma mortal – 1000 pts, 1 HP
 * Se mueve lentamente hacia la dirección general de Bomberman,
 * y al ver una bomba simplemente se da la vuelta.
 * Puede atravesar bloques blandos (destructibles).
 */
class FantasmaMortal : public Enemy {
public:
    static constexpr float kSpeed        = 0.20f;
    static constexpr float kBlockSlowdown = 0.35f;

    FantasmaMortal(glm::vec2 pos, glm::vec2 size);
    ~FantasmaMortal() override;

    void Update() override;
    void Draw()   override;

    // Notifica al fantasma de que hay una bomba cerca para que gire 180°.
    void notifyBombNearby(glm::vec2 bombPos) override;

    EnemyDirection findPathToPlayer() const;

private:
    float movementStepForDirection(EnemyDirection dir) const;

    // Evita girar en bucle continuo mientras la misma bomba está cerca.
    float bombTurnCooldown;
    EnemyDirection bombEscapeDir;
    float bombAvoidTimer;
    glm::vec2 lastBombPos;
};

#endif // FANTASMA_MORTAL_HPP
