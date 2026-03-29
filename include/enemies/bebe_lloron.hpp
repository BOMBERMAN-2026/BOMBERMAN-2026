#ifndef BEBE_LLORON_HPP
#define BEBE_LLORON_HPP

#include "enemy.hpp"

/*
 * Bebé llorón – 800 pts, 1 HP
 * Patrulla un área básica, pero perseguirá a Bomberman
 * sin mucha insistencia si se le presenta la oportunidad.
 */
class BebeLloron : public Enemy {
public:
    BebeLloron(glm::vec2 pos, glm::vec2 size, float speed);
    ~BebeLloron() override;

    void Update() override;
    void Draw()   override;

private:
    float pursuitRange;        // Distancia a la que empieza a perseguir
    float pursuitGiveUpRange;  // Distancia a la que deja de perseguir
    bool  pursuing;            // Estado actual de persecución
    float patrolTimer;         // Timer para cambios de dirección en patrulla
    float patrolInterval;      // Intervalo de cambio al patrullar

    bool hasLineOfSightToPlayer() const; // Comprueba si no hay bloques bloqueando la visión
};

#endif // BEBE_LLORON_HPP
