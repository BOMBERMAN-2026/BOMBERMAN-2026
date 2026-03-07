#include "game_map.hpp"
#include "sprite_atlas.hpp" // for resolveAssetPath, loadSpriteAtlasMinimal, getUvRectForSprite

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>

GameMap::GameMap() {}

GameMap::~GameMap() {}

BlockType GameMap::blockTypeFromString(const std::string& typeStr) {
    if (typeStr == "barrier")        return BlockType::BARRIER;
    if (typeStr == "indestructible") return BlockType::INDESTRUCTIBLE;
    if (typeStr == "destructible")   return BlockType::DESTRUCTIBLE;
    return BlockType::FLOOR; // default
}

bool GameMap::loadFromFile(const std::string& filePath) {
    std::string resolved = resolveAssetPath(filePath);
    std::ifstream file(resolved);
    if (!file.is_open()) {
        std::cerr << "GameMap: no se pudo abrir " << resolved << std::endl;
        return false;
    }

    grid.clear();
    std::string line;
    int maxCols = 0;

    while (std::getline(file, line)) {
        if (line.empty()) continue;
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        std::vector<Block> row;
        std::istringstream iss(line);
        int id;
        while (iss >> id) {
            Block b;
            b.spriteId  = id;
            b.type      = BlockType::FLOOR; // se asignara bien tras loadAtlas
            b.destroyed = false;
            b.hasPowerUp = false;
            row.push_back(b);
        }
        if ((int)row.size() > maxCols)
            maxCols = (int)row.size();
        if (!row.empty())
            grid.push_back(row);
    }

    rows = (int)grid.size();
    cols = maxCols;

    // Rellenar filas cortas con suelo
    for (auto& row : grid) {
        while ((int)row.size() < cols) {
            Block b;
            b.spriteId  = destroyedFloorId;
            b.type      = BlockType::FLOOR;
            b.destroyed = false;
            b.hasPowerUp = false;
            row.push_back(b);
        }
    }

    if (rows == 0 || cols == 0) {
        std::cerr << "GameMap: mapa vacio" << std::endl;
        return false;
    }

    calculateTileMetrics();

    std::cout << "GameMap: cargado " << cols << "x" << rows
              << " (tileSize=" << tileSize << ")" << std::endl;
    return true;
}

bool GameMap::loadAtlas(const std::string& jsonPath) {
    std::string resolved = resolveAssetPath(jsonPath);
    if (!loadSpriteAtlasMinimal(resolved, atlas)) {
        std::cerr << "GameMap: error cargando atlas " << resolved << std::endl;
        return false;
    }
    atlasLoaded = true;
    
    // Iniciar sistema de animacion de tiles (si el atlas lo define)
    animator.setup(atlas);

    // Ahora que tenemos el atlas, asignar BlockType a cada celda del grid
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            Block& b = grid[r][c];
            auto it = atlas.sprites.find(std::to_string(b.spriteId));
            if (it != atlas.sprites.end()) {
                b.type = blockTypeFromString(it->second.type);
            }
        }
    }

    std::cout << "GameMap: atlas cargado con " << atlas.sprites.size()
              << " sprites" << std::endl;
    return true;
}

void GameMap::update(float deltaTime) {
    animator.update(deltaTime);
}

void GameMap::calculateTileMetrics() {
    float sizeByWidth  = 2.0f / (float)cols;
    float sizeByHeight = 2.0f / (float)rows;
    tileSize = std::min(sizeByWidth, sizeByHeight);

    float mapWidth  = cols * tileSize;
    float mapHeight = rows * tileSize;
    offsetX = (2.0f - mapWidth)  / 2.0f;
    offsetY = (2.0f - mapHeight) / 2.0f;
}

glm::vec2 GameMap::gridToNDC(int row, int col) const {
    float x = -1.0f + offsetX + (col + 0.5f) * tileSize;
    float y =  1.0f - offsetY - (row + 0.5f) * tileSize;
    return glm::vec2(x, y);
}

void GameMap::ndcToGrid(glm::vec2 ndc, int& row, int& col) const {
    col = (int)std::floor((ndc.x + 1.0f - offsetX) / tileSize);
    row = (int)std::floor((1.0f - offsetY - ndc.y) / tileSize);
}

