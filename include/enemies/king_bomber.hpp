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
    glm::vec2 position; // Posición en NDC
    glm::vec2 direction; // Dirección de movimiento
    float speed; // Velocidad
    bool onFire; // true si está en modo fuego
    float fireTimer; // Tiempo restante en modo fuego
    bool alive; // true si existe (para render/IA)

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
    bool  shieldActive;          // true si el escudo está activo
    float shieldRecoverTimer;    // Tiempo acumulado para recuperar
    float shieldRecoverTime;     // Tiempo para recuperar el escudo

    // Retirada
    bool  retreating;            // true si está retirándose
    float retreatTimer;          // Tiempo acumulado en retirada
    float retreatDuration;       // Duración total de la retirada

    // Bombas en cadena
    float chainBombCooldown;     // Cooldown restante
    float chainBombCooldownMax;  // Intervalo entre bombas
    int   chainBombCount;        // Bombas por cadena

    // Drones
    std::array<KingDrone, 4> drones;
    float droneFireCycle;        // Intervalo entre ciclos de fuego de los drones
    float droneFireTimer;        // Timer del ciclo
    float droneFireDuration;     // Duración del estado de fuego de cada dron

    void updateDrones();
    void updateShield();
    void placeChainBombs();
    void onBombsExploded();      // Callback cuando sus bombas explotan
    void startRetreat();
};

#endif // KING_BOMBER_HPP
