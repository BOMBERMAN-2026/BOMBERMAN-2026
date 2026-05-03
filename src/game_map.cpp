#include "game_map.hpp"
#include "sprite_atlas.hpp" // for resolveAssetPath, loadSpriteAtlasMinimal, getUvRectForSprite
#include "player.hpp"
#include "audio_manager.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

extern GLuint uniformFlipX;
extern GLuint uniformWhiteFlash;

#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <random>
#include <chrono>
#include <cmath>
#include <map>

extern uint32_t getHistoryRankingHighScore();
extern int gCurrentLoadedStageNum;

namespace {
    constexpr float kPowerUpPickupFxDuration = 0.26f;
    constexpr float kPowerUpPickupFxStartScale = 0.32f;
    constexpr float kPowerUpPickupFxEndScale = 1.08f;
    constexpr float kPowerUpPickupFxMaxAlpha = 0.95f;
}

// Constantes del HUD
    // HUD del modo historia
    static constexpr glm::vec2 scorePos(-1.117f, 0.935f);
    static constexpr glm::vec2 scorePos2(0.861f, 0.935f);
    static constexpr glm::vec2 scorePos5(0.015f, 0.935f);
    static constexpr glm::vec2 livesPos(-0.83f, 0.825f);
    static constexpr glm::vec2 livesPos2(1.175f, 0.825f);
    static constexpr glm::vec2 enemiesLeftPos(-0.1675f, 0.813f);
    static constexpr glm::vec2 levelPos(-0.410f, 0.935f);
    static constexpr glm::vec2 timePos(0.2350f, 0.813f);

    // HUD del modo VS
    static constexpr glm::vec2 livesPosVS1(-1.05f, 0.813f);
    static constexpr glm::vec2 livesPosVS2(0.93f, 0.813f);
    static constexpr glm::vec2 numWinsPos(-0.78f, 0.813f);
    static constexpr glm::vec2 numWinsPos2(1.20f, 0.813f);
    static constexpr glm::vec2 levelPosVS(-0.1285f, 0.813f);

static constexpr float scaleUsualHud = 0.0028f;
static constexpr float scaleLives = 0.0040f;
static constexpr float interSeparation = 0.015f;
static constexpr glm::vec2 inventoryPosP1(-1.65f, 0.65f);
static constexpr glm::vec2 inventoryPosP2(1.65f, 0.65f);

// Convierte un string de nivel (p.ej. "flame", "bomba", "velocidad") a PowerUpType.
// Se usa para la directiva: `powerup <type> <x> <y>` dentro de `levels/*.txt`.
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

    // Items
    if (s == "matches" || s == "cerillas") { outType = PowerUpType::Matches; return true; }
    if (s == "can" || s == "lata") { outType = PowerUpType::Can; return true; }
    if (s == "lighter" || s == "mechero") { outType = PowerUpType::Lighter; return true; }
    if (s == "battery" || s == "pila") { outType = PowerUpType::Battery; return true; }
    if (s == "dragonfly" || s == "libelula") { outType = PowerUpType::Dragonfly; return true; }
    if (s == "hudsonbee" || s == "hudson") { outType = PowerUpType::HudsonBee; return true; }

    return false;
}

