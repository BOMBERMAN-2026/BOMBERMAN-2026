#include "game_map.hpp"
#include "sprite_atlas.hpp" // for resolveAssetPath, loadSpriteAtlasMinimal, getUvRectForSprite
#include "player.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <random>
#include <chrono>

// Convierte un string de nivel (p.ej. "flame", "bomba", "velocidad") a PowerUpType.
// Se usa para la directiva: `powerup <type> <row> <col>` dentro de `levels/*.txt`.
// Nota: normaliza a minúsculas y elimina espacios/guiones/guiones bajos.
static bool parsePowerUpTypeFromString(const std::string& raw, PowerUpType& outType) {
    // Normalizar: minúsculas y sin guiones/espacios
    std::string s;
    s.reserve(raw.size());
    for (unsigned char ch : raw) {
        if (std::isspace(ch) || ch == '-' || ch == '_') continue;
        s.push_back((char)std::tolower(ch));
    }

    // Stackeables
    if (s == "bomb" || s == "bomba" || s == "bombup") { outType = PowerUpType::BombUp; return true; }
    if (s == "flame" || s == "fire" || s == "llama" || s == "fireup" || s == "flameup") { outType = PowerUpType::FireUp; return true; }
    if (s == "speed" || s == "velocidad" || s == "speedup") { outType = PowerUpType::SpeedUp; return true; }

    // No stackeables
    if (s == "1up" || s == "extralife" || s == "vida") { outType = PowerUpType::ExtraLife; return true; }
    if (s == "invincibility" || s == "invencibilidad") { outType = PowerUpType::Invincibility; return true; }
    if (s == "remote" || s == "remotecontrol" || s == "detonador") { outType = PowerUpType::RemoteControl; return true; }

    return false;
}

