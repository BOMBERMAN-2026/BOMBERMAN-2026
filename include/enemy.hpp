#ifndef ENEMY_HPP
#define ENEMY_HPP

#include "entity.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>

class GameMap;
class Player;
struct SpriteAtlas;

// Estado de vida del enemigo.
enum class EnemyLifeState {
    Alive, // IA/movimiento normal.
    Dying, // Reproduce animación "<base>.muerto.N".
    Dead   // Listo para eliminar de la lista.
};

// Direcciones posibles para el movimiento de enemigos.
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
    // Stats
    int hitPoints;          // Golpes necesarios para eliminarlo
    int maxHitPoints;       // HP máximos (para restaurar/referencia)
    int scoreValue;         // Puntos que otorga al ser destruido
    bool alive;             // Legacy: se conserva por compatibilidad (la fuente de verdad es `lifeState`)
    EnemyLifeState lifeState = EnemyLifeState::Alive; // Estado de vida
    bool canPassSoftBlocks; // Puede atravesar bloques destructibles
    bool isBoss;            // Es un jefe

    // Movimiento
    EnemyDirection facing;  // Dirección actual de movimiento
    float deltaTime;        // Tiempo entre frames (segundos)

    // Mundo (NO propiedad del enemigo)
    const GameMap* gameMap;                       // Referencia al mapa
    const std::vector<Player*>* playersList;      // Referencia a lista de jugadores

    // Animación
    float animTimer = 0.0f;                       // Acumulador para avanzar animación
    int animFrame = 0;                            // Frame actual
    std::string currentSpriteName;                // Sprite actual en el atlas
    float flipX = 0.0f;                           // 0.0 normal, 1.0 espejo horizontal

    // Animación de muerte (sprites "<base>.muerto.N")
    std::string spriteBaseId;                     // p.ej. "leon", "bebe", "babosa"...
    std::string deathSpritePrefix;                // "<base>.muerto."
    float deathTimer = 0.0f;                      // Acumulador para frames de muerte
    int deathFrame = 0;                           // Frame actual de muerte
    int deathFrameCount = 0;                      // Nº de frames (inferido del atlas)
    float deathFrameInterval = 0.08f;             // Segundos por frame de muerte

    Enemy(glm::vec2 pos, glm::vec2 size, float speed,
          int hp, int score, bool passSoftBlocks = false, bool boss = false);

    ~Enemy() override;

    // Interfaz Entity
    virtual void Update() = 0;
    virtual void Draw() = 0;

    virtual bool takeDamage(const SpriteAtlas& atlas, int amount = 1); // Devuelve true si pasa de Alive -> Dying.

    bool isAlive() const { return lifeState == EnemyLifeState::Alive; }
    bool isDying() const { return lifeState == EnemyLifeState::Dying; }
    bool isDead() const { return lifeState == EnemyLifeState::Dead; }

    virtual void startDying(const SpriteAtlas& atlas); // Fuerza estado Dying e inicializa contador de frames.

    virtual void updateDeath(float dt); // Avanza animación; al terminar marca Dead.

    // Vincula las referencias externas (llamar después de construir)
    void setContext(const GameMap* map, const std::vector<Player*>* players);

    // Actualiza el deltaTime (llamar cada frame antes de Update)
    void setDeltaTime(float dt);

    // Método virtual para reaccionar a una bomba cercana
    virtual void notifyBombNearby(glm::vec2 bombPos) {}

    // Excepción de diseño: algunos enemigos especiales pueden ignorar bombas.
    virtual bool canTraverseBombs() const { return false; }

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
