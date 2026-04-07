#ifndef DRAGON_JOVEN_HPP
#define DRAGON_JOVEN_HPP

#include "enemy.hpp"
#include "bomb.hpp" // Para ExplosionSegment

/*
 * Dragón joven – 400 pts, 1 HP
 * Patrulla de forma básica. Escupe fuego hacia Bomberman
 * cuando lo ve en línea recta, alcanzando 2 casillas al frente.
 */
class DragonJoven : public Enemy {
public:
    DragonJoven(glm::vec2 pos, glm::vec2 size, float speed);
    ~DragonJoven() override;

    void Update() override;
    void Draw()   override;

private:
    // IA
    float fireCooldown;      // Tiempo restante hasta poder escupir fuego otra vez
    float fireCooldownMax;   // Intervalo entre ataques de fuego
    int   fireRange;         // Alcance en casillas (2)
    bool  isCharging;        // Está parpadeando antes de escupir fuego
    float chargeTimer;       // Duración del parpadeo
    bool  isFiring;          // Está en animación de escupir fuego
    float fireAnimTimer;     // Duración de la animación de fuego
    int   fireAnimFrame;     // Frame actual de la explosión (0..3)

    std::vector<ExplosionSegment> fireSegments; // Tramos de fuego a dibujar

    // Comprueba si el jugador está alineado en línea recta (misma fila o columna)
    // y dentro del rango. Devuelve la dirección del disparo o NONE.
    EnemyDirection checkLineOfSight() const;
};

#endif // DRAGON_JOVEN_HPP