// Convierte un string de nivel (p.ej. "leon", "bebe", "fantasma", "king", "dron") a EnemySpawnType.
// Se usa para la directiva: `enemy <type> <row> <col>` dentro de `levels/*.txt`.
static bool parseEnemySpawnTypeFromString(const std::string& raw, EnemySpawnType& outType) {
    // Normaliza a minúsculas y elimina espacios/guiones/guiones bajos.
    std::string s;
    s.reserve(raw.size());
    for (unsigned char ch : raw) {
        if (std::isspace(ch) || ch == '-' || ch == '_') continue;
        s.push_back((char)std::tolower(ch));
    }

    if (s == "leon") { outType = EnemySpawnType::Leon; return true; }
    if (s == "babosa") { outType = EnemySpawnType::Babosa; return true; }
    if (s == "bebe" || s == "bebelloron") { outType = EnemySpawnType::BebeLloron; return true; }
    if (s == "fantasma" || s == "fantasmamortal") { outType = EnemySpawnType::FantasmaMortal; return true; }
    if (s == "sol" || s == "solpervertido" || s == "sol_pervertido") { outType = EnemySpawnType::SolPervertido; return true; }
    if (s == "king" || s == "kingbomber" || s == "king_bomber") { outType = EnemySpawnType::KingBomber; return true; }
    if (s == "dron" || s == "drone" || s == "dronbombardero" || s == "dron_bombardero") { outType = EnemySpawnType::DronBombardero; return true; }

    return false;
}

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
    enemySpawns.clear();

    struct PendingSpawn {
        int index = -1;
        int x = 0;      // columna
        int y = 0;      // fila
        bool inner = true; // si true: coordenadas en el área jugable (sin borde) => +1
    };
    std::vector<PendingSpawn> pendingSpawns;

    struct PendingPowerUp {
        PowerUpType type = PowerUpType::ExtraLife;
        int row = 0;
        int col = 0;
    };
    std::vector<PendingPowerUp> pendingPowerUps;

    struct PendingEnemy {
        // Enemigo definido por directiva `enemy` en el TXT.
        EnemySpawnType type = EnemySpawnType::Leon;
        int row = 0;
        int col = 0;
    };
    std::vector<PendingEnemy> pendingEnemies;

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

        // Directivas: powerup <type> <row> <col>
        {
            std::istringstream issDir(trimmed);
            std::string kw;
            issDir >> kw;
            if (kw == "powerup") {
                std::string typeStr;
                int row = 0, col = 0;
                if (!(issDir >> typeStr >> row >> col)) {
                    std::cerr << "GameMap: directiva powerup inválida: '" << trimmed << "'\n";
                    continue;
                }

                PowerUpType type;
                if (!parsePowerUpTypeFromString(typeStr, type)) {
                    std::cerr << "GameMap: powerup: tipo desconocido '" << typeStr << "'\n";
                    continue;
                }

                PendingPowerUp pp;
                pp.type = type;
                pp.row = row;
                pp.col = col;
                pendingPowerUps.push_back(pp);
                continue;
            }
        }

        // Directivas: enemy <type> <row> <col>
        {
            std::istringstream issDir(trimmed);
            std::string kw;
            issDir >> kw;
            if (kw == "enemy") {
                std::string typeStr;
                int row = 0, col = 0;
                if (!(issDir >> typeStr >> row >> col)) {
                    std::cerr << "GameMap: directiva enemy inválida: '" << trimmed << "'\n";
                    continue;
                }

                EnemySpawnType type;
                if (!parseEnemySpawnTypeFromString(typeStr, type)) {
                    std::cerr << "GameMap: enemy: tipo desconocido '" << typeStr << "'\n";
                    continue;
                }

                PendingEnemy pe;
                pe.type = type;
                pe.row = row;
                pe.col = col;
                pendingEnemies.push_back(pe);
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

    // Aplicar power-ups definidos por directiva.
    // Nota: aquí aún no sabemos si una celda es destructible (eso llega tras loadAtlas).
    // De momento marcamos el power-up; placePowerUps() ajustará visible/oculto según tipo de bloque.
    for (const auto& pp : pendingPowerUps) {
        if (pp.row < 0 || pp.row >= rows || pp.col < 0 || pp.col >= cols) {
            std::cerr << "GameMap: powerup fuera de rango (row=" << pp.row << ", col=" << pp.col << ")\n";
            continue;
        }
        Block& b = grid[pp.row][pp.col];
        b.hasPowerUp = true;
        b.powerUpType = pp.type;
        b.powerUpCollected = false;
        // provisional: visible; se corrige tras loadAtlas en placePowerUps()
        b.powerUpRevealed = true;
    }

    // Registrar spawns de enemigos definidos por directiva.
    for (const auto& pe : pendingEnemies) {
        if (pe.row < 0 || pe.row >= rows || pe.col < 0 || pe.col >= cols) {
            std::cerr << "GameMap: enemy fuera de rango (row=" << pe.row << ", col=" << pe.col << ")\n";
            continue;
        }
        EnemySpawn es;
        es.type = pe.type;
        es.row = pe.row;
        es.col = pe.col;
        enemySpawns.push_back(es);
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
                        // Revelar power-up si tenía uno escondido
                        if (b.hasPowerUp && !b.powerUpCollected) {
                            b.powerUpRevealed = true;
                        }
                    }
                }
            }
        }
    }

    // Decrementar el timer del nivel
    levelTimeRemaining -= deltaTime;
    if (levelTimeRemaining < 0.0f) levelTimeRemaining = 0.0f;
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
                else if (indestUp)      floorId = 11;  // Sombra arriba (por indestructible)
                else if (indestLeft)    floorId = 21;  // Sombra izquierda (por indestructible)
                
                displayId = floorId;
            } else if (block.type == BlockType::FLOOR) {
                // Forzar visualmente el sprite si es un suelo base
                bool indestLeft = (c > 0 && grid[r][c-1].type == BlockType::INDESTRUCTIBLE);
                bool indestUp   = (r > 0 && grid[r-1][c].type == BlockType::INDESTRUCTIBLE);
                
                if (indestUp) {
                    displayId = 11;
                } else if (indestLeft) {
                    displayId = 21;
                }
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

// Función para renderizar un score usando sprites del atlas del scoreboard
void renderScore(uint score, glm::vec2 startPos, SpriteAtlas gScoreboardAtlas, GLuint scoreboardTexture, GLuint vao, GLuint uniformModel, GLuint uniformUvRect) {
    
    if (score > 9999999) score = 9999999; // Limitar a 7 dígitos

    std::ostringstream oss;
    oss << std::setw(7) << std::setfill('0') << score;
    std::string scoreStr = oss.str();  // Ahora scoreStr siempre tiene 7 caracteres

    glBindVertexArray(vao);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, scoreboardTexture);  // Bind la textura del scoreboard

    float currentX = startPos.x;  // Posición X inicial
    float y = startPos.y;         // Posición Y fija

    for (char c : scoreStr) {
        std::string spriteName = std::string(1, c);  // Nombre del sprite (e.g., "1", "2", etc.)
        glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);

        // Obtén el UV rect del sprite desde el atlas
        if (!getUvRectForSprite(gScoreboardAtlas, spriteName, uvRect)) {
            std::cerr << "Advertencia: Sprite '" << spriteName << "' no encontrado en atlas del scoreboard\n";
            continue;  // Salta si no existe
        }

        // Calcula el ancho del sprite (basado en el atlas)
        auto it = gScoreboardAtlas.sprites.find(spriteName);
        float scale = 0.0028f; // Escala base para ajustar el tamaño del sprite en pantalla
        float spriteWidth = (it != gScoreboardAtlas.sprites.end()) ? (float)it->second.w * scale : 20.0f * scale; 
        float spriteHeight = (it != gScoreboardAtlas.sprites.end()) ? (float)it->second.h * scale : 23.0f * scale;

        // Matriz de modelo: posiciona y escala el sprite
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(currentX + spriteWidth * 0.5f, y, 0.0f));  // Centra en X
        model = glm::scale(model, glm::vec3(spriteWidth * 0.5f, spriteHeight * 0.5f, 1.0f));  // Escala (0.5f porque el quad va de -1 a 1)

        glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
        glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // Avanza la posición X para el siguiente dígito
        float interSeparation = 0.015f; // Espacio entre dígitos
        currentX += spriteWidth + interSeparation;
    }

    glBindVertexArray(0);
}