// Convierte un string de nivel (p.ej. "leon", "bebe", "fantasma", "king", "dron") a EnemySpawnType.
// Se usa para la directiva: `enemy <type> <x> <y>` dentro de `levels/*.txt`.
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
    if (s == "dronrosa") { outType = EnemySpawnType::DronRosa; return true; }
    if (s == "dronverde") { outType = EnemySpawnType::DronVerde; return true; }
    if (s == "dronamarillo") { outType = EnemySpawnType::DronAmarillo; return true; }
    if (s == "dronazul") { outType = EnemySpawnType::DronAzul; return true; }
    if (s == "dragon" || s == "dragonjoven" || s == "dragon_joven") { outType = EnemySpawnType::DragonJoven; return true; }

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
 * - En directivas `spawn`, `powerup`, `enemy`, el fichero habla en (x,y) = (col,row) con (0,0) arriba-izquierda.
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

        // Directivas: powerup <type> <x> <y>
        {
            std::istringstream issDir(trimmed);
            std::string kw;
            issDir >> kw;
            if (kw == "powerup") {
                std::string typeStr;
                int x = 0, y = 0;
                if (!(issDir >> typeStr >> x >> y)) {
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
                pp.row = y;
                pp.col = x;
                pendingPowerUps.push_back(pp);
                continue;
            }
        }

        // Directivas: enemy <type> <x> <y>
        {
            std::istringstream issDir(trimmed);
            std::string kw;
            issDir >> kw;
            if (kw == "enemy") {
                std::string typeStr;
                int x = 0, y = 0;
                if (!(issDir >> typeStr >> x >> y)) {
                    std::cerr << "GameMap: directiva enemy inválida: '" << trimmed << "'\n";
                    continue;
                }

                EnemySpawnType type;
                if (!parseEnemySpawnTypeFromString(typeStr, type)) {
                    const bool isNumericSlot = !typeStr.empty() && std::all_of(typeStr.begin(), typeStr.end(), [](unsigned char ch) {
                        return std::isdigit(ch) != 0;
                    });
                    if (isNumericSlot) {
                        // Formato custom game: enemy <slotId> <x> <y>.
                        // Se procesa fuera de GameMap para mantener el flujo clásico intacto.
                        continue;
                    }
                    std::cerr << "GameMap: enemy: tipo desconocido '" << typeStr << "'\n";
                    continue;
                }

                PendingEnemy pe;
                pe.type = type;
                pe.row = y;
                pe.col = x;
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
            std::cerr << "GameMap: powerup fuera de rango (x=" << pp.col << ", y=" << pp.row << ")\n";
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
            std::cerr << "GameMap: enemy fuera de rango (x=" << pe.col << ", y=" << pe.row << ")\n";
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

    powerUpAnimTimer += deltaTime;
    if (powerUpAnimTimer >= 0.075f) {
        powerUpAnimTimer -= 0.075f;
        powerUpAnimFrame = (powerUpAnimFrame + 1) % 2;
    }

    // Actualizar bloques que se están rompiendo y objetos volando
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            Block& b = grid[r][c];

            // 1. Animación de rotura (2D y lógica de colisión)
            if (b.breaking) {
                b.breakTimer += deltaTime;
                if (b.breakTimer >= 0.13f) {
                    b.breakTimer -= 0.13f;
                    b.breakFrame++;
                    if (b.breakFrame >= 5) {
                        b.breaking = false;
                        b.destroyed = true; 
                        if (b.hasPowerUp && !b.powerUpCollected) {
                            b.powerUpRevealed = true;
                        }
                    }
                }
            }

            // 2. Física 3D de "salir volando"
            if (b.isFlying) {
                b.flyTimer += deltaTime;
                
                // Aplicar velocidad y gravedad reducida para que suban más
                b.flyOffset += b.flyVel * deltaTime;
                b.flyVel.y -= 4.0f * deltaTime; // gravedad suave para que no caigan tan rápido
                b.flyRot += b.flyRotVel * deltaTime;

                // Desaparecer rápido (antes de que vuelvan a caer al suelo)
                if (b.flyTimer >= 1.2f) {
                    b.isFlying = false;
                    if (b.itemExploding) {
                        b.itemExploding = false;
                        b.hasPowerUp = false;
                    }
                }
            }

            if (b.powerUpPickupFxActive) {
                b.powerUpPickupFxTimer += deltaTime;
                if (b.powerUpPickupFxTimer >= kPowerUpPickupFxDuration) {
                    b.powerUpPickupFxActive = false;
                    b.powerUpPickupFxTimer = 0.0f;
                    if (b.powerUpCollected) {
                        b.hasPowerUp = false;
                    }
                }
            }

            if (b.itemExploding) {
                b.itemExplodingTimer += deltaTime;
                if (b.itemExplodingTimer >= 0.065f) {
                    b.itemExplodingTimer -= 0.065f;
                    b.itemExplodingFrame++;
                    if (b.itemExplodingFrame >= 8) {
                        b.itemExploding = false;
                        b.hasPowerUp = false;
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

        const bool indestUpLeft = (row > 0 && col > 0 && grid[row - 1][col - 1].type == BlockType::INDESTRUCTIBLE);

        int floorId = 10;
        if (wallLeft && wallUp) floorId = 6;
        else if (indestLeft && wallUp) floorId = 6;
        else if (indestUp && wallLeft) floorId = 6;
        else if (indestUp && indestUpLeft) floorId = 7;
        else if (indestLeft && indestUpLeft) floorId = 9;
        else if (wallLeft && !indestUp) floorId = 9;
        else if (wallUp && !indestLeft) floorId = 7;
        else if (indestUp && indestLeft) floorId = 9;
        else if (indestUp) floorId = 11;
        else if (indestLeft) floorId = 21;
        else if (indestUpLeft && !wallLeft && !indestUp) floorId = 10;

        displayId = floorId;
    } else if (block.type == BlockType::FLOOR) {
        bool wallLeft = (col > 0 && grid[row][col - 1].type == BlockType::BARRIER);
        bool wallUp = (row > 0 && grid[row - 1][col].type == BlockType::BARRIER);
        bool indestLeft = (col > 0 && grid[row][col - 1].type == BlockType::INDESTRUCTIBLE);
        bool indestUp = (row > 0 && grid[row - 1][col].type == BlockType::INDESTRUCTIBLE);
        bool indestUpLeft = (row > 0 && col > 0 && grid[row - 1][col - 1].type == BlockType::INDESTRUCTIBLE);

        if (wallLeft && wallUp) displayId = 6;
        else if (indestLeft && wallUp) displayId = 6;
        else if (indestUp && wallLeft) displayId = 6;
        else if (indestUp && indestUpLeft) displayId = 7;
        else if (indestLeft && indestUpLeft) displayId = 9;
        else if (wallLeft && !indestUp) displayId = 9;
        else if (wallUp && !indestLeft) displayId = 7;
        else if (indestUp && indestLeft) displayId = 9;
        else if (indestUp) displayId = 11;
        else if (indestLeft) displayId = 21;
    }

    const std::string idStr = std::to_string(displayId);
    if (getUvRectForSprite(atlas, idStr, uvRect)) {
        return true;
    }

    // Fallback defensivo para evitar "tiles grises" cuando un ID no exista en el atlas.
    int fallbackId = 10; // Suelo neutro
    if (block.type == BlockType::DESTRUCTIBLE) {
        fallbackId = 20;
    } else if (block.type == BlockType::INDESTRUCTIBLE) {
        fallbackId = 8;
    } else if (block.type == BlockType::BARRIER) {
        fallbackId = 0;
    }

    const std::string fallbackIdStr = std::to_string(fallbackId);
    if (getUvRectForSprite(atlas, fallbackIdStr, uvRect)) {
        return true;
    }

    // Último recurso: cualquier suelo válido conocido por el motor.
    return getUvRectForSprite(atlas, "6", uvRect);
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
    
    // El bloque se marca como destruido y tipo suelo para que se vea el fondo y se pueda pasar,
    // pero b.breaking lo mantendrá dibujando la animación de rotura en 2D.
    b.type = BlockType::FLOOR;
    b.destroyed = true;

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

            if (gCurrentLoadedStageNum == 2 && (originalId == 12 || originalId == 13)) {
                continue;
            }
            
            if (block.destroyed || block.breaking) {
                // Calcular el sprite de suelo dinámicamente según el entorno
                bool wallLeft   = (c > 0 && grid[r][c-1].type == BlockType::BARRIER);
                bool wallUp     = (r > 0 && grid[r-1][c].type == BlockType::BARRIER);
                bool indestLeft = (c > 0 && grid[r][c-1].type == BlockType::INDESTRUCTIBLE);
                bool indestUp   = (r > 0 && grid[r-1][c].type == BlockType::INDESTRUCTIBLE);

                bool indestUpLeft = (r > 0 && c > 0 && grid[r-1][c-1].type == BlockType::INDESTRUCTIBLE);

                int floorId = 10; 
                if (wallLeft && wallUp) floorId = 6;
                else if (indestLeft && wallUp) floorId = 6;
                else if (indestUp && wallLeft) floorId = 6;
                else if (indestUp && indestUpLeft) floorId = 7;
                else if (indestLeft && indestUpLeft) floorId = 9;
                else if (wallLeft && !indestUp) floorId = 9;
                else if (wallUp && !indestLeft) floorId = 7;
                else if (indestUp && indestLeft) floorId = 9;
                else if (indestUp) floorId = 11;
                else if (indestLeft) floorId = 21;
                
                displayId = floorId;
            } else if (block.type == BlockType::FLOOR) {
                // Aplicar las mismas reglas férreas a todo el suelo
                bool wallLeft   = (c > 0 && grid[r][c-1].type == BlockType::BARRIER);
                bool wallUp     = (r > 0 && grid[r-1][c].type == BlockType::BARRIER);
                bool indestLeft = (c > 0 && grid[r][c-1].type == BlockType::INDESTRUCTIBLE);
                bool indestUp   = (r > 0 && grid[r-1][c].type == BlockType::INDESTRUCTIBLE);
                bool indestUpLeft = (r > 0 && c > 0 && grid[r-1][c-1].type == BlockType::INDESTRUCTIBLE);
                
                if (wallLeft && wallUp) displayId = 6;
                else if (indestLeft && wallUp) displayId = 6;
                else if (indestUp && wallLeft) displayId = 6;
                else if (indestUp && indestUpLeft) displayId = 7;
                else if (indestLeft && indestUpLeft) displayId = 9;
                else if (wallLeft && !indestUp) displayId = 9;
                else if (wallUp && !indestLeft) displayId = 7;
                else if (indestUp && indestLeft) displayId = 9;
                else if (indestUp) displayId = 11;
                else if (indestLeft) displayId = 21;
                else if (indestUpLeft && !wallLeft && !indestUp) displayId = 10;
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

void GameMap::renderHudUtils(uint32_t data, glm::vec2 startPos, float scale, typeOfHud hudType, SpriteAtlas gScoreboardAtlas, GLuint scoreboardTexture, GLuint vao, GLuint uniformModel, GLuint uniformUvRect) {

    std::string displayInfo;
    std::ostringstream oss;

    uint32_t totalSeconds;
    uint32_t minutes;
    uint32_t seconds;

    switch (hudType){
        case typeOfHud::Score:
            displayInfo = data > 9999999 ? "9999999" : std::to_string(data);
            oss << std::setw(7) << std::setfill('0') << displayInfo;
            displayInfo = oss.str();  // Fixeamos el valor de score a 7 digitos añadiendo 0 a la izquierda
            break;
        case typeOfHud::Lives:
            displayInfo = std::to_string(data > 9 ? 9 : data);
            break;
        case typeOfHud::Timer:
            totalSeconds = std::ceil(data);
            minutes = totalSeconds / 60;
            seconds = totalSeconds % 60;
            displayInfo = std::to_string(minutes) + ":" + (seconds < 10 ? "0" : "") + std::to_string(seconds);
            break;
        case typeOfHud::CurrentLevel:
            displayInfo = currentLevel;
            break;
        case typeOfHud::EnemiesLeft:
            displayInfo = std::to_string(data);
            break;
        case typeOfHud::NumWins:
            displayInfo = data > 99 ? "99" : std::to_string(data);
            oss << std::setw(2) << std::setfill('0') << displayInfo;
            displayInfo = oss.str();
            break;
        case typeOfHud::CurrentLevelVS:
            displayInfo = currentLevelVS;
            break;
        default:
            std::cerr << "Error: tipo de dato del HUD a mostrar no reconocido\n";
            break;
    }

    glBindVertexArray(vao);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, scoreboardTexture);

    float currentX = startPos.x;
    float y = startPos.y;

    for (char c : displayInfo) {

        std::string spriteName = std::string(1, c);
        glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);

        if (!getUvRectForSprite(gScoreboardAtlas, spriteName, uvRect)) {
            std::cerr << "Advertencia: Sprite '" << spriteName << "' no encontrado en atlas del scoreboard\n";
            continue;
        }

        auto it = gScoreboardAtlas.sprites.find(spriteName);
        float spriteWidth = (it != gScoreboardAtlas.sprites.end()) ? (float)it->second.w * scale : 20.0f * scale; 
        float spriteHeight = (it != gScoreboardAtlas.sprites.end()) ? (float)it->second.h * scale : 23.0f * scale;

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(currentX + spriteWidth * 0.5f, y, 0.0f));
        model = glm::scale(model, glm::vec3(spriteWidth * 0.5f, spriteHeight * 0.5f, 1.0f));

        glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
        glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        currentX += spriteWidth + interSeparation;
    }

    glBindVertexArray(0);

}

void GameMap::renderPlayerInventory(const Player* player, glm::vec2 startPos, float direction,
                                     GLuint vao, GLuint uniformModel, GLuint uniformUvRect, GLuint uniformTintColor,
                                     SpriteAtlas gScoreboardAtlas, GLuint scoreboardTexture) {
    if (!player || !powerUpTexturesLoaded) return;

    // Cada icono es un quad unitario [-1,1], el scale en NDC da el tamaño real.
    // iconScale de 0.055 → quad de 0.11 NDC ≈ 50-70 px dependiendo de resolución
    const float iconScale   = 0.055f;
    const float spacing     = iconScale * 2.0f * 1.35f; // espaciado = ancho del icono + margen para el contador
    const float counterScale = 0.0022f;                  // escala del texto "xN" (igual a scaleUsualHud)

    // ── Recopilar qué iconos mostrar, en orden de inserción ──────────────────
    // Usamos un vector para mantener el orden y un mapa para contar repeticiones.
    std::vector<PowerUpType> orderedTypes;
    std::map<PowerUpType, int> counts;

    auto addType = [&](PowerUpType t, int increment = 1) {
        if (counts.find(t) == counts.end()) {
            orderedTypes.push_back(t);
            counts[t] = 0;
        }
        counts[t] += increment;
    };

    // Ítems recogidos explícitamente
    for (auto type : player->collectedItems) {
        addType(type);
    }
    // Importante: no duplicar conteos con estado derivado (maxBombs/explosionPower/speed/...)
    // porque cada recogida ya se registra en collectedItems dentro de Player::applyPowerUp.

    if (orderedTypes.empty()) return;

    glBindVertexArray(vao);
    glUniform1f(uniformFlipX, 0.0f);
    glUniform1f(uniformWhiteFlash, 0.0f);
    const glm::vec4 whiteTint(1.0f, 1.0f, 1.0f, 1.0f);
    const glm::vec4 uvFull(0.0f, 0.0f, 1.0f, 1.0f);

    float currentX = startPos.x;
    float currentY = startPos.y;

    for (auto type : orderedTypes) {
        int texIdx = (int)type;
        if (texIdx < 0 || texIdx >= POWER_UP_TYPE_COUNT || powerUpTextures[texIdx][0] == 0)
            continue;

        // ── Dibujar icono ─────────────────────────────────────────────────────
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, powerUpTextures[texIdx][0]);

        glm::mat4 model(1.0f);
        model = glm::translate(model, glm::vec3(currentX, currentY, 0.0f));
        model = glm::scale(model, glm::vec3(iconScale, iconScale, 1.0f));

        glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
        glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvFull));
        glUniform4fv(uniformTintColor, 1, glm::value_ptr(whiteTint));

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // ── Dibujar contador "xN" al lado del icono ────────────────────────────
        int cnt = counts[type];
        if (cnt >= 1) {
            std::string label = "x" + std::to_string(cnt);

            // Cambiar textura al atlas del scoreboard
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, scoreboardTexture);

            // Posición: justo a la derecha del icono, centrado verticalmente con él
            float labelX = currentX + iconScale * 0.9f; 
            float labelY = currentY - iconScale * 0.15f; 

            for (char ch : label) {
                std::string spriteName(1, ch);
                glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
                if (!getUvRectForSprite(gScoreboardAtlas, spriteName, uvRect)) continue;

                auto it = gScoreboardAtlas.sprites.find(spriteName);
                float sw = (it != gScoreboardAtlas.sprites.end()) ? (float)it->second.w * counterScale : 20.0f * counterScale;
                float sh = (it != gScoreboardAtlas.sprites.end()) ? (float)it->second.h * counterScale : 23.0f * counterScale;

                glm::mat4 cmodel(1.0f);
                cmodel = glm::translate(cmodel, glm::vec3(labelX + sw * 0.5f, labelY, 0.0f));
                cmodel = glm::scale(cmodel, glm::vec3(sw * 0.5f, sh * 0.5f, 1.0f));

                glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(cmodel));
                glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));
                glUniform4fv(uniformTintColor, 1, glm::value_ptr(whiteTint));

                glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

                labelX += sw + 0.004f; // avance letra a letra
            }

            // Restaurar textura del power-up para la siguiente iteración
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, powerUpTextures[texIdx][0]);
        }

        currentX += direction * spacing;
    }
}


