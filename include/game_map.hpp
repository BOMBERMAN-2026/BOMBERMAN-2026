#ifndef GAME_MAP_HPP
#define GAME_MAP_HPP

#include <GL/glew.h>
#include <glm/glm.hpp>
#include "sprite_atlas.hpp"
#include "tile_animator.hpp"

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
    int spriteId;
    BlockType type;
    bool destroyed  = false; // Solo relevante si type == DESTRUCTIBLE
    bool hasPowerUp = false; // Si al destruirse revela un power-up

    bool isWalkable() const {
        if (destroyed) return true; // destructible ya destruido → suelo
        return (type == BlockType::FLOOR);
    }

    bool isDestructible() const {
        return (type == BlockType::DESTRUCTIBLE && !destroyed);
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

    int getRows() const { return rows; }
    int getCols() const { return cols; }

    int getSpriteId(int row, int col) const;

    // True si la celda permite caminar.
    bool isWalkable(int row, int col) const;

    // Destruye una celda (si es destructible). Devuelve true si se destruyó.
    bool destroyTile(int row, int col);

    // Convierte posición NDC a celda (row, col) del grid.
    void ndcToGrid(glm::vec2 ndc, int& row, int& col) const;

    // Devuelve true si un AABB centrado en `center` con semilado `halfSize`
    // no solapa ningún tile no caminable.
    bool canMoveTo(glm::vec2 center, float halfSize) const;

    // Spawn de jugador por índice.
    // - Si el nivel define `spawn` para ese índice, se usa (si es walkable).
    // - Si no, cae a un spawn automático (primera casilla walkable desde una esquina).
    glm::vec2 getSpawnPosition(int playerIndex) const;

    // Convierte una celda (row,col) al centro en NDC.
    glm::vec2 gridToNDC(int row, int col) const;

    // Modifica el estado interno de la animación de tiles del mapa (si la hay).
    void update(float deltaTime);

    float getTileSize() const { return tileSize; }

    // Calcula tamaño de tile y offsets para centrar el mapa en pantalla.
    void calculateTileMetrics(float aspectRatio);

private:
    struct SpawnCell {
        int row = -1;
        int col = -1;
    };

    std::vector<std::vector<Block>> grid;
    int rows = 0;
    int cols = 0;

    // Spawns opcionales definidos por nivel (por índice de jugador).
    // Si no existe o es inválido, se usa el fallback de buscar primera casilla walkable.
    std::vector<SpawnCell> spawnCells;

    float tileSize = 0.0f;
    float offsetX  = 0.0f;
    float offsetY  = 0.0f;
    float currentAspectRatio = 1.0f;

    SpriteAtlas atlas;
    bool atlasLoaded = false;
    
    TileAnimator animator;

    int destroyedFloorId = 5; // sprite a mostrar cuando se destruye

    // Convierte el string "type" del atlas JSON a BlockType
    static BlockType blockTypeFromString(const std::string& typeStr);
};

#endif // GAME_MAP_HPP
