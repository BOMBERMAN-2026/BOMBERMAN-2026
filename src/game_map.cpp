#include "game_map.hpp"
#include "sprite_atlas.hpp" // for resolveAssetPath, loadSpriteAtlasMinimal, getUvRectForSprite

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>

/*
 * game_map.cpp
 * -----------
 * Implementación de `GameMap`.
 *
 * Qué hace este módulo:
 * - Carga el layout del nivel desde `levels/*.txt`.
 * - Carga el atlas de stage (JSON) y traduce `spriteId` -> propiedades (walkable/tipo/align).
 * - Calcula métricas de tile para encajar el mapa en ortográfica (-aspect..aspect, -1..1).
 * - Provee utilidades: conversiones grid<->NDC, colisión simple y spawns por jugador.
 *
 * Coordenadas de grid:
 * - Se usan como (row, col) internamente en la mayoría de funciones.
 * - En directivas `spawn`, el fichero habla en (x,y) = (col,row) con (0,0) arriba-izquierda.
 */

// ============================== Ctor / dtor ==============================

GameMap::GameMap() {}

GameMap::~GameMap() {}

// ============================== Carga / atlas ==============================

BlockType GameMap::blockTypeFromString(const std::string& typeStr) {
    if (typeStr == "barrier")        return BlockType::BARRIER;
    if (typeStr == "indestructible") return BlockType::INDESTRUCTIBLE;
    if (typeStr == "destructible")   return BlockType::DESTRUCTIBLE;
    return BlockType::FLOOR; // default
}

// Carga el grid del nivel y parsea directivas opcionales (p.ej. `spawn`).
bool GameMap::loadFromFile(const std::string& filePath) {
    std::string resolved = resolveAssetPath(filePath);
    std::ifstream file(resolved);
    if (!file.is_open()) {
        std::cerr << "GameMap: no se pudo abrir " << resolved << std::endl;
        return false;
    }

    grid.clear();
    spawnCells.clear();

    struct PendingSpawn {
        int index = -1;
        int x = 0;      // columna
        int y = 0;      // fila
        bool inner = true; // si true: coordenadas en el área jugable (sin borde) => +1
    };
    std::vector<PendingSpawn> pendingSpawns;

    std::string line;
    int maxCols = 0;

    while (std::getline(file, line)) {
        if (line.empty()) continue;
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        // Trim simple de espacios iniciales
        size_t firstNonSpace = line.find_first_not_of(" \t");
        if (firstNonSpace == std::string::npos) continue;
        std::string trimmed = line.substr(firstNonSpace);

        // Comentarios
        if (!trimmed.empty() && trimmed[0] == '#') continue;

        // Directivas: spawn <white|red|blanco|rojo|index> <x> <y>
        // - "spawn" usa coordenadas en el área interior (sin borde), ya en coordenadas de grid:
        //   (x,y) => (col=x,row=y), típicamente con x∈[1..cols-2], y∈[1..rows-2]
        // - "spawn_abs" usa coordenadas absolutas del grid: (x,y) => (col=x,row=y)
        {
            std::istringstream issDir(trimmed);
            std::string kw;
            issDir >> kw;
            if (kw == "spawn" || kw == "spawn_abs") {
                std::string who;
                int x = 0, y = 0;
                if (!(issDir >> who >> x >> y)) {
                    std::cerr << "GameMap: directiva spawn inválida: '" << trimmed << "'\n";
                    continue;
                }

                int idx = -1;
                // who puede ser nombre o índice
                if (who == "white" || who == "blanco") idx = 0;
                else if (who == "red" || who == "rojo") idx = 1;
                else {
                    // intentar parsear índice
                    bool allDigits = !who.empty() && std::all_of(who.begin(), who.end(), [](unsigned char ch){ return std::isdigit(ch) != 0; });
                    if (allDigits) idx = std::atoi(who.c_str());
                }

                if (idx < 0) {
                    std::cerr << "GameMap: spawn: jugador desconocido '" << who << "'\n";
                    continue;
                }

                PendingSpawn ps;
                ps.index = idx;
                ps.x = x;
                ps.y = y;
                ps.inner = (kw == "spawn");
                pendingSpawns.push_back(ps);
                continue;
            }
        }

        std::vector<Block> row;
        std::istringstream iss(trimmed);
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

    // Resolver spawns definidos (si los hay)
    if (!pendingSpawns.empty()) {
        int maxIndex = -1;
        for (const auto& ps : pendingSpawns) maxIndex = std::max(maxIndex, ps.index);
        spawnCells.assign(maxIndex + 1, SpawnCell{});

        for (const auto& ps : pendingSpawns) {
            const int col = ps.x;
            const int row = ps.y;

            if (row < 0 || row >= rows || col < 0 || col >= cols) {
                std::cerr << "GameMap: spawn fuera de rango para jugador " << ps.index
                          << " (x=" << ps.x << ", y=" << ps.y << ", inner=" << (ps.inner ? "true" : "false") << ")\n";
                continue;
            }
            if (ps.index >= 0 && ps.index < (int)spawnCells.size()) {
                spawnCells[ps.index].row = row;
                spawnCells[ps.index].col = col;
            }
        }
    }

    std::cout << "GameMap: cargado " << cols << "x" << rows << std::endl;
    return true;
}

// Carga el atlas del stage (JSON) y asigna `BlockType` por celda según metadata del sprite.
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

// Avanza animaciones de tiles (si el atlas define animaciones).
void GameMap::update(float deltaTime) {
    animator.update(deltaTime);

    // Actualizar bloques que se están rompiendo
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            Block& b = grid[r][c];
            if (b.breaking) {
                b.breakTimer += deltaTime;
                if (b.breakTimer >= 0.08f) { // misma velocidad rápida que la explosión
                    b.breakTimer -= 0.08f;
                    b.breakFrame++;
                    if (b.breakFrame >= 4) {
                        b.breaking = false;
                        b.destroyed = true; // finalmente se destruye y pasa a ser suelo 100% transitable
                    }
                }
            }
        }
    }
}

