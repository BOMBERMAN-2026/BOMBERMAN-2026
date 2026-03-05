#ifndef GAME_MAP_HPP
#define GAME_MAP_HPP

#include <GL/glew.h>
#include <glm/glm.hpp>
#include "sprite_atlas.hpp"

#include <string>
#include <vector>

struct Tile {
    int spriteId;
    bool destroyed; // for destructible tiles that have been blown up
};

class GameMap {
public:
    GameMap();
    ~GameMap();

    // Carga el mapa desde un fichero con IDs numericos separados por espacios
    bool loadFromFile(const std::string& filePath);

    // Carga el atlas JSON (coordenadas de sprites en la sprite sheet)
    bool loadAtlas(const std::string& jsonPath);

    // Renderiza todos los tiles usando la textura atlas
    void render(GLuint vao, GLuint atlasTexture,
                GLuint uniformModel, GLuint uniformUvRect,
                GLuint uniformTintColor, GLuint uniformFlipX);

    int getRows() const { return rows; }
    int getCols() const { return cols; }

    int getSpriteId(int row, int col) const;
    bool isWalkable(int row, int col) const;
    bool destroyTile(int row, int col);

    // Convierte posición NDC a celda (row, col) del grid
    void ndcToGrid(glm::vec2 ndc, int& row, int& col) const;

    // Devuelve true si el rectángulo centrado en 'center' con semilado 'halfSize'
    // no solapa ningún tile no caminable
    bool canMoveTo(glm::vec2 center, float halfSize) const;

    glm::vec2 getSpawnPosition(int playerIndex) const;
    glm::vec2 gridToNDC(int row, int col) const;

    float getTileSize() const { return tileSize; }

private:
    std::vector<std::vector<Tile>> grid;
    int rows = 0;
    int cols = 0;

    float tileSize = 0.0f;
    float offsetX  = 0.0f;
    float offsetY  = 0.0f;

    SpriteAtlas atlas;
    bool atlasLoaded = false;

    int destroyedFloorId = 5; // sprite a mostrar cuando se destruye

    void calculateTileMetrics();
};

#endif // GAME_MAP_HPP