bool GameMap::canMoveTo(glm::vec2 center, float halfSize) const {
    // Hitbox asimétrico: ancho suficiente para detectar muros laterales,
    // pero bajo en Y para que las esquinas no entren en la fila de arriba/abajo
    // cuando el jugador está ligeramente descentrado verticalmente.
    // Las sondas explícitas de player.cpp cubren los bordes visuales sup/inf.
    const float mX = halfSize * 0.72f;
    const float mY = halfSize * 0.45f;

    glm::vec2 probes[] = {
        {center.x - mX, center.y - mY},
        {center.x + mX, center.y - mY},
        {center.x - mX, center.y + mY},
        {center.x + mX, center.y + mY}
    };

    for (const auto& c : probes) {
        int r, cl;
        ndcToGrid(c, r, cl);
        if (!isWalkable(r, cl)) return false;
    }
    return true;
}

int GameMap::getSpriteId(int row, int col) const {
    if (row < 0 || row >= rows || col < 0 || col >= cols)
        return 0; // fuera de limites = borde
        
    int rawId = grid[row][col].spriteId;
    return animator.getDisplayId(rawId);
}

bool GameMap::isWalkable(int row, int col) const {
    if (row < 0 || row >= rows || col < 0 || col >= cols)
        return false;
    return grid[row][col].isWalkable();
}

bool GameMap::destroyTile(int row, int col) {
    if (row < 0 || row >= rows || col < 0 || col >= cols)
        return false;
    Block& b = grid[row][col];
    if (!b.isDestructible())
        return false;
    b.destroyed = true;
    // Si tenia un power-up, el sistema de juego deberia spawnearlo aqui
    return true;
}

glm::vec2 GameMap::getSpawnPosition(int playerIndex) const {
    // Busca la primera celda walkable desde la esquina correspondiente
    if (playerIndex == 0) {
        for (int r = 1; r < rows - 1; r++)
            for (int c = 1; c < cols - 1; c++)
                if (isWalkable(r, c)) return gridToNDC(r, c);
    } else {
        for (int r = rows - 2; r > 0; r--)
            for (int c = cols - 2; c > 0; c--)
                if (isWalkable(r, c)) return gridToNDC(r, c);
    }
    return gridToNDC(rows / 2, cols / 2);
}

void GameMap::render(GLuint vao, GLuint atlasTexture,
                     GLuint uniformModel, GLuint uniformUvRect,
                     GLuint uniformTintColor, GLuint uniformFlipX) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlasTexture);

    // Sin flip ni tint para los tiles del mapa
    glUniform1f(uniformFlipX, 0.0f);
    glm::vec4 tint(1.0f, 1.0f, 1.0f, 1.0f);
    glUniform4fv(uniformTintColor, 1, glm::value_ptr(tint));

    glBindVertexArray(vao);

    // Escala exacta del tile para evitar que un tile pise la ultima fila del de arriba.
    float scale = tileSize / 2.0f;

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            const Block& block = grid[r][c];

            // Si el bloque fue destruido, pintar suelo
            int originalId = block.spriteId;
            int displayId = animator.getDisplayId(originalId);
            
            if (block.destroyed)
                displayId = destroyedFloorId;

            // Obtener UV rect del atlas
            glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
            float scaleX = scale;
            float scaleY = scale;

            if (atlasLoaded) {
                std::string idStr = std::to_string(displayId);
                getUvRectForSprite(atlas, idStr, uvRect);

                // Ajustar escala segun el tamano real del sprite en el atlas
                auto it = atlas.sprites.find(idStr);
                if (it != atlas.sprites.end()) {
                    const SpriteFrame& frame = it->second;
                    // Usar la dimension mayor como referencia (normalmente 48)
                    float maxDim = (float)std::max(frame.w, frame.h);
                    scaleX = ((float)frame.w / maxDim) * scale;
                    scaleY = ((float)frame.h / maxDim) * scale;
                }
            }

            glm::vec2 center = gridToNDC(r, c);

            // Ajustar posicion segun el campo "align" del atlas
            if (atlasLoaded) {
                std::string idStr = std::to_string(displayId);
                auto it = atlas.sprites.find(idStr);
                if (it != atlas.sprites.end()) {
                    const std::string& align = it->second.align;
                    if (align == "right") {
                        center.x += (scale - scaleX);
                    } else if (align == "left") {
                        center.x -= (scale - scaleX);
                    }
                }
            }

            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(center, 0.0f));
            model = glm::scale(model, glm::vec3(scaleX, scaleY, 1.0f));

            glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
            glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        }
    }

    glBindVertexArray(0);
}