void GameMap::renderHud(GLuint vao, GLuint uniformModel, GLuint uniformUvRect, GLuint uniformTintColor,
                        SpriteAtlas gScoreboardAtlas, GLuint scoreboardTexture,
                        std::vector<int>* playerScores, std::vector<Player*>* gPlayers, std::vector<Enemy*>* gEnemies, std::string currentGameLevel, float levelTimeRemaining, uint8_t gamemode, bool showInventory) {
    float hudWidth = cols * tileSize;

    glActiveTexture(GL_TEXTURE0);
    glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);

    // La variable gamemode es para decidir si estamos en modo historia (0) o versus (1) 
    // (mirar dnd se llama renderHud para ver como se da el valor)

    if (gamemode == 0) {
        if (!getUvRectForSprite(gScoreboardAtlas, "scoreboard", uvRect)) {
            std::cerr << "Advertencia: Sprite '" << "Scoreboard" << "' no encontrado en atlas del scoreboard\n";
            exit(1);
        }
    }
    else if (gamemode == 1) {
        if (!getUvRectForSprite(gScoreboardAtlas, "scoreboardVersusMode", uvRect)) {
            std::cerr << "Advertencia: Sprite '" << "scoreboardVersusMode" << "' no encontrado en atlas del scoreboard\n";
            exit(1);
        }
    }

    glBindTexture(GL_TEXTURE_2D, scoreboardTexture);
    glBindVertexArray(vao);

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(0.0f, 1.0f - hudTopSpace * 0.5f, 0.0f));
    model = glm::scale(model, glm::vec3(hudWidth * 0.5f, hudTopSpace * 0.5f, 1.0f));

    glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
    glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    if (gamemode == 0) { // Modo historia
        // SCORES
        // esquina superior izquierda del HUD (1er player)
        renderHudUtils((*playerScores)[0], scorePos, scaleUsualHud, typeOfHud::Score, gScoreboardAtlas, scoreboardTexture, vao, uniformModel, uniformUvRect);

            // esquina superior derecha del HUD (2do player)
        renderHudUtils( playerScores->size() > 1 ? (*playerScores)[1] : 0, scorePos2, scaleUsualHud, typeOfHud::Score, gScoreboardAtlas, scoreboardTexture, vao, uniformModel, uniformUvRect);

            // centro (highest score)
        renderHudUtils(getHistoryRankingHighScore(), scorePos5, scaleUsualHud, typeOfHud::Score, gScoreboardAtlas, scoreboardTexture, vao, uniformModel, uniformUvRect);

        // VIDAS
            // esquina superior izquierda del HUD (1er player)
        renderHudUtils(gPlayers->at(0)->lives, livesPos, scaleLives, typeOfHud::Lives, gScoreboardAtlas, scoreboardTexture, vao, uniformModel, uniformUvRect);

            // esquina inferior izquierda del HUD (2do player)
            // comprobamos que tengamos dos players, sino ponemos 0
        renderHudUtils( (gPlayers->size() > 1) ? gPlayers->at(1)->lives : 0, livesPos2, scaleLives, typeOfHud::Lives, gScoreboardAtlas, scoreboardTexture, vao, uniformModel, uniformUvRect);

        // ENEMIGOS RESTANTES
        renderHudUtils(gEnemies->size(), enemiesLeftPos, scaleUsualHud, typeOfHud::EnemiesLeft, gScoreboardAtlas, scoreboardTexture, vao, uniformModel, uniformUvRect);

        // NIVEL ACTUAL
        currentLevel = currentGameLevel;
        renderHudUtils(0, levelPos, scaleUsualHud, typeOfHud::CurrentLevel, gScoreboardAtlas, scoreboardTexture, vao, uniformModel, uniformUvRect);

        // TIMER
        renderHudUtils(levelTimeRemaining, timePos, scaleUsualHud, typeOfHud::Timer, gScoreboardAtlas, scoreboardTexture, vao, uniformModel, uniformUvRect);

        // INVENTARIO (Power-Ups e ítems recogidos) arriba a la izquierda
        if (showInventory && gPlayers && !gPlayers->empty() && gPlayers->at(0)) {
            renderPlayerInventory(gPlayers->at(0), inventoryPosP1, 1.0f,
                                  vao, uniformModel, uniformUvRect, uniformTintColor,
                                  gScoreboardAtlas, scoreboardTexture);
        }
    }
    else if (gamemode == 1) { // Modo Versus
        // VIDAS
            // esquina superior izquierda del HUD (1er player)
        const uint32_t p1Lives = (gPlayers && !gPlayers->empty() && gPlayers->at(0) != nullptr) ? (uint32_t)gPlayers->at(0)->lives : 0;
        renderHudUtils(p1Lives, livesPosVS1, scaleUsualHud, typeOfHud::Lives, gScoreboardAtlas, scoreboardTexture, vao, uniformModel, uniformUvRect);

            // esquina inferior izquierda del HUD (2do player)
        const uint32_t p2Lives = (gPlayers && gPlayers->size() > 1 && gPlayers->at(1) != nullptr) ? (uint32_t)gPlayers->at(1)->lives : 0;
        renderHudUtils(p2Lives, livesPosVS2, scaleUsualHud, typeOfHud::Lives, gScoreboardAtlas, scoreboardTexture, vao, uniformModel, uniformUvRect);
        
        // NUM WINS
        const uint32_t p1Wins = (playerScores && !playerScores->empty()) ? (uint32_t)playerScores->at(0) : 0;
        const uint32_t p2Wins = (playerScores && playerScores->size() > 1) ? (uint32_t)playerScores->at(1) : 0;
        renderHudUtils(p1Wins, numWinsPos, scaleUsualHud, typeOfHud::NumWins, gScoreboardAtlas, scoreboardTexture, vao, uniformModel, uniformUvRect);
        renderHudUtils(p2Wins, numWinsPos2, scaleUsualHud, typeOfHud::NumWins, gScoreboardAtlas, scoreboardTexture, vao, uniformModel, uniformUvRect);

        // NIVEL ACTUAL
        currentLevelVS = currentGameLevel;
        renderHudUtils(0, levelPosVS, scaleUsualHud, typeOfHud::CurrentLevelVS, gScoreboardAtlas, scoreboardTexture, vao, uniformModel, uniformUvRect);

        // TIMER
        renderHudUtils(levelTimeRemaining, timePos, scaleUsualHud, typeOfHud::Timer, gScoreboardAtlas, scoreboardTexture, vao, uniformModel, uniformUvRect);

        // INVENTARIO (Power-Ups e ítems recogidos) P1
        if (showInventory && gPlayers && !gPlayers->empty() && gPlayers->at(0)) {
            const glm::vec2 invPos = (gPlayers->at(0)->playerId == 1) ? inventoryPosP2 : inventoryPosP1;
            const float invDir = (gPlayers->at(0)->playerId == 1) ? -1.0f : 1.0f;
            renderPlayerInventory(gPlayers->at(0), invPos, invDir,
                                  vao, uniformModel, uniformUvRect, uniformTintColor,
                                  gScoreboardAtlas, scoreboardTexture);
        }
    }
}
// ============================== Power-Ups ==============================