// Recalcula tileSize/offsets para el aspect ratio actual y centra el mapa en pantalla.
void GameMap::calculateTileMetrics(float aspectRatio) {
    currentAspectRatio = aspectRatio;
    // La pantalla OpenGL ortográfica va desde -aspectRatio hasta aspectRatio en X
    // y de -1.0 a 1.0 en Y.
    float screenWidthInOrtho = 2.0f * aspectRatio;
    float screenHeightInOrtho = 2.0f;

    float availableHeight = screenHeightInOrtho - hudTopSpace;
    float sizeByWidth  = screenWidthInOrtho / (float)cols;
    float sizeByHeight = availableHeight / (float)rows;
    // Escoger el mínimo para que los tiles sean siempre cuadrados perfectos y entren completos
    tileSize = std::min(sizeByWidth, sizeByHeight);

    float mapWidth  = cols * tileSize;
    float mapHeight = rows * tileSize;
    
    // El mapa siempre lo centramos en medio de la pantalla ortográfica
    offsetX = (screenWidthInOrtho - mapWidth)  / 2.0f;
    offsetY = hudTopSpace + (availableHeight - mapHeight) / 2.0f;
}

// ============================== Coordenadas: Grid <-> NDC ==============================

glm::vec2 GameMap::gridToNDC(int row, int col) const {
    // x va desde -screenWidthInOrtho/2 hasta +screenWidthInOrtho/2
    // Sabiendo que offsetX ya calcula el margen desde -screenWidthInOrtho/2...
    // Necesitamos ajustar la base. Mejor recalculamos usando el tamaño de pantalla ortográfico
    // pero lo más fácil es que gridToNDC empiece en el punto superior izquierdo:
    
    // Asumimos un espacio ortográfico donde Left = -aspectRatio y Top = 1.0f
    float left = -currentAspectRatio;
    float top = 1.0f;
    
    float x = left + offsetX + (col + 0.5f) * tileSize;
    float y = top - offsetY - (row + 0.5f) * tileSize;
    return glm::vec2(x, y);
}

// Convierte una posición NDC en una celda (row,col) usando tileSize y offsets actuales.
void GameMap::ndcToGrid(glm::vec2 ndc, int& row, int& col) const {
    float left = -currentAspectRatio;
    float top = 1.0f;
    
    col = (int)std::floor((ndc.x - left - offsetX) / tileSize);
    row = (int)std::floor((top - offsetY - ndc.y) / tileSize);
}

// Comprueba si una AABB centrada en `center` (NDC) cabe en celdas walkable del grid.
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

// ============================== Consultas del grid / colisión ==============================

int GameMap::getSpriteId(int row, int col) const {
    if (row < 0 || row >= rows || col < 0 || col >= cols)
        return 0; // fuera de limites = borde
        
    int rawId = grid[row][col].spriteId;
    return animator.getDisplayId(rawId);
}

BlockType GameMap::getBlockType(int row, int col) const {
    if (row < 0 || row >= rows || col < 0 || col >= cols) {
        return BlockType::BARRIER;
    }
    return grid[row][col].type;
}

bool GameMap::getUvRectForTile(int row, int col, glm::vec4& uvRect) const {
    uvRect = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
    if (!atlasLoaded) return false;
    if (row < 0 || row >= rows || col < 0 || col >= cols) return false;

    const Block& block = grid[row][col];
    int displayId = animator.getDisplayId(block.spriteId);

    if (block.destroyed || block.breaking) {
        const bool wallLeft = (col > 0 && grid[row][col - 1].type == BlockType::BARRIER);
        const bool wallUp = (row > 0 && grid[row - 1][col].type == BlockType::BARRIER);
        const bool indestLeft = (col > 0 && grid[row][col - 1].type == BlockType::INDESTRUCTIBLE);
        const bool indestUp = (row > 0 && grid[row - 1][col].type == BlockType::INDESTRUCTIBLE);

        int floorId = 10;
        if (wallLeft && wallUp) floorId = 6;
        else if (wallLeft) floorId = 9;
        else if (wallUp) floorId = 7;
        else if (indestUp && indestLeft) floorId = 9;
        else if (indestUp) floorId = 11;
        else if (indestLeft) floorId = 9;

        displayId = floorId;
    }

    const std::string idStr = std::to_string(displayId);
    return getUvRectForSprite(atlas, idStr, uvRect);
}

