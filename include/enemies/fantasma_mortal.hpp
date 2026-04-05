#ifndef FANTASMA_MORTAL_HPP
#define FANTASMA_MORTAL_HPP

#include "enemy.hpp"

/*
 * Fantasma mortal – 1000 pts, 1 HP
 * Se mueve lentamente hacia la dirección general de Bomberman,
 * pero retrocederá rápidamente al ver una bomba.
 * Puede atravesar bloques blandos (destructibles).
 */
class FantasmaMortal : public Enemy {
public:
    FantasmaMortal(glm::vec2 pos, glm::vec2 size, float speed);
    ~FantasmaMortal() override;

    void Update() override;
    void Draw()   override;

    // Notifica al fantasma de que hay una bomba cerca para que retroceda
    void notifyBombNearby(glm::vec2 bombPos) override;

    EnemyDirection findPathToPlayer() const;

private:
    bool  retreating;       // Está retrocediendo de una bomba
    
    float retreatTimer;     // Tiempo restante de retroceso
    float retreatSpeed;     // Velocidad al retroceder (más rápida)
    float normalSpeed;      // Velocidad normal guardada
    glm::vec2 retreatDir;   // Dirección de huida
};

#endif // FANTASMA_MORTAL_HPP