// LoadTexture definido en bomberman.cpp
extern GLuint LoadTexture(const char* filePath);
extern GLuint uniformWhiteFlash;
extern SpriteAtlas gExplosionObjetoAtlas;
extern GLuint gExplosionObjetoTexture;

void GameMap::loadPowerUpTextures() {
    if (powerUpTexturesLoaded) return;

    // Rutas de cada power-up indexadas por PowerUpType y variante (normal / azul)
    const char* paths[POWER_UP_TYPE_COUNT][2] = {
        {"resources/sprites/power_ups/Bomberman_AC_-_1-UP.png",           "resources/sprites/power_ups/Bomberman_AC_-_1-UP_azul.png"},
        {"resources/sprites/power_ups/Bomberman_AC_-_Bomb_Up.png",       "resources/sprites/power_ups/Bomberman_AC_-_Bomb_Up_azul.png"},
        {"resources/sprites/power_ups/Bomberman_AC_-_Fire_Up.png",       "resources/sprites/power_ups/Bomberman_AC_-_Fire_Up_azul.png"},
        {"resources/sprites/power_ups/Bomberman_AC_-_Speed_Up.png",      "resources/sprites/power_ups/Bomberman_AC_-_Speed_Up_azul.png"},
        {"resources/sprites/power_ups/Bomberman_AC_-_Invincibility.png", "resources/sprites/power_ups/Bomberman_AC_-_Invincibility_azul.png"},
        {"resources/sprites/power_ups/Bomberman_AC_-_Remote_Control.png", "resources/sprites/power_ups/Bomberman_AC_-_Remote_Control_azul.png"},
        
        {"resources/sprites/items/B_AC_-_Bonus_Item_Matches.png",            "resources/sprites/items/B_AC_-_Bonus_Item_Matches_azul.png"},
        {"resources/sprites/items/B_AC_-_Bonus_Item_Can.png",                "resources/sprites/items/B_AC_-_Bonus_Item_Can_azul.png"},
        {"resources/sprites/items/B_AC_-_Bonus_Item_Lighter.png",            "resources/sprites/items/B_AC_-_Bonus_Item_Lighter_azul.png"},
        {"resources/sprites/items/B_AC_-_Bonus_Item_Battery.png",            "resources/sprites/items/B_AC_-_Bonus_Item_Battery_azul.png"},
        {"resources/sprites/items/B_AC_-_Bonus_Item_Dragonfly.png",          "resources/sprites/items/B_AC_-_Bonus_Item_Dragonfly_azul.png"},
        {"resources/sprites/items/B_AC_-_Bonus_Item_Hudson Bee.png",         "resources/sprites/items/B_AC_-_Bonus_Item_Hudson Bee_azul.png"}
    };

    for (int i = 0; i < POWER_UP_TYPE_COUNT; i++) {
        for (int j = 0; j < 2; j++) {
            std::string resolved = resolveAssetPath(paths[i][j]);
            powerUpTextures[i][j] = LoadTexture(resolved.c_str());
            if (powerUpTextures[i][j] == 0) {
                std::cerr << "GameMap: Error cargando textura power-up: " << resolved << std::endl;
            }
        }
    }

    powerUpTexturesLoaded = true;
    std::cout << "GameMap: power-up textures loaded" << std::endl;
}


