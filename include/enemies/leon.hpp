#ifndef LEON_HPP
#define LEON_HPP

#include "enemy.hpp"

/*
 * León – 100 pts, 1 HP
 * Se mueve de un lado a otro de forma repetitiva,
 * cambiando rara vez de dirección.
 */
class Leon : public Enemy {
public:
    Leon(glm::vec2 pos, glm::vec2 size, float speed);
    ~Leon() override;

    void Update() override;
    void Draw()   override;

private:
    float dirChangeTimer;           // Tiempo acumulado desde el último cambio
    float dirChangeInterval;        // Intervalo mínimo entre cambios de dirección
    float dirChangeChance;          // Probabilidad de cambiar al chocar (baja)
};

#endif // LEON_HPP