void renderLives(uint numLives, glm::vec2 startPos, SpriteAtlas gScoreboardAtlas, GLuint scoreboardTexture, GLuint vao, GLuint uniformModel, GLuint uniformUvRect) {
    
    if (numLives > 9) numLives = 9; // Máx de vidas de 9
    
    glBindVertexArray(vao);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, scoreboardTexture);  // Bind la textura del scoreboard

    float currentX = startPos.x;  // Posición X inicial
    float y = startPos.y;         // Posición Y fija

    std::string spriteName = std::to_string(numLives);
    glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);

    // Obtén el UV rect del sprite desde el atlas
    if (!getUvRectForSprite(gScoreboardAtlas, spriteName, uvRect)) {
        std::cerr << "Advertencia: Sprite '" << spriteName << "' no encontrado en atlas del scoreboard\n";
        exit;  // Salta si no existe
    }

    // Calcula el ancho del sprite (basado en el atlas)
    auto it = gScoreboardAtlas.sprites.find(spriteName);
    float scale = 0.0028f; // Escala base para ajustar el tamaño del sprite en pantalla
    float spriteWidth = (it != gScoreboardAtlas.sprites.end()) ? (float)it->second.w * scale : 20.0f * scale; 
    float spriteHeight = (it != gScoreboardAtlas.sprites.end()) ? (float)it->second.h * scale : 23.0f * scale;

    // Matriz de modelo: posiciona y escala el sprite
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(currentX + spriteWidth * 0.5f, y, 0.0f));  // Centra en X
    model = glm::scale(model, glm::vec3(spriteWidth * 0.5f, spriteHeight * 0.5f, 1.0f));  // Escala (0.5f porque el quad va de -1 a 1)

    glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
    glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);
}

void renderTimer(float time, glm::vec2 startPos, SpriteAtlas gScoreboardAtlas, GLuint scoreboardTexture, GLuint vao, GLuint uniformModel, GLuint uniformUvRect) {
    int totalSeconds = (int)std::ceil(time);
    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;

    std::string timerStr = std::to_string(minutes) + ":" + (seconds < 10 ? "0" : "") + std::to_string(seconds);

    glBindVertexArray(vao);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, scoreboardTexture);

    float currentX = startPos.x;
    float y = startPos.y;

    for (char c : timerStr) {
        std::string spriteName;
        if (c == ':') {
            spriteName = "dos_puntos";
            currentX += 0.005f; // Ajuste extra para el símbolo de dos puntos
        } else {
            spriteName = std::string(1, c);
        }
        glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);

        if (!getUvRectForSprite(gScoreboardAtlas, spriteName, uvRect)) {
            std::cerr << "Advertencia: Sprite '" << spriteName << "' no encontrado en atlas del scoreboard\n";
            continue;
        }

        auto it = gScoreboardAtlas.sprites.find(spriteName);
        float scale = 0.0028f;
        float spriteWidth = (it != gScoreboardAtlas.sprites.end()) ? (float)it->second.w * scale : 20.0f * scale; 
        float spriteHeight = (it != gScoreboardAtlas.sprites.end()) ? (float)it->second.h * scale : 23.0f * scale;


        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(currentX + spriteWidth * 0.5f, y, 0.0f));
        model = glm::scale(model, glm::vec3(spriteWidth * 0.5f, spriteHeight * 0.5f, 1.0f));

        glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
        glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        float interSeparation = 0.015f;
        currentX += spriteWidth + interSeparation;
    }

    glBindVertexArray(0);
}

