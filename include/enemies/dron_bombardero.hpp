#ifndef DRON_BOMBARDERO_HPP
#define DRON_BOMBARDERO_HPP

#include "enemy.hpp"

/*
 * Dron bombardero – 2000 pts, 3 HP
 * Persigue activamente a Bomberman pero mantiene las distancias.
 * Suelta bombas con explosión de 2 casillas cuando está al alcance.
 * Lanza bolas de fuego con rango infinito en una dirección.
 * Cada cierto tiempo se prende fuego durante 5 segundos (inmortal),
 * enviando una bola de fuego en cada dirección al prenderse.
 */
class DronBombardero : public Enemy {
public:
    DronBombardero(glm::vec2 pos, glm::vec2 size, float speed);
    ~DronBombardero() override;

    void Update() override;
    void Draw()   override;

    bool isOnFire() const { return onFire; }

private:
    // Distancias de combate
    float preferredDistance;   // Distancia ideal al jugador
    float bombDropRange;       // Rango para soltar bombas
    int   bombExplosionRange;  // Alcance de explosión de sus bombas (2)

    // Bolas de fuego
    float fireballCooldown;      // Cooldown restante
    float fireballCooldownMax;   // Intervalo entre disparos

    // Estado de fuego (inmunidad temporal)
    bool  onFire;
    float fireTimer;           // Tiempo restante prendido
    float fireDuration;        // Duración total del estado de fuego (5s)
    float fireCycleCooldown;   // Tiempo entre ciclos de fuego
    float fireCycleTimer;      // Timer para el próximo ciclo de fuego

    // Cooldown para colocar bombas
    float bombCooldown;          // Cooldown restante
    float bombCooldownMax;       // Intervalo entre bombas

    void enterFireMode();
    void updateFireMode();
    void dropBomb();
    void shootFireball(EnemyDirection dir);
    void shootFireballAllDirections();
};

#endif // DRON_BOMBARDERO_HPP