GLuint GameMap::getPowerUpTexture(PowerUpType type, int frame) const {
    int idx = (int)type;
    if (idx < 0 || idx >= POWER_UP_TYPE_COUNT) return 0;
    int f = std::max(0, std::min(1, frame));
    return powerUpTextures[idx][f];
}

void GameMap::placePowerUps(bool excludeItemsInVersus, bool excludeExtraLife) {
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

    std::vector<PowerUpPlacement> placementsVec;
    placementsVec.push_back({ PowerUpType::BombUp, 2 });
    placementsVec.push_back({ PowerUpType::FireUp, 2 });
    placementsVec.push_back({ PowerUpType::SpeedUp, 2 });
    if (!excludeExtraLife) placementsVec.push_back({ PowerUpType::ExtraLife, 1 });
    placementsVec.push_back({ PowerUpType::Invincibility, 1 });
    placementsVec.push_back({ PowerUpType::RemoteControl, 1 });

    int cellIndex = 0;
    int totalPlaced = 0;

    for (const auto& p : placementsVec) {
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
    
    // Asignar items de puntuación en los bloques destructibles sobrantes
    // Solo se colocan si no estamos en Versus (excludeItemsInVersus = false)
    int itemsPlaced = 0;
    if (!excludeItemsInVersus) {
        std::uniform_real_distribution<float> itemDist(0.0f, 100.0f);

        while (cellIndex < (int)destructibles.size()) {
            float randVal = itemDist(rng);
            PowerUpType chosenItemType;
            bool placeItem = false;

            // Probabilidades acumulativas aproximadas (del 0 a 100%)
            // ~10% de que un bloque sobrante tenga un item
            if (randVal < 10.0f) {
                placeItem = true;
                float rarityRoll = itemDist(rng); // 0 a 100
                
                if (rarityRoll < 35.0f) chosenItemType = PowerUpType::Matches;        // 35% dentro del pool (Común)
                else if (rarityRoll < 70.0f) chosenItemType = PowerUpType::Can;       // 35% dentro del pool (Común)
                else if (rarityRoll < 85.0f) chosenItemType = PowerUpType::Lighter;   // 15% dentro del pool (Poco común)
                else if (rarityRoll < 93.0f) chosenItemType = PowerUpType::Battery;   // 8% dentro del pool (Rara)
                else if (rarityRoll < 98.0f) chosenItemType = PowerUpType::Dragonfly; // 5% dentro del pool (Rara)
                else chosenItemType = PowerUpType::HudsonBee;                         // 2% dentro del pool (Muy rara)
            }

            if (placeItem) {
                Cell& cell = destructibles[cellIndex];
                Block& b = grid[cell.r][cell.c];
                b.hasPowerUp = true;
                b.powerUpType = chosenItemType;
                b.powerUpRevealed = false;
                b.powerUpCollected = false;
                itemsPlaced++;
            }
            
            cellIndex++;
        }
    }

    std::cout << "GameMap: " << totalPlaced << " power-ups fijos y " << itemsPlaced 
              << " items de puntuacion colocados en " << destructibles.size() << " bloques destructibles" << std::endl;
}

bool GameMap::getVisiblePowerUpType(int row, int col, PowerUpType& outType) const {
    if (row < 0 || row >= rows || col < 0 || col >= cols) return false;

    const Block& b = grid[row][col];
    if (!b.hasPowerUp || !b.powerUpRevealed || b.powerUpCollected) return false;

    outType = b.powerUpType;
    return true;
}

bool GameMap::getHiddenPowerUpType(int row, int col, PowerUpType& outType) const {
    if (row < 0 || row >= rows || col < 0 || col >= cols) return false;

    const Block& b = grid[row][col];
    if (!b.hasPowerUp || b.powerUpRevealed || b.powerUpCollected) return false;

    outType = b.powerUpType;
    return true;
}

bool GameMap::getPowerUpPickupFx(int row, int col, PowerUpType& outType, float& outNormalizedTime) const {
    if (row < 0 || row >= rows || col < 0 || col >= cols) return false;

    const Block& b = grid[row][col];
    if (!b.powerUpPickupFxActive) return false;

    outType = b.powerUpType;
    outNormalizedTime = b.powerUpPickupFxTimer / kPowerUpPickupFxDuration;
    outNormalizedTime = std::max(0.0f, std::min(1.0f, outNormalizedTime));
    return true;
}

void GameMap::renderPowerUps(GLuint vao, GLuint uniformModel, GLuint uniformUvRect,
                             GLuint uniformTintColor, GLuint uniformFlipX) {
    if (!powerUpTexturesLoaded) return;

    glBindVertexArray(vao);
    glUniform1f(uniformFlipX, 0.0f);
    glUniform1f(uniformWhiteFlash, 0.0f);

    float scale = tileSize / 2.0f;
    glm::vec4 uvRectFull(0.0f, 0.0f, 1.0f, 1.0f);
    glm::vec4 baseTint(1.0f, 1.0f, 1.0f, 1.0f);

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            const Block& block = grid[r][c];

            if (block.hasPowerUp && block.powerUpRevealed && !block.powerUpCollected) {
                int texIdx = (int)block.powerUpType;
                if (texIdx >= 0 && texIdx < POWER_UP_TYPE_COUNT && powerUpTextures[texIdx][powerUpAnimFrame] != 0) {
                    glm::vec2 center = gridToNDC(r, c);

                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, powerUpTextures[texIdx][powerUpAnimFrame]);

                    glm::mat4 model = glm::mat4(1.0f);
                    model = glm::translate(model, glm::vec3(center, 0.0f));
                    model = glm::scale(model, glm::vec3(scale, scale, 1.0f));

                    glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
                    glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRectFull));
                    glUniform4fv(uniformTintColor, 1, glm::value_ptr(baseTint));
                    glUniform1f(uniformWhiteFlash, 0.0f);
                    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
                }
            }

            if (block.powerUpPickupFxActive) {
                const float tRaw = block.powerUpPickupFxTimer / kPowerUpPickupFxDuration;
                const float t = std::max(0.0f, std::min(1.0f, tRaw));
                const float easeOut = 1.0f - (1.0f - t) * (1.0f - t);
                const float fxScale = scale * (kPowerUpPickupFxStartScale + (kPowerUpPickupFxEndScale - kPowerUpPickupFxStartScale) * easeOut);
                const float fxAlpha = (1.0f - t) * kPowerUpPickupFxMaxAlpha;

                int texIdx = (int)block.powerUpType;
                if (texIdx >= 0 && texIdx < POWER_UP_TYPE_COUNT && powerUpTextures[texIdx][powerUpAnimFrame] != 0) {
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, powerUpTextures[texIdx][powerUpAnimFrame]);

                    glm::vec2 center = gridToNDC(r, c);
                    glm::mat4 fxModel = glm::mat4(1.0f);
                    fxModel = glm::translate(fxModel, glm::vec3(center, 0.0f));
                    fxModel = glm::scale(fxModel, glm::vec3(fxScale, fxScale, 1.0f));

                    glm::vec4 fxTint(1.0f, 1.0f, 1.0f, fxAlpha);
                    glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(fxModel));
                    glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRectFull));
                    glUniform4fv(uniformTintColor, 1, glm::value_ptr(fxTint));
                    glUniform1f(uniformWhiteFlash, 1.0f);
                    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
                    glUniform1f(uniformWhiteFlash, 0.0f);
                }
            }
        }
    }

    glUniform4fv(uniformTintColor, 1, glm::value_ptr(baseTint));
    glUniform1f(uniformWhiteFlash, 0.0f);
    glBindVertexArray(0);
}