void GameMap::renderCurrentLevel(glm::vec2 startPos, SpriteAtlas gScoreboardAtlas, GLuint scoreboardTexture, GLuint vao, GLuint uniformModel, GLuint uniformUvRect) {

    glBindVertexArray(vao);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, scoreboardTexture);

    float currentX = startPos.x;
    float y = startPos.y;

    for (char c : currentLevel) {
        std::string spriteName;
        if (c == '-') {
            spriteName = "guion";
        } else {
            spriteName = std::string(1, c);
        }
        glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);

        if (!getUvRectForSprite(gScoreboardAtlas, spriteName, uvRect)) {
            std::cerr << "Advertencia: Sprite '" << spriteName << "' no encontrado en atlas del scoreboard\n";
            continue;
        }

        auto it = gScoreboardAtlas.sprites.find(spriteName);
        float scale = 0.0028f;
        float spriteWidth = (it != gScoreboardAtlas.sprites.end()) ? (float)it->second.w * scale : 20.0f * scale; 
        float spriteHeight = (it != gScoreboardAtlas.sprites.end()) ? (float)it->second.h * scale : 23.0f * scale;

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(currentX + spriteWidth * 0.5f, y, 0.0f));
        model = glm::scale(model, glm::vec3(spriteWidth * 0.5f, spriteHeight * 0.5f, 1.0f));

        glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
        glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        float interSeparation = 0.018f;
        currentX += spriteWidth + interSeparation;
    }

    glBindVertexArray(0);

}

void GameMap::renderHud(GLuint vao, GLuint hudTexture,
                        GLuint uniformModel, GLuint uniformUvRect, 
                        SpriteAtlas gScoreboardAtlas, GLuint scoreboardTexture) {
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

    // Scoreboard esquina superior izquierda del HUD (1er player)
    glm::vec2 scorePos(-1.26f, 0.935f);
    renderScore(100, scorePos, gScoreboardAtlas, scoreboardTexture, vao, uniformModel, uniformUvRect);

    // Scoreboard esquina inferior izquierda del HUD (2do player)
    glm::vec2 scorePos2(-1.26f, 0.813f);
    renderScore(2000, scorePos2, gScoreboardAtlas, scoreboardTexture, vao, uniformModel, uniformUvRect);
    
    // Scoreboard esquina superior derecha del HUD (3ro player)
    glm::vec2 scorePos3(0.5675f, 0.935f);
    renderScore(300, scorePos3, gScoreboardAtlas, scoreboardTexture, vao, uniformModel, uniformUvRect);

    // Scoreboard esquina inferior derecha del HUD (4to player)
    glm::vec2 scorePos4(0.5675f, 0.813f);
    renderScore(4000, scorePos4, gScoreboardAtlas, scoreboardTexture, vao, uniformModel, uniformUvRect);

    // Scoreboard centro (general)
    glm::vec2 scorePos5(-0.135f, 0.935f);
    renderScore(100 + 2000 + 300 + 4000, scorePos5, gScoreboardAtlas, scoreboardTexture, vao, uniformModel, uniformUvRect);

    // Vidas esquina superior izquierda del HUD (1er player)
    glm::vec2 livesPos(-0.55f, 0.935f);
    renderLives(1, livesPos, gScoreboardAtlas, scoreboardTexture, vao, uniformModel, uniformUvRect);
    
    // Vidas esquina inferior izquierda del HUD (2do player)
    glm::vec2 livesPos2(-0.55f, 0.813f);
    renderLives(2, livesPos2, gScoreboardAtlas, scoreboardTexture, vao, uniformModel, uniformUvRect);

    // Vidas esquina superior derecha del HUD (3ro player)
    glm::vec2 livesPos3(1.275f, 0.935f);
    renderLives(3, livesPos3, gScoreboardAtlas, scoreboardTexture, vao, uniformModel, uniformUvRect);

    // Vidas esquina inferior derecha del HUD (4to player)
    glm::vec2 livesPos4(1.275f, 0.813f);
    renderLives(5, livesPos4, gScoreboardAtlas, scoreboardTexture, vao, uniformModel, uniformUvRect);

    // Nivel actual
    glm::vec2 levelPos(-0.270f, 0.813f);
    renderCurrentLevel(levelPos, gScoreboardAtlas, scoreboardTexture, vao, uniformModel, uniformUvRect);

    // Contador del tiempo
    glm::vec2 timePos(0.09f, 0.813f);
    renderTimer(levelTimeRemaining, timePos, gScoreboardAtlas, scoreboardTexture, vao, uniformModel, uniformUvRect);
}
// ============================== Power-Ups ==============================

