#ifndef BOMB_HPP
#define BOMB_HPP

#include <glm/glm.hpp>
#include <string>
#include <vector>

class GameMap;
class Player;

/*
 * bomb.hpp
 * --------
 * Bomba que el jugador puede colocar en el mapa.
 *
 * - Controles de colocación (actual):
 *   - Jugador 1 (flechas): Ctrl derecho.
 *   - Jugador 2 (WASD): X.
 * - Tiene una animación de 3 frames mientras la mecha está activa.
 * - Tras un temporizador (fuseTime), explota y muestra 4 frames de explosión muy rápidos.
 * - Al terminar la explosión, se elimina.
 */

// Estado de la bomba.
enum class BombState {
    FUSE,      // Mecha activa (animación con bomb.0-1-2).
    EXPLODING, // Explosión en curso.
    DONE       // Explosión terminada, lista para eliminar.
};

struct ExplosionSegment {
    glm::vec2 pos;          // Centro del tile en NDC
    std::string baseName;   // "explosion", "explosion_mid", "explosion_end"
    float rotation;         // Ángulo en radianes (0 para derecha, 90 arriba, etc.)
};

class Bomb {
public:
    glm::vec2 position;     // Centro del tile en NDC
    int gridRow;            // Fila del tile
    int gridCol;            // Columna del tile

    float fuseTime;         // Tiempo total antes de explotar (segundos)
    float fuseTimer;        // Temporizador acumulado de mecha

    BombState state;        // Estado actual de la bomba

    // Animación
    float animTimer;        // Acumulador para cambiar de frame
    int   animFrame;        // Frame actual
    int   animStep;         // Paso actual en la secuencia de mecha (0-7)
    float animInterval;     // Tiempo entre frames (mecha)
    float explodeInterval;  // Tiempo entre frames de explosión
    std::string currentSpriteName; // Sprite actual en el atlas

    int ownerIndex;         // Índice del jugador que la colocó (0 o 1)

    // Bloqueo de paso (tile-based).
    bool ownerLeftTile; // true cuando el dueño ya abandonó el tile (si es true, bloquea también para él).

    bool blocksForEnemy() const { return state != BombState::DONE; } // Enemigos: bloquea mientras exista.
    bool blocksForPlayer(int playerId) const; // Jugadores: el dueño puede salir una vez; luego bloquea.

    int power;              // Longitud total de la explosión (ej. 3 -> centro + mid + end)
    std::vector<ExplosionSegment> explosionSegments; // Segmentos calculados al inicio de la explosión

    Player* owner;           // Puntero al jugador dueño (para decrementar activeBombs al explotar)
    bool remoteControlled;   // Si true, la mecha no avanza sola (se detona manualmente)

    Bomb(glm::vec2 pos, int row, int col, Player* owner, int power = 2, bool remote = false);
    ~Bomb();

    bool Update(float deltaTime); // Devuelve true cuando termina (lista para eliminar).
    void detonate();              // Fuerza explosión inmediata (Remote Control).

    // Dibuja la bomba/explosión usando la textura del mapa (sprites-Stage1).
    void Draw();
};

// Lista global de bombas activas (definida en bomberman.cpp)
extern std::vector<Bomb*> gBombs;

#endif // BOMB_HPP
