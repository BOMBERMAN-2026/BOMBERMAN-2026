#ifndef ENEMY_HPP
#define ENEMY_HPP

#include "entity.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>

class GameMap;
class Player;

/*
 * Enumeración con las direcciones posibles para el movimiento de enemigos.
 */
enum class EnemyDirection {
    UP,
    DOWN,
    LEFT,
    RIGHT,
    NONE
};

/*
 * Clase base para todos los enemigos del juego.
 * Hereda de Entity y añade atributos comunes a todos los enemigos:
 *   - Puntos de vida, puntuación, IA básica, referencia al mapa y al jugador.
 */
class Enemy : public Entity {
public:
    int hitPoints;          // Golpes necesarios para eliminarlo
    int maxHitPoints;       // HP máximos (para restaurar/referencia)
    int scoreValue;         // Puntos que otorga al ser destruido
    bool alive;             // Si el enemigo sigue vivo
    bool canPassSoftBlocks; // Puede atravesar bloques destructibles
    bool isBoss;            // Es un jefe

    EnemyDirection facing;  // Dirección actual de movimiento

    // Referencias al mundo (NO son propiedad del enemigo, no se hacen delete)
    const GameMap* gameMap;
    const std::vector<Player*>* playersList; // Puntero a la lista de jugadores

    float deltaTime;        // Tiempo entre frames, actualizado externamente

    // Campos para animación
    float animTimer = 0.0f;
    int animFrame = 0;
    std::string currentSpriteName;
    float flipX = 0.0f;

    Enemy(glm::vec2 pos, glm::vec2 size, float speed,
          int hp, int score, bool passSoftBlocks = false, bool boss = false);

    ~Enemy() override;

    // Interfaz Entity
    void Update() override = 0;
    void Draw()   override = 0;

    // Recibe daño; devuelve true si muere
    bool takeDamage(int amount = 1);

    // Vincula las referencias externas (llamar después de construir)
    void setContext(const GameMap* map, const std::vector<Player*>* players);

    // Actualiza el deltaTime (llamar cada frame antes de Update)
    void setDeltaTime(float dt);

protected:
    // ── Utilidades de IA que pueden usar las subclases ──
    glm::vec2 getClosestPlayerPos(float& out_dist) const;

    // Distancia Manhattan al jugador (en coordenadas NDC)
    float distanceToPlayer() const;

    // Dirección general hacia el jugador (eje dominante)
    EnemyDirection directionTowardPlayer() const;

    // Dirección opuesta
    static EnemyDirection oppositeDirection(EnemyDirection dir);

    // Convierte EnemyDirection a un vector unitario
    static glm::vec2 dirToVec(EnemyDirection dir);

    // Elige una dirección aleatoria entre las 4 cardinales
    static EnemyDirection randomDirection();

    // Intenta moverse en la dirección dada; devuelve true si pudo avanzar
    bool tryMove(EnemyDirection dir, float stepSize);

    // Comprueba si la casilla en esa dirección es transitable
    bool canMoveInDirection(EnemyDirection dir, float lookAhead) const;
};

#endif // ENEMY_HPP