void GameMap::renderPowerUpExplosions(GLuint vao, GLuint uniformModel, GLuint uniformUvRect, GLuint uniformTintColor, GLuint uniformFlipX) {
    if (gExplosionObjetoTexture == 0) return;

    glBindVertexArray(vao);
    glUniform1f(uniformFlipX, 0.0f);
    glUniform1f(uniformWhiteFlash, 0.0f);

    float scale = tileSize / 2.0f;
    glm::vec4 baseTint(1.0f, 1.0f, 1.0f, 1.0f);

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            const Block& block = grid[r][c];
            if (block.itemExploding) {
                std::string spriteName = "explosion_objeto." + std::to_string(block.itemExplodingFrame);
                glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
                if (getUvRectForSprite(gExplosionObjetoAtlas, spriteName, uvRect)) {
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, gExplosionObjetoTexture);

                    glm::vec2 center = gridToNDC(r, c);
                    glm::mat4 expModel = glm::mat4(1.0f);
                    expModel = glm::translate(expModel, glm::vec3(center, 0.0f));
                    
                    float scaleX = scale;
                    float scaleY = scale;
                    auto it = gExplosionObjetoAtlas.sprites.find(spriteName);
                    if (it != gExplosionObjetoAtlas.sprites.end()) {
                        // El sprite estándar mide 48x48. Escalar usando su tamaño real del atlas.
                        scaleX = ((float)it->second.w / 48.0f) * scale;
                        scaleY = ((float)it->second.h / 48.0f) * scale;
                    }
                    
                    expModel = glm::scale(expModel, glm::vec3(scaleX, scaleY, 1.0f));

                    glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(expModel));
                    glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));
                    glUniform4fv(uniformTintColor, 1, glm::value_ptr(baseTint));
                    glUniform1f(uniformWhiteFlash, 0.0f);
                    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
                }
            }
        }
    }

    glUniform4fv(uniformTintColor, 1, glm::value_ptr(baseTint));
    glUniform1f(uniformWhiteFlash, 0.0f);
    glBindVertexArray(0);
}