// LoadTexture definido en bomberman.cpp
extern GLuint LoadTexture(const char* filePath);

void GameMap::loadPowerUpTextures() {
    if (powerUpTexturesLoaded) return;

    // Rutas de cada power-up indexadas por PowerUpType
    const char* paths[] = {
        "resources/sprites/power_ups/Bomberman_AC_-_1-UP.png",           // ExtraLife
        "resources/sprites/power_ups/Bomberman_AC_-_Bomb_Up.png",       // BombUp
        "resources/sprites/power_ups/Bomberman_AC_-_Fire_Up.png",       // FireUp
        "resources/sprites/power_ups/Bomberman_AC_-_Speed_Up.png",      // SpeedUp
        "resources/sprites/power_ups/Bomberman_AC_-_Invincibility.png", // Invincibility
        "resources/sprites/power_ups/Bomberman_AC_-_Remote_Control.png"  // RemoteControl
    };

    for (int i = 0; i < POWER_UP_TYPE_COUNT; i++) {
        std::string resolved = resolveAssetPath(paths[i]);
        powerUpTextures[i] = LoadTexture(resolved.c_str());
        if (powerUpTextures[i] == 0) {
            std::cerr << "GameMap: Error cargando textura power-up: " << resolved << std::endl;
        }
    }

    powerUpTexturesLoaded = true;
    std::cout << "GameMap: power-up textures loaded" << std::endl;
}

void GameMap::placePowerUps() {
    // 1) Si el nivel define power-ups explícitos (directiva `powerup`),
    //    no usamos colocación aleatoria; solo ajustamos si son visibles u ocultos.
    bool hasExplicit = false;
    for (int r = 0; r < rows && !hasExplicit; r++) {
        for (int c = 0; c < cols; c++) {
            if (grid[r][c].hasPowerUp) { hasExplicit = true; break; }
        }
    }

    if (hasExplicit) {
        int visibleNow = 0;
        int hiddenNow = 0;
        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < cols; c++) {
                Block& b = grid[r][c];
                if (!b.hasPowerUp || b.powerUpCollected) continue;

                // Si el tile es destructible y aún no fue destruido, queda oculto.
                if (b.type == BlockType::DESTRUCTIBLE && !b.destroyed && !b.breaking) {
                    b.powerUpRevealed = false;
                    hiddenNow++;
                } else {
                    b.powerUpRevealed = true;
                    visibleNow++;
                }
            }
        }

        std::cout << "GameMap: power-ups estáticos: " << visibleNow << " visibles, "
                  << hiddenNow << " ocultos" << std::endl;
        return;
    }

    // 2) Fallback legacy: si el nivel no define power-ups, usar el sistema aleatorio anterior.
    // Recopilar todas las celdas destructibles
    struct Cell { int r, c; };
    std::vector<Cell> destructibles;

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            if (grid[r][c].type == BlockType::DESTRUCTIBLE && !grid[r][c].destroyed) {
                destructibles.push_back({r, c});
            }
        }
    }

    if (destructibles.empty()) {
        std::cerr << "GameMap: no hay bloques destructibles para colocar power-ups" << std::endl;
        return;
    }

    // Shuffle aleatorio
    unsigned seed = (unsigned)std::chrono::system_clock::now().time_since_epoch().count();
    std::mt19937 rng(seed);
    std::shuffle(destructibles.begin(), destructibles.end(), rng);

    // Power-ups a colocar:
    // Stackeables (2 de cada): BombUp, FireUp, SpeedUp
    // No stackeables (1 de cada): ExtraLife, Invincibility, RemoteControl
    struct PowerUpPlacement {
        PowerUpType type;
        int count;
    };

    PowerUpPlacement placements[] = {
        { PowerUpType::BombUp,         2 },  // Stackeable
        { PowerUpType::FireUp,         2 },  // Stackeable
        { PowerUpType::SpeedUp,        2 },  // Stackeable
        { PowerUpType::ExtraLife,      1 },  // No stackeable
        { PowerUpType::Invincibility,  1 },  // No stackeable
        { PowerUpType::RemoteControl,  1 },  // No stackeable
    };

    int cellIndex = 0;
    int totalPlaced = 0;

    for (const auto& p : placements) {
        for (int i = 0; i < p.count; i++) {
            if (cellIndex >= (int)destructibles.size()) break;
            Cell& cell = destructibles[cellIndex++];
            Block& b = grid[cell.r][cell.c];
            b.hasPowerUp = true;
            b.powerUpType = p.type;
            b.powerUpRevealed = false;
            b.powerUpCollected = false;
            totalPlaced++;
        }
    }

    std::cout << "GameMap: " << totalPlaced << " power-ups colocados en "
              << destructibles.size() << " bloques destructibles" << std::endl;
}

