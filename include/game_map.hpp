#ifndef GAME_MAP_HPP
#define GAME_MAP_HPP

#include <GL/glew.h>
#include <glm/glm.hpp>
#include "sprite_atlas.hpp"
#include "tile_animator.hpp"
#include "power_up.hpp"

#include <string>
#include <vector>

/*
 * game_map.hpp
 * ------------
 * Representación y render de un mapa en grid.
 *
 * - Carga un mapa desde `levels/*.txt` (IDs numéricos por celda).
 * - Usa un atlas JSON (SpriteAtlasStage*.json) para asociar ID -> tipo (walkable, destructible...).
 * - Expone utilidades de conversión Grid <-> NDC para movimiento/colisión.
 * - Soporta spawns definidos por nivel con directivas `spawn ...` en el TXT.
 *
 * Convención de coordenadas:
 * - Grid: (row, col) = (y, x), con (0,0) en la esquina superior-izquierda del TXT.
 * - `gridToNDC(row,col)` devuelve el centro de la celda.
 */

enum class BlockType {
    FLOOR,          // Suelo caminable
    BARRIER,        // Borde del mapa, nunca se destruye
    INDESTRUCTIBLE, // Muro fijo interior, bloquea pero no se rompe
    DESTRUCTIBLE    // Ladrillo que se puede destruir con bombas
};

struct Block {
    int spriteId;            // ID del sprite en el atlas (según el .txt)
    BlockType type;          // Tipo lógico del bloque
    bool destroyed  = false; // Solo relevante si type == DESTRUCTIBLE
    bool breaking   = false; // true cuando está reproduciendo la animación de rotura
    float breakTimer = 0.0f; // Acumulador de animación de rotura
    int breakFrame  = 0;     // Frame actual de rotura
    bool hasPowerUp = false; // Si al destruirse revela un power-up
    PowerUpType powerUpType = PowerUpType::ExtraLife; // Tipo de power-up escondido
    bool powerUpRevealed = false; // El power-up está visible pero no recogido
    bool powerUpCollected = false; // El power-up ya fue recogido

    bool isWalkable() const {
        if (destroyed) return true; // destructible ya destruido → suelo
        // Si está rompiéndose, TODAVÍA NO es caminable (foco de fuego)
        return (type == BlockType::FLOOR);
    }

    bool isDestructible() const {
        return (type == BlockType::DESTRUCTIBLE && !destroyed && !breaking);
    }
};

class GameMap {
public:
    GameMap();
    ~GameMap();

    // Carga el mapa desde un fichero con IDs numéricos separados por espacios.
    // También admite directivas tipo: `spawn white 8 5` (ver implementación).
    bool loadFromFile(const std::string& filePath);

    // Carga el atlas JSON (coordenadas de sprites en la sprite sheet + metadata type/walkable).
    bool loadAtlas(const std::string& jsonPath);

    // Renderiza todos los tiles usando la textura atlas.
    void render(GLuint vao, GLuint atlasTexture,
                GLuint uniformModel, GLuint uniformUvRect,
                GLuint uniformTintColor, GLuint uniformFlipX);

    void renderHud(GLuint vao, GLuint hudTexture,
                   GLuint uniformModel, GLuint uniformUvRect);

    int getRows() const { return rows; }
    int getCols() const { return cols; }

    int getSpriteId(int row, int col) const;

    // True si la celda permite caminar.
    bool isWalkable(int row, int col) const;

    // True si la celda es un bloque destructible.
    bool isDestructible(int row, int col) const;

    // Destruye una celda (si es destructible). Devuelve true si se destruyó.
    bool destroyTile(int row, int col);

    // Convierte posición NDC a celda (row, col) del grid.
    void ndcToGrid(glm::vec2 ndc, int& row, int& col) const;

    bool canMoveTo(glm::vec2 center, float halfSize) const; // True si un AABB no solapa ningún tile no caminable.

    glm::vec2 getSpawnPosition(int playerIndex) const; // Usa spawn del nivel o fallback automático.

    // Convierte una celda (row,col) al centro en NDC.
    glm::vec2 gridToNDC(int row, int col) const;

    // Modifica el estado interno de la animación de tiles del mapa (si la hay).
    void update(float deltaTime);

    float getTileSize() const { return tileSize; }

    // Calcula tamaño de tile y offsets para centrar el mapa en pantalla.
    void calculateTileMetrics(float aspectRatio);

    // === Power-Ups ===
    // Coloca power-ups aleatoriamente debajo de bloques destructibles.
    // stackeables (BombUp, FireUp, SpeedUp): 2 de cada uno
    // no stackeables (ExtraLife, Invincibility, RemoteControl): 1 de cada uno
    void placePowerUps();

    // Renderiza los power-ups revelados (bloques destruidos con power-up visible).
    void renderPowerUps(GLuint vao, GLuint uniformModel, GLuint uniformUvRect,
                        GLuint uniformTintColor, GLuint uniformFlipX);

    // Carga las texturas de los power-ups.
    void loadPowerUpTextures();

    // Comprueba si un jugador está sobre un power-up revelado y lo recoge.
    // Devuelve true si recogió un power-up (y lo aplica al player).
    bool tryCollectPowerUp(int row, int col, class Player* player);

private:
    struct SpawnCell {
        int row = -1; // Fila en grid
        int col = -1; // Columna en grid
    };

    // Grid
    std::vector<std::vector<Block>> grid; // Tiles del mapa
    int rows = 0;                         // Nº filas
    int cols = 0;                         // Nº columnas

    // Spawns (opcional): si no existe/vale, se usa fallback automático.
    std::vector<SpawnCell> spawnCells;    // Spawns por índice de jugador

    // Layout
    float tileSize = 0.0f;                // Tamaño de tile en NDC
    float offsetX  = 0.0f;                // Offset para centrar mapa
    float offsetY  = 0.0f;                // Offset para centrar mapa
    float currentAspectRatio = 1.0f;      // Aspect ratio usado en calculateTileMetrics

    // Atlas
    SpriteAtlas atlas;                    // Atlas del stage
    bool atlasLoaded = false;             // true si se cargó el atlas

    // Hud
    float hudTopSpace = 0.25f; // espacio para los marcadores y otras metricas
    
    // Animación
    TileAnimator animator;                // Swap de IDs animados

    int destroyedFloorId = 10;            // Sprite al destruir (por defecto; se recalcula dinámico en render)

    // Convierte el string "type" del atlas JSON a BlockType
    static BlockType blockTypeFromString(const std::string& typeStr);

    // === Power-Ups ===
    static constexpr int POWER_UP_TYPE_COUNT = 6;
    GLuint powerUpTextures[POWER_UP_TYPE_COUNT] = {0}; // Texturas indexadas por PowerUpType
    bool powerUpTexturesLoaded = false;
};

#endif // GAME_MAP_HPP