bool GameMap::tryCollectPowerUp(int row, int col, Player* player) {
    if (!player) return false;
    if (row < 0 || row >= rows || col < 0 || col >= cols) return false;

    Block& b = grid[row][col];
    if (!b.hasPowerUp || !b.powerUpRevealed || b.powerUpCollected)
        return false;

    // Reproducir sonido instantáneo de recogida
    AudioManager::get().playVfx(VfxSound::Pickup);

    // Aplicar el power-up y marcarlo como recogido
    player->applyPowerUp(b.powerUpType);
    b.powerUpCollected = true;
    b.powerUpRevealed = false;
    b.powerUpPickupFxActive = true;
    b.powerUpPickupFxTimer = 0.0f;

    // Log para debug
    const char* names[] = { "1-UP", "Bomb Up", "Fire Up", "Speed Up", "Invincibility", "Remote Control",
                            "Matches", "Can", "Lighter", "Battery", "Dragonfly", "HudsonBee" };
    int idx = (int)b.powerUpType;
    if (idx >= 0 && idx < 12) {
        std::cout << "[PowerUp] Jugador " << player->playerId << " recogio " << names[idx] << std::endl;
    }

    return true;
}

// Destruye un power-up que esté suelto/visible en la celda alcanzada por una explosión.
void GameMap::destroyExposedPowerUp(int row, int col) {
    if (row < 0 || row >= rows || col < 0 || col >= cols) return;

    Block& b = grid[row][col];
    // Solo destruir si está suelto/visible (no si está escondido bajo un destructible)
    if (!b.hasPowerUp || !b.powerUpRevealed || b.powerUpCollected || b.itemExploding) return;

    b.itemExploding = true;
    b.itemExplodingTimer = 0.0f;
    b.itemExplodingFrame = 0;
    
    // Iniciamos la física de vuelo 3D para el power-up
    b.isFlying = true;
    b.flyTimer = 0.0f;
    b.flyOffset = glm::vec3(0.0f);
    
    float vx = ((float)rand() / (float)RAND_MAX - 0.5f) * 3.0f;
    float vy = 14.0f + ((float)rand() / (float)RAND_MAX) * 6.0f;
    float vz = ((float)rand() / (float)RAND_MAX - 0.5f) * 3.0f;
    b.flyVel = glm::vec3(vx, vy, vz);
    
    b.flyRot = 0.0f;
    b.flyRotVel = 500.0f + ((float)rand() / (float)RAND_MAX) * 1000.0f;

    b.powerUpRevealed = false;
    b.powerUpCollected = true;
}