void GameMap::renderPowerUps(GLuint vao, GLuint uniformModel, GLuint uniformUvRect,
                             GLuint uniformTintColor, GLuint uniformFlipX) {
    if (!powerUpTexturesLoaded) return;

    glBindVertexArray(vao);
    glUniform1f(uniformFlipX, 0.0f);
    glm::vec4 tint(1.0f, 1.0f, 1.0f, 1.0f);
    glUniform4fv(uniformTintColor, 1, glm::value_ptr(tint));

    float scale = tileSize / 2.0f;

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            const Block& block = grid[r][c];
            if (!block.hasPowerUp || !block.powerUpRevealed || block.powerUpCollected)
                continue;

            int texIdx = (int)block.powerUpType;
            if (texIdx < 0 || texIdx >= POWER_UP_TYPE_COUNT || powerUpTextures[texIdx] == 0)
                continue;

            // Bind la textura del power-up
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, powerUpTextures[texIdx]);

            glm::vec2 center = gridToNDC(r, c);
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(center, 0.0f));
            model = glm::scale(model, glm::vec3(scale, scale, 1.0f));

            glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));

            // UV rect completo para texturas individuales
            glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
            glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        }
    }

    glBindVertexArray(0);
}

bool GameMap::tryCollectPowerUp(int row, int col, Player* player) {
    if (!player) return false;
    if (row < 0 || row >= rows || col < 0 || col >= cols) return false;

    Block& b = grid[row][col];
    if (!b.hasPowerUp || !b.powerUpRevealed || b.powerUpCollected)
        return false;

    // Aplicar el power-up y marcarlo como recogido
    player->applyPowerUp(b.powerUpType);
    b.powerUpCollected = true;
    b.powerUpRevealed = false;

    // Log para debug
    const char* names[] = { "1-UP", "Bomb Up", "Fire Up", "Speed Up", "Invincibility", "Remote Control" };
    int idx = (int)b.powerUpType;
    if (idx >= 0 && idx < 6) {
        std::cout << "[PowerUp] Jugador " << player->playerId << " recogio " << names[idx] << std::endl;
    }

    return true;
}

// Destruye un power-up que esté suelto/visible en la celda alcanzada por una explosión.
void GameMap::destroyExposedPowerUp(int row, int col) {
    if (row < 0 || row >= rows || col < 0 || col >= cols) return;

    Block& b = grid[row][col];
    // Solo destruir si está suelto/visible (no si está escondido bajo un destructible)
    if (!b.hasPowerUp || !b.powerUpRevealed || b.powerUpCollected) return;

    b.hasPowerUp = false;
    b.powerUpRevealed = false;
    b.powerUpCollected = true;
}
