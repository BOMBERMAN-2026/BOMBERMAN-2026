#ifndef BABOSA_HPP
#define BABOSA_HPP

#include "enemy.hpp"

/*
 * Babosa – 400 pts, 1 HP
 * Actúa de forma muy similar al cangrejo (patrulla),
 * pero cambia de dirección más a menudo.
 */
class Babosa : public Enemy {
public:
    Babosa(glm::vec2 pos, glm::vec2 size, float speed);
    ~Babosa() override;

    void Update() override;
    void Draw()   override;

private:
    // IA
    float dirChangeTimer;     // Tiempo acumulado desde el último cambio
    float dirChangeInterval;  // Intervalo corto → cambia frecuentemente
};

#endif // BABOSA_HPP
