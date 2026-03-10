#ifndef KING_BOMBER_HPP
#define KING_BOMBER_HPP

#include "enemy.hpp"
#include <array>

/*
 * King Bomber – 77000 pts, 5 HP (JEFE)
 *
 * Mecánicas:
 * - Tiene 4 drones que persiguen al jugador.
 * - Los drones periódicamente estallan en llamas, enviando bolas de fuego
 *   por los pasillos mientras siguen al jugador.
 * - King Bomber se mueve en la dirección general del jugador colocando
 *   bombas en cadena.
 * - Cuando sus bombas explotan, su escudo se desactiva y se retira
 *   → es vulnerable a las bombas del jugador.
 * - El escudo se recupera poco después.
 */

// Dron acompañante de King Bomber
struct KingDrone {
    glm::vec2 position;
    glm::vec2 direction;
    float speed;
    bool onFire;
    float fireTimer;
    bool alive;

    KingDrone()
        : position(0.0f), direction(0.0f), speed(0.0f),
          onFire(false), fireTimer(0.0f), alive(true) {}
};

class KingBomber : public Enemy {
public:
    KingBomber(glm::vec2 pos, glm::vec2 size, float speed);
    ~KingBomber() override;

    void Update() override;
    void Draw()   override;

    bool isShieldActive() const { return shieldActive; }
    bool isRetreating()   const { return retreating; }

    // Acceso a los drones para renderizar/interactuar desde fuera
    const std::array<KingDrone, 4>& getDrones() const { return drones; }

private:
    // Escudo
    bool  shieldActive;
    float shieldRecoverTimer;
    float shieldRecoverTime;     // Tiempo para recuperar el escudo

    // Retirada
    bool  retreating;
    float retreatTimer;
    float retreatDuration;

    // Bombas en cadena
    float chainBombCooldown;
    float chainBombCooldownMax;
    int   chainBombCount;        // Bombas por cadena

    // Drones
    std::array<KingDrone, 4> drones;
    float droneFireCycle;        // Intervalo entre ciclos de fuego de los drones
    float droneFireTimer;
    float droneFireDuration;     // Duración del estado de fuego de cada dron

    void updateDrones();
    void updateShield();
    void placeChainBombs();
    void onBombsExploded();      // Callback cuando sus bombas explotan
    void startRetreat();
};

#endif // KING_BOMBER_HPP
