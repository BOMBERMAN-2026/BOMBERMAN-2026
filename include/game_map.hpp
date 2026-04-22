#ifndef GAME_MAP_HPP
#define GAME_MAP_HPP

#include <GL/glew.h>
#include <glm/glm.hpp>
#include "player.hpp"
#include "enemy.hpp"
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
 * - Soporta power-ups y enemigos definidos por nivel con directivas `powerup ...` y `enemy ...`.
 *
 * Convención de coordenadas:
 * - Grid interno (código): (row, col) = (y, x), con (0,0) en la esquina superior-izquierda del TXT.
 * - Directivas en `levels/*.txt`: se escriben como (x, y) = (col, row).
 *   - `spawn <player> <x> <y>`
 *   - `powerup <type> <x> <y>`
 *   - `enemy <type> <x> <y>`
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
    bool powerUpPickupFxActive = false; // Efecto visual de brillo al recoger
    float powerUpPickupFxTimer = 0.0f;  // Tiempo acumulado del efecto de recogida

    bool isWalkable() const {
        if (destroyed) return true; // destructible ya destruido → suelo
        // Si está rompiéndose, TODAVÍA NO es caminable (foco de fuego)
        return (type == BlockType::FLOOR);
    }

    bool isDestructible() const {
        return (type == BlockType::DESTRUCTIBLE && !destroyed && !breaking);
    }
};

// Tipo de enemigo declarado en `levels/*.txt` vía directiva `enemy`.
enum class EnemySpawnType {
    Leon,
    Babosa,
    BebeLloron,
    FantasmaMortal,
    SolPervertido,
    KingBomber,
    DronRosa,
    DronVerde,
    DronAmarillo,
    DronAzul,
    DragonJoven
};

// Tipo de dato para mostrar en el HUD
enum class typeOfHud {
    Score,
    Lives,
    EnemiesLeft,
    CurrentLevel,
    Timer,
    NumWins,
    CurrentLevelVS
};

// Spawn de enemigo definido por nivel.
struct EnemySpawn {
    EnemySpawnType type;
    int row = 0;
    int col = 0;
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

    void renderHud(GLuint vao, GLuint uniformModel, GLuint uniformUvRect, GLuint uniformTintColor, SpriteAtlas gScoreboardAtlas, GLuint scoreboardTexture, 
                   std::vector<int>* playerScores, std::vector<Player*>* gPlayers, std::vector<Enemy*>* gEnemies, std::string currentGameLevel, float levelTimeRemaining, uint8_t gamemode, bool showInventory = true);
    
    void renderHudUtils(uint32_t data, glm::vec2 startPos, float scale, 
                        typeOfHud hudType, SpriteAtlas gScoreboardAtlas, GLuint scoreboardTexture, 
                        GLuint vao, GLuint uniformModel, GLuint uniformUvRect);

    int getRows() const { return rows; }
    int getCols() const { return cols; }

    int getSpriteId(int row, int col) const;
    BlockType getBlockType(int row, int col) const;
    bool getUvRectForTile(int row, int col, glm::vec4& uvRect) const;
    bool getUvRectForSpriteId(int spriteId, glm::vec4& uvRect) const;

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

    // Consulta el power-up visible (revelado y no recogido) en una celda.
    bool getVisiblePowerUpType(int row, int col, PowerUpType& outType) const;

    // Consulta el power-up oculto en una celda.
    // Un power-up está "oculto" cuando existe pero todavía no está revelado (p.ej. bajo un destructible intacto).
    // Nota: pensado para IA/dificultades (p.ej. Omniscient).
    bool getHiddenPowerUpType(int row, int col, PowerUpType& outType) const;

    // Consulta si en una celda hay efecto de recogida activo.
    // Devuelve el tipo de power-up y progreso normalizado [0..1].
    bool getPowerUpPickupFx(int row, int col, PowerUpType& outType, float& outNormalizedTime) const;

    // Renderiza el inventario de un jugador en el HUD en una posición concreta.
    // `direction` controla el avance horizontal de iconos: 1.0 (izq->der), -1.0 (der->izq).
    // Muestra un contador "xN" junto a cada icono cuando hay más de uno del mismo tipo.
    void renderPlayerInventory(const Player* player, glm::vec2 startPos, float direction,
                               GLuint vao, GLuint uniformModel, GLuint uniformUvRect, GLuint uniformTintColor,
                               SpriteAtlas gScoreboardAtlas, GLuint scoreboardTexture);

    // Carga las texturas de los power-ups.
    void loadPowerUpTextures();

    // Comprueba si un jugador está sobre un power-up revelado y lo recoge.
    // Devuelve true si recogió un power-up (y lo aplica al player).
    bool tryCollectPowerUp(int row, int col, class Player* player);

    // Si hay un power-up suelto/visible en esa celda, lo destruye (p.ej. por explosión).
    // Regla: NO afecta a power-ups ocultos bajo bloques destructibles intactos.
    void destroyExposedPowerUp(int row, int col);

    // === Enemigos ===
    // Devuelve los spawns de enemigos declarados en el TXT con `enemy <tipo> <x> <y>`.
    const std::vector<EnemySpawn>& getEnemySpawns() const { return enemySpawns; }

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

    // Spawns de enemigos declarados por nivel.
    std::vector<EnemySpawn> enemySpawns;

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

    std::string currentLevel = "5-5";
    std::string currentLevelVS = "2";

    // Convierte el string "type" del atlas JSON a BlockType
    static BlockType blockTypeFromString(const std::string& typeStr);

    // === Power-Ups ===
    static constexpr int POWER_UP_TYPE_COUNT = 12;
    GLuint powerUpTextures[POWER_UP_TYPE_COUNT][2] = {{0}}; // Texturas indexadas por PowerUpType y frame (normal/azul)
    bool powerUpTexturesLoaded = false;
    
    float powerUpAnimTimer = 0.0f;
    int powerUpAnimFrame = 0;
};

#endif // GAME_MAP_HPP