bool GameMap::getUvRectForSpriteId(int spriteId, glm::vec4& uvRect) const {
    uvRect = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
    if (!atlasLoaded) return false;
    const std::string idStr = std::to_string(spriteId);
    return getUvRectForSprite(atlas, idStr, uvRect);
}

// Walkable lógico (según `BlockType` + flags del bloque).
bool GameMap::isWalkable(int row, int col) const {
    if (row < 0 || row >= rows || col < 0 || col >= cols)
        return false;
    return grid[row][col].isWalkable();
}

bool GameMap::isDestructible(int row, int col) const {
    if (row < 0 || row >= rows || col < 0 || col >= cols)
        return false;
    return grid[row][col].isDestructible();
}

// Marca un bloque destructible como roto (inicia la animación de destruirse).
bool GameMap::destroyTile(int row, int col) {
    if (row < 0 || row >= rows || col < 0 || col >= cols)
        return false;
    Block& b = grid[row][col];
    if (!b.isDestructible())
        return false;
    // Iniciamos la animación
    b.breaking = true;
    b.breakTimer = 0.0f;
    b.breakFrame = 0;
    // Si tenia un power-up, el sistema de juego deberia spawnearlo aqui cuando acabe
    return true;
}

// ============================== Spawn de jugadores ==============================
glm::vec2 GameMap::getSpawnPosition(int playerIndex) const {
    // 1) Si el nivel define un spawn para este índice, usarlo (si es válido y walkable)
    if (playerIndex >= 0 && playerIndex < (int)spawnCells.size()) {
        const SpawnCell& sc = spawnCells[playerIndex];
        if (sc.row >= 0 && sc.col >= 0) {
            if (isWalkable(sc.row, sc.col)) {
                return gridToNDC(sc.row, sc.col);
            }
            static bool warnedBadSpawn = false;
            if (!warnedBadSpawn) {
                warnedBadSpawn = true;
                std::cerr << "GameMap: spawn definido para jugador " << playerIndex
                          << " no es walkable (row=" << sc.row << ", col=" << sc.col << "). Usando fallback.\n";
            }
        }
    }

    // 2) Fallback: buscar la primera celda walkable desde la esquina correspondiente
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

// ============================== Render ==============================

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

            // Si el bloque fue destruido o se está rompiendo, el fondo será el suelo
            int originalId = block.spriteId;
            int displayId = animator.getDisplayId(originalId);
            
            if (block.destroyed || block.breaking) {
                // Calcular el sprite de suelo dinámicamente según el entorno
                bool wallLeft   = (c > 0 && grid[r][c-1].type == BlockType::BARRIER);
                bool wallUp     = (r > 0 && grid[r-1][c].type == BlockType::BARRIER);
                bool indestLeft = (c > 0 && grid[r][c-1].type == BlockType::INDESTRUCTIBLE);
                bool indestUp   = (r > 0 && grid[r-1][c].type == BlockType::INDESTRUCTIBLE);

                // Por defecto, suelo sombra en medio del mapa
                int floorId = 10; 

                if (wallLeft && wallUp) floorId = 6;   // Esquina arriba izquierda
                else if (wallLeft)      floorId = 9;   // Sombra izquierda (por muro/barrera)
                else if (wallUp)        floorId = 7;   // Sombra arriba (por muro/barrera)
                else if (indestUp && indestLeft) floorId = 9;  // Arriba e izquierda por indestructible -> Sombra izquierda
                else if (indestUp)       floorId = 11;  // Sombra arriba (por indestructible)
                else if (indestLeft)     floorId = 9;   // Sombra izquierda (por indestructible)
                
                displayId = floorId;
            }
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

            // Si se está rompiendo, dibujar la animación de rotura ENCIMA del suelo
            if (block.breaking && atlasLoaded) {
                std::string breakStr = "destructible_break." + std::to_string(block.breakFrame);
                glm::vec4 breakUv(0.0f, 0.0f, 1.0f, 1.0f);
                if (getUvRectForSprite(atlas, breakStr, breakUv)) {
                    glUniform4fv(uniformUvRect, 1, glm::value_ptr(breakUv));
                    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
                }
            }
        }
    }

    glBindVertexArray(0);
}

void GameMap::renderHud(GLuint vao, GLuint hudTexture,
                        GLuint uniformModel, GLuint uniformUvRect) {
    float hudWidth = cols * tileSize;

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hudTexture);

    glBindVertexArray(vao);

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(0.0f, 1.0f - hudTopSpace * 0.5f, 0.0f));
    model = glm::scale(model, glm::vec3(hudWidth * 0.5f, hudTopSpace * 0.5f, 1.0f));

    glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));

    glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
    glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}
