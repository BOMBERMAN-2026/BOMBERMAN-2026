#include "bomberman.hpp"
#include "player.hpp"
#include "sprite_atlas.hpp"
#include "game_map.hpp"
#include "bomb.hpp"
#include "menu.hpp"
#include "enemies/leon.hpp"
#include "enemies/bebe_lloron.hpp"
#include "enemies/babosa.hpp"
#include "enemies/fantasma_mortal.hpp"
#include "enemies/sol_pervertido.hpp"
#include "enemies/king_bomber.hpp"
#include "enemies/dron_bombardero.hpp"
#include "enemies/dragon_joven.hpp"

/*
 * bomberman.cpp
 * ------------
 * Implementaci├│n del bucle de juego (init/input/update/render) + render OpenGL simple.
 *
 * Funcionalidades principales:
 * - Inicializa OpenGL (VAO/VBO/EBO + shaders) y carga texturas.
 * - Carga el mapa (`GameMap`) y su atlas.
 * - Crea 1 o 2 jugadores seg├║n `GameMode`.
 * - Entrada: Jugador 1 con flechas, Jugador 2 con WASD.
 * - Render: mapa primero, jugadores encima (sprites desde SpriteAtlasPlayer).
 *
 * Nota:
 * - Este archivo es deliberadamente "monol├¡tico" por ahora. Se organizan secciones
 *   para facilitar lectura sin introducir demasiadas clases nuevas.
 */

#define STB_IMAGE_IMPLEMENTATION
#include "external/stb_image.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <string>
#include <cstdlib>
#include <vector>
#include <cmath>
#include <utility>
#include <fstream>
#include <sstream>

static std::vector<Player*> gPlayers;
GameMap* gameMap;
GLuint mapTexture;
GLuint hudTexture;  

// ============================== OpenGL: estado global ==============================

// Global variables for OpenGL
GLuint VAO, VBO, EBO, shader, uniformModel, uniformProjection, uniformTexture, uniformTintColor, uniformUvRect, uniformFlipX, uniformWhiteFlash;

GLuint texture;

// ============================== Shaders (vertex/fragment) ==============================
static const char* kSpriteVertexShaderPath = "shaders/sprite.vs";
static const char* kSpriteFragmentShaderPath = "shaders/sprite.frag";
static const char* kModel3DVertexShaderPath = "shaders/model3D.vs";
static const char* kModel3DFragmentShaderPath = "shaders/model3D.frag";

GLuint cubeVAO = 0;
GLuint cubeVBO = 0;
GLuint cubeEBO = 0;
GLuint shader3D = 0;
GLuint uniform3DModel = 0;
GLuint uniform3DView = 0;
GLuint uniform3DProjection = 0;
GLuint uniform3DColor = 0;

SpriteAtlas gPlayerAtlas; // No est├ítico para usarlo en player.cpp

SpriteAtlas gEnemyAtlas; // No est├ítico para usarlo en enemigos .cpp
GLuint enemyTexture = 0;

SpriteAtlas gBombAtlas; // Atlas para las bombas (misma sprite sheet del stage)

std::vector<Enemy*> gEnemies;
std::vector<Bomb*> gBombs;

static constexpr float kDefaultPlayerSpeed = 0.4f;
static constexpr glm::vec2 kDefaultPlayerSize(0.2f, 0.2f);

static const char* viewModeToString(ViewMode mode) {
    return (mode == ViewMode::Mode3D) ? "3D" : "2D";
}

static const char* camera3DTypeToString(Camera3DType type) {
    switch (type) {
        case Camera3DType::OrthographicFixed: return "OrthographicFixed";
        case Camera3DType::PerspectiveFixed: return "PerspectiveFixed";
        case Camera3DType::PerspectiveMobile: return "PerspectiveMobile";
        case Camera3DType::FirstPerson: return "FirstPerson";
        default: return "Unknown";
    }
}

// ============================== Gameplay: helpers ==============================

// Devuelve true si dos posiciones NDC caen en el mismo tile del mapa.
static bool isSameTile(const GameMap* map, const glm::vec2& a, const glm::vec2& b) {
    if (!map) return false;
    int ar, ac, br, bc;
    map->ndcToGrid(a, ar, ac);
    map->ndcToGrid(b, br, bc);
    return ar == br && ac == bc;
}

// Colisi├│n enemigo-jugador (AABB simple por tile): detecta contacto.
static bool overlapsEnemyPlayer(const GameMap* map, const glm::vec2& enemyPos, const glm::vec2& playerPos) {
    if (!map) return false;
    // AABB simple alrededor del centro del tile. Ajustable.
    const float halfTile = map->getTileSize() / 2.0f;
    const float r = halfTile * 0.70f;
    return (std::abs(enemyPos.x - playerPos.x) <= r) && (std::abs(enemyPos.y - playerPos.y) <= r);
}

// Devuelve true si la caja de colisi├│n de la entidad intersecta con el ├írea de explosi├│n
// calculada de la bomba. Permite detectar da├▒o si est├í parcialmente en la casilla.
static bool explosionHitsEntity(const GameMap* map, const Bomb* bomb, const glm::vec2& entityPos) {
    if (!map || !bomb) return false;
    
    // Asumimos un radio de colisi├│n del 45% del tile para la entidad
    float entityRadius = map->getTileSize() * 0.45f;
    float tileHalf = map->getTileSize() * 0.5f;

    for (const auto& seg : bomb->explosionSegments) {
        // La explosi├│n ocupa casi todo el segmento/tile
        float exMin = seg.pos.x - tileHalf;
        float exMax = seg.pos.x + tileHalf;
        float eyMin = seg.pos.y - tileHalf;
        float eyMax = seg.pos.y + tileHalf;

        // La caja de colisi├│n de la entidad
        float entMinX = entityPos.x - entityRadius;
        float entMaxX = entityPos.x + entityRadius;
        float entMinY = entityPos.y - entityRadius;
        float entMaxY = entityPos.y + entityRadius;

        // Comprobar intersecci├│n AABB (Eje X e Y)
        bool intersectX = (entMaxX > exMin) && (entMinX < exMax);
        bool intersectY = (entMaxY > eyMin) && (entMinY < eyMax);

        if (intersectX && intersectY) {
            return true;
        }
    }
    return false;
}

// ============================== OpenGL: helpers ==============================

static bool readTextFile(const std::string& filePath, std::string& out)
{
    std::ifstream file(filePath.c_str(), std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    out = ss.str();
    return true;
}

void AddShader(GLuint program, const char* shaderCode, GLenum shaderType);

void CreateRectangle()
{
    GLfloat vertices[] = {
        -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, // Bottom-left
         1.0f, -1.0f, 0.0f, 1.0f, 1.0f, // Bottom-right
         1.0f,  1.0f, 0.0f, 1.0f, 0.0f, // Top-right
        -1.0f,  1.0f, 0.0f, 0.0f, 0.0f  // Top-left
    };

    GLuint indices[] = {
        0, 1, 2,
        2, 3, 0
    };

    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glGenBuffers(1, &EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glBindVertexArray(0);
}

void CreateCube()
{
    const GLfloat vertices[] = {
        -0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f,
        -0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f
    };

    const GLuint indices[] = {
        0, 1, 2, 2, 3, 0, // back
        4, 5, 6, 6, 7, 4, // front
        0, 4, 7, 7, 3, 0, // left
        1, 5, 6, 6, 2, 1, // right
        3, 2, 6, 6, 7, 3, // top
        0, 1, 5, 5, 4, 0  // bottom
    };

    glGenVertexArrays(1, &cubeVAO);
    glBindVertexArray(cubeVAO);

    glGenBuffers(1, &cubeVBO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glGenBuffers(1, &cubeEBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void Compile3DShaders()
{
    std::string vertexShaderCode;
    std::string fragmentShaderCode;

    const std::string resolvedVertexPath = resolveAssetPath(kModel3DVertexShaderPath);
    const std::string resolvedFragmentPath = resolveAssetPath(kModel3DFragmentShaderPath);
    if (!readTextFile(resolvedVertexPath, vertexShaderCode))
    {
        std::cerr << "No se pudo leer shader 3D de vertice: " << resolvedVertexPath << "\n";
        return;
    }
    if (!readTextFile(resolvedFragmentPath, fragmentShaderCode))
    {
        std::cerr << "No se pudo leer shader 3D de fragmento: " << resolvedFragmentPath << "\n";
        return;
    }

    shader3D = glCreateProgram();
    if (!shader3D)
    {
        std::cerr << "Error creando programa shader 3D\n";
        return;
    }

    AddShader(shader3D, vertexShaderCode.c_str(), GL_VERTEX_SHADER);
    AddShader(shader3D, fragmentShaderCode.c_str(), GL_FRAGMENT_SHADER);

    GLint result = 0;
    GLchar errorLog[1024] = { 0 };

    glLinkProgram(shader3D);
    glGetProgramiv(shader3D, GL_LINK_STATUS, &result);
    if (!result)
    {
        glGetProgramInfoLog(shader3D, sizeof(errorLog), nullptr, errorLog);
        std::cerr << "Error link shader 3D: '" << errorLog << "'\n";
        return;
    }

    uniform3DModel = glGetUniformLocation(shader3D, "model");
    uniform3DView = glGetUniformLocation(shader3D, "view");
    uniform3DProjection = glGetUniformLocation(shader3D, "projection");
    uniform3DColor = glGetUniformLocation(shader3D, "objectColor");
}

static glm::vec3 gridToWorld3D(const GameMap* map, int row, int col, float y)
{
    const float worldX = (float)col - ((float)map->getCols() * 0.5f) + 0.5f;
    const float worldZ = (float)row - ((float)map->getRows() * 0.5f) + 0.5f;
    return glm::vec3(worldX, y, worldZ);
}

void AddShader(GLuint program, const char* shaderCode, GLenum shaderType)
{
    GLuint shaderObject = glCreateShader(shaderType);
    glShaderSource(shaderObject, 1, &shaderCode, nullptr);
    glCompileShader(shaderObject);

    GLint result = 0;
    GLchar errorLog[1024] = { 0 };
    glGetShaderiv(shaderObject, GL_COMPILE_STATUS, &result);
    if (!result)
    {
        glGetShaderInfoLog(shaderObject, sizeof(errorLog), nullptr, errorLog);
        printf("Error compiling the %d shader: '%s'\n", shaderType, errorLog);
        glDeleteShader(shaderObject);
        return;
    }

    glAttachShader(program, shaderObject);
    glDeleteShader(shaderObject);
}

void CompileShaders()
{
    std::string vertexShaderCode;
    std::string fragmentShaderCode;

    const std::string resolvedVertexPath = resolveAssetPath(kSpriteVertexShaderPath);
    const std::string resolvedFragmentPath = resolveAssetPath(kSpriteFragmentShaderPath);
    if (!readTextFile(resolvedVertexPath, vertexShaderCode))
    {
        std::cerr << "No se pudo leer shader de vertice: " << resolvedVertexPath << "\n";
        return;
    }
    if (!readTextFile(resolvedFragmentPath, fragmentShaderCode))
    {
        std::cerr << "No se pudo leer shader de fragmento: " << resolvedFragmentPath << "\n";
        return;
    }

    shader = glCreateProgram();
    if (!shader)
    {
        printf("Error creating shader program!\n");
        return;
    }

    AddShader(shader, vertexShaderCode.c_str(), GL_VERTEX_SHADER);
    AddShader(shader, fragmentShaderCode.c_str(), GL_FRAGMENT_SHADER);

    GLint result = 0;
    GLchar errorLog[1024] = { 0 };

    glLinkProgram(shader);
    glGetProgramiv(shader, GL_LINK_STATUS, &result);
    if (!result)
    {
        glGetProgramInfoLog(shader, sizeof(errorLog), nullptr, errorLog);
        printf("Error linking program: '%s'\n", errorLog);
        return;
    }

    glValidateProgram(shader);
    glGetProgramiv(shader, GL_VALIDATE_STATUS, &result);
    if (!result)
    {
        glGetProgramInfoLog(shader, sizeof(errorLog), nullptr, errorLog);
        printf("Error validating program: '%s'\n", errorLog);
        return;
    }

    uniformModel = glGetUniformLocation(shader, "model");
    uniformProjection = glGetUniformLocation(shader, "projection");
    uniformTexture = glGetUniformLocation(shader, "ourTexture");
    uniformTintColor = glGetUniformLocation(shader, "tintColor");
    uniformWhiteFlash = glGetUniformLocation(shader, "whiteFlash");
    uniformUvRect = glGetUniformLocation(shader, "uvRect");
    uniformFlipX = glGetUniformLocation(shader, "flipX");
}

GLuint LoadTexture(const char* filePath);

// ============================== Game: run/level helpers ==============================

/*
 * Recursos de render que s├│lo deber├¡an inicializarse una vez por ejecuci├│n:
 * - VAO/VBO/EBO del quad 2D
 * - Programas shader 2D y 3D
 * - Geometr├¡a del cubo para modo 3D
 * Adem├ís activamos blending para sprites con alpha.
 */
void Game::ensureRenderResources() {
    if (renderResourcesInitialized) return;

    CreateRectangle();
    CompileShaders();
    CreateCube();
    Compile3DShaders();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    renderResourcesInitialized = true;
}

// Carga atlas/texturas compartidas que se reutilizan entre niveles (1 vez por ejecuci├│n).
void Game::ensureGameplayAssets() {
    // Evita recargar texturas/atlases en cada `loadLevel()`.
    if (gameplayAssetsLoaded) return;

    // Atlas + textura del jugador
    {
        const std::string atlasPath = resolveAssetPath("resources/sprites/atlases/SpriteAtlasPlayer.json");
        if (!loadSpriteAtlasMinimal(atlasPath, gPlayerAtlas)) {
            std::cerr << "Error cargando atlas: " << atlasPath << std::endl;
            std::exit(EXIT_FAILURE);
        }

        const std::string texturePath = resolveAssetPath(gPlayerAtlas.imagePath);
        texture = LoadTexture(texturePath.c_str());
        if (texture == 0) {
            std::cerr << "Error cargando textura: " << texturePath << std::endl;
            std::exit(EXIT_FAILURE);
        }
    }

    // Atlas + textura de enemigos
    {
        const std::string enemyAtlasPath = resolveAssetPath("resources/sprites/atlases/SpriteAtlasEnemy.json");
        if (!loadSpriteAtlasMinimal(enemyAtlasPath, gEnemyAtlas)) {
            std::cerr << "Error cargando atlas enemigos: " << enemyAtlasPath << std::endl;
            std::exit(EXIT_FAILURE);
        }
        const std::string enemyTexPath = resolveAssetPath(gEnemyAtlas.imagePath);
        enemyTexture = LoadTexture(enemyTexPath.c_str());
        if (enemyTexture == 0) {
            std::cerr << "Error cargando textura enemigos: " << enemyTexPath << std::endl;
            std::exit(EXIT_FAILURE);
        }
    }

    // Textura del mapa + HUD (por ahora fijas)
    {
        const std::string mapTexPath = resolveAssetPath("resources/sprites/mapas/Stage1/sprites-Stage1.png");
        mapTexture = LoadTexture(mapTexPath.c_str());
        if (mapTexture == 0) {
            std::cerr << "Error cargando textura del mapa: " << mapTexPath << std::endl;
            std::exit(EXIT_FAILURE);
        }

        const std::string hudTexPath = resolveAssetPath("resources/sprites/marcadores_bomban.png");
        hudTexture = LoadTexture(hudTexPath.c_str());
        if (hudTexture == 0) {
            std::cerr << "Error cargando textura del HUD: " << hudTexPath << std::endl;
            std::exit(EXIT_FAILURE);
        }
    }

    // Atlas de bombas (misma sprite sheet del stage)
    {
        const std::string bombAtlasPath = resolveAssetPath("resources/sprites/atlases/SpriteAtlasStage1.json");
        if (!loadSpriteAtlasMinimal(bombAtlasPath, gBombAtlas)) {
            std::cerr << "Error cargando atlas bombas: " << bombAtlasPath << std::endl;
        }
    }

    gameplayAssetsLoaded = true;
}

// Limpieza del contenido "din├ímico" de un nivel.
// Orden: bombas -> enemigos -> jugadores.
void Game::cleanupGameplayEntities() {
    for (auto* b : gBombs) delete b;
    gBombs.clear();

    for (auto* e : gEnemies) delete e;
    gEnemies.clear();

    for (auto* p : gPlayers) delete p;
    gPlayers.clear();
}

// Condici├│n de Game Over: todos los jugadores est├ín sin vidas.
bool Game::allPlayersOutOfLives() const {
    if (gPlayers.empty()) return true;
    for (auto* p : gPlayers) {
        if (!p) continue;
        if (p->lives > 0) return false;
    }
    return true;
}

bool Game::allEnemiesCleared() const {
    if (!currentLevelHadEnemies) return false; // El nivel nunca tuvo enemigos
    return gEnemies.empty();
}

// Carga completa de un nivel.
void Game::loadLevel(int levelIndex, bool preserveLivesAndScore) {
    ensureGameplayAssets();
    // Guardar progreso a preservar.
    std::vector<int> savedLives;
    if (preserveLivesAndScore) {
        // Conserva vidas y puntuaci├│n acumulada.
        savedLives.reserve(gPlayers.size());
        for (auto* p : gPlayers) {
            savedLives.push_back(p ? p->lives : 0);
        }
    }

    // Limpiar entidades del nivel anterior.
    cleanupGameplayEntities();

    // Reset de transici├│n de nivel (para no arrastrar timers entre niveles).
    pendingLevelAdvance = false;
    levelAdvanceTimer = 0.0f;

    // Cargar / recargar mapa.
    if (!gameMap) gameMap = new GameMap();

    if (levelIndex < 0 || levelIndex >= (int)levelSequence.size()) {
        // Seguridad: si algo fuerza un ├¡ndice inv├ílido, terminamos run y volvemos al men├║.
        std::cerr << "Nivel fuera de rango: " << levelIndex << std::endl;
        returnToMenuFromGame(/*resetRun=*/true);
        return;
    }

    if (!gameMap->loadFromFile(levelSequence[levelIndex])) {
        std::cerr << "Error cargando mapa: " << levelSequence[levelIndex] << std::endl;
        std::exit(EXIT_FAILURE);
    }

    if (!gameMap->loadAtlas("resources/sprites/atlases/SpriteAtlasStage1.json")) {
        std::cerr << "Error cargando atlas del mapa" << std::endl;
        std::exit(EXIT_FAILURE);
    }

    float aspectRatio = (float)WIDTH / (float)HEIGHT;
    gameMap->calculateTileMetrics(aspectRatio);

    // Crear jugadores seg├║n el modo.
    const int numPlayers = (mode == GameMode::TwoPlayers) ? 2 : 1;
    if (!preserveLivesAndScore) {
        playerScores.assign(numPlayers, 0);
    } else {
        if ((int)playerScores.size() < numPlayers) playerScores.resize(numPlayers, 0);
    }

    gPlayers.reserve(numPlayers);
    for (int i = 0; i < numPlayers; ++i) {
        glm::vec2 spawnPos = gameMap->getSpawnPosition(i);
        const std::string prefix = (i == 0) ? "jugadorblanco" : "jugadorrojo";
        Player* p = new Player(spawnPos, kDefaultPlayerSize, kDefaultPlayerSpeed, /*playerId=*/i, prefix);
        if (preserveLivesAndScore && i < (int)savedLives.size()) {
            p->lives = savedLives[i];
        }
        gPlayers.push_back(p);
    }

    // Crear enemigos seg├║n el nivel.
    // La lista de spawns viene del TXT (enemy <type> <x> <y>).
    {
        const auto& spawns = gameMap->getEnemySpawns();
        for (const auto& s : spawns) {
            glm::vec2 pos = gameMap->gridToNDC(s.row, s.col);
            if (!gameMap->isWalkable(s.row, s.col)) {
                std::cerr << "Enemy spawn no walkable (row=" << s.row << ", col=" << s.col << ")\n";
                continue;
            }

            Enemy* enemy = nullptr;
            switch (s.type) {
                case EnemySpawnType::Leon: {
                    auto* e = new Leon(pos, kDefaultPlayerSize, /*speed=*/0.10f);
                    e->currentSpriteName = "leon.abajo.0";
                    enemy = e;
                    break;
                }
                case EnemySpawnType::Babosa: {
                    auto* e = new Babosa(pos, kDefaultPlayerSize, /*speed=*/0.06f);
                    e->currentSpriteName = "babosa.abajo.0";
                    enemy = e;
                    break;
                }
                case EnemySpawnType::BebeLloron: {
                    auto* e = new BebeLloron(pos, kDefaultPlayerSize, /*speed=*/0.08f);
                    e->currentSpriteName = "bebe.derecha.0";
                    enemy = e;
                    break;
                }
                case EnemySpawnType::FantasmaMortal: {
                    auto* e = new FantasmaMortal(pos, kDefaultPlayerSize, /*speed=*/0.11f);
                    e->currentSpriteName = "fantasma.derecha.0";
                    enemy = e;
                    break;
                }
                case EnemySpawnType::SolPervertido: {
                    auto* e = new SolPervertido(pos, kDefaultPlayerSize, /*speed=*/0.07f);
                    e->currentSpriteName = "sol.grande.0";
                    enemy = e;
                    break;
                }
                case EnemySpawnType::KingBomber: {
                    auto* e = new KingBomber(pos, kDefaultPlayerSize, /*speed=*/0.07f);
                    e->currentSpriteName = "kingbomber1.abajo.0";
                    enemy = e;
                    break;
                }
                case EnemySpawnType::DronRosa: {
                    auto* e = new DronBombardero(pos, kDefaultPlayerSize, /*speed=*/0.09f);
                    e->currentSpriteName = "dronrosa.abajo.0";
                    enemy = e;
                    break;
                }
                case EnemySpawnType::DronVerde: {
                    auto* e = new DronBombardero(pos, kDefaultPlayerSize, /*speed=*/0.09f);
                    e->currentSpriteName = "dronverde.abajo.0";
                    enemy = e;
                    break;
                }
                case EnemySpawnType::DronAmarillo: {
                    auto* e = new DronBombardero(pos, kDefaultPlayerSize, /*speed=*/0.09f);
                    e->currentSpriteName = "dronamarillo.abajo.0";
                    enemy = e;
                    break;
                }
                case EnemySpawnType::DronAzul: {
                    auto* e = new DronBombardero(pos, kDefaultPlayerSize, /*speed=*/0.09f);
                    e->currentSpriteName = "dronazul.abajo.0";
                    enemy = e;
                    break;
                }
                case EnemySpawnType::DragonJoven: {
                    auto* e = new DragonJoven(pos, kDefaultPlayerSize, /*speed=*/0.07f);
                    e->currentSpriteName = "dragon.abajo.0";
                    enemy = e;
                    break;
                }
            }

            if (!enemy) continue;
            enemy->setContext(gameMap, &gPlayers);
            gEnemies.push_back(enemy);
        }
    }

    currentLevelHadEnemies = !gEnemies.empty();

    // Power-Ups (texturas: se cargan una vez por instancia de GameMap)
    gameMap->loadPowerUpTextures();
    gameMap->placePowerUps();
}

// Arranca una partida nueva desde nivel_01 (├¡ndice 0).
void Game::startNewRun(GameMode newMode) {
    mode = newMode;
    currentLevelIndex = 0;
    currentLevelHadEnemies = false;
    playerScores.clear();

    state = GAME_PLAYING;
    loadLevel(currentLevelIndex, /*preserveLivesAndScore=*/false);

    // Por si el men├║ dej├│ la marca de transici├│n activa.
    menuScreen.resetTransition();
}

// Avanza al siguiente nivel (si existe) preservando el progreso definido en `loadLevel`.
void Game::advanceToNextLevel() {
    const int nextIndex = currentLevelIndex + 1;
    if (nextIndex >= (int)levelSequence.size()) {
        // No hay ranking ni pantalla de victoria: volver al men├║.
        returnToMenuFromGame(/*resetRun=*/true);
        return;
    }

    currentLevelIndex = nextIndex;
    loadLevel(currentLevelIndex, /*preserveLivesAndScore=*/true);
}

// Sale a men├║ desde gameplay (Game Over / fin de campa├▒a).
void Game::returnToMenuFromGame(bool resetRun) {
    cleanupGameplayEntities();
    pendingLevelAdvance = false;
    levelAdvanceTimer = 0.0f;
    if (resetRun) {
        currentLevelIndex = 0;
        currentLevelHadEnemies = false;
        playerScores.clear();
    }
    state = GAME_MENU;
    init();
}

Game::~Game() {
    cleanupGameplayEntities();
    // Liberar gameMap para evitar fugas.
    if (gameMap) {
        delete gameMap;
        gameMap = nullptr;
    }
}

GLuint LoadTexture(const char* filePath)
{
    GLuint textureID;
    int width, height, nrChannels;
    unsigned char* data = stbi_load(filePath, &width, &height, &nrChannels, STBI_rgb_alpha);
    if (!data)
    {
        printf("Failed to load texture\n");
        return 0;
    }

    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    stbi_image_free(data);
    return textureID;
}

// ============================== Utilidades (debug/keys) ==============================


// Convierte un c├│digo de tecla GLFW en una etiqueta corta (utilidad de depuraci├│n).
static std::string getKeyName(GLint key){
    std::string str;
    switch(key) {
        case GLFW_KEY_UNKNOWN: str = "UNK";
        break;
        case GLFW_KEY_SPACE: str = "SPACE";
        break;
        case GLFW_KEY_APOSTROPHE: str = "APOS";
        break;
        case GLFW_KEY_COMMA: str = ",";
        break;
        case GLFW_KEY_MINUS: str = "-";
        break;
        case GLFW_KEY_PERIOD: str = "PER";
        break;
        case GLFW_KEY_SLASH: str = "/";
        break;
        case GLFW_KEY_0: str = "0";
        break;
        case GLFW_KEY_1: str = "1";
        break;
        case GLFW_KEY_2: str = "2";
        break;
        case GLFW_KEY_3: str = "3";
        break;
        case GLFW_KEY_4: str = "4";
        break;
        case GLFW_KEY_5: str = "5";
        break;
        case GLFW_KEY_6: str = "6";
        break;
        case GLFW_KEY_7: str = "7";
        break;
        case GLFW_KEY_8: str = "8";
        break;
        case GLFW_KEY_9: str = "9";
        break;
        case GLFW_KEY_SEMICOLON: str = "'";
        break;
        case GLFW_KEY_EQUAL: str = "=";
        break;
        case GLFW_KEY_A: str = "A";
        break;
        case GLFW_KEY_B: str = "B";
        break;
        case GLFW_KEY_C: str = "C";
        break;
        case GLFW_KEY_D: str = "D";
        break;
        case GLFW_KEY_E: str = "E";
        break;
        case GLFW_KEY_F: str = "F";
        break;
        case GLFW_KEY_G: str = "G";
        break;
        case GLFW_KEY_H: str = "H";
        break;
        case GLFW_KEY_I: str = "I";
        break;
        case GLFW_KEY_J: str = "J";
        break;
        case GLFW_KEY_K: str = "K";
        break;
        case GLFW_KEY_L: str = "L";
        break;
        case GLFW_KEY_M: str = "M";
        break;
        case GLFW_KEY_N: str = "N";
        break;
        case GLFW_KEY_O: str = "O";
        break;
        case GLFW_KEY_P: str = "P";
        break;
        case GLFW_KEY_Q: str = "Q";
        break;
        case GLFW_KEY_R: str = "R";
        break;
        case GLFW_KEY_S: str = "S";
        break;
        case GLFW_KEY_T: str = "T";
        break;
        case GLFW_KEY_U: str = "U";
        break;
        case GLFW_KEY_V: str = "V";
        break;
        case GLFW_KEY_W: str = "W";
        break;
        case GLFW_KEY_X: str = "X";
        break;
        case GLFW_KEY_Y: str = "Y";
        break;
        case GLFW_KEY_Z: str = "Z";
        break;
        case GLFW_KEY_LEFT_BRACKET: str = "{";
        break;
        case GLFW_KEY_BACKSLASH: str = "\\";
        break;
        case GLFW_KEY_RIGHT_BRACKET: str = "}";
        break;
        case GLFW_KEY_GRAVE_ACCENT: str = "`";
        break;
        case GLFW_KEY_WORLD_1: str = "W1";
        break;
        case GLFW_KEY_WORLD_2: str = "W2";
        break;
        case GLFW_KEY_ESCAPE: str = "ESC";
        break;
        case GLFW_KEY_ENTER: str = "ENTER";
        break;
        case GLFW_KEY_TAB: str = "TAB";
        break;
        case GLFW_KEY_BACKSPACE: str = "BS";
        break;
        case GLFW_KEY_INSERT: str = "INS";
        break;
        case GLFW_KEY_DELETE: str = "DEL";
        break;
        case GLFW_KEY_RIGHT: str = "RIGHT";
        break;
        case GLFW_KEY_LEFT: str = "LEFT";
        break;
        case GLFW_KEY_DOWN: str = "DOWN";
        break;
        case GLFW_KEY_UP: str = "UP";
        break;
        case GLFW_KEY_PAGE_UP: str = "PAG.UP";
        break;
        case GLFW_KEY_PAGE_DOWN: str = "PAG.DOWN";
        break;
        case GLFW_KEY_HOME: str = "HOME";
        break;
        case GLFW_KEY_END: str = "END";
        break;
        case GLFW_KEY_CAPS_LOCK: str = "C.LOCK";
        break;
        case GLFW_KEY_SCROLL_LOCK: str = "S.LOCK";
        break;
        case GLFW_KEY_NUM_LOCK: str = "N.LOCK";
        break;
        case GLFW_KEY_PRINT_SCREEN: str = "IMPR";
        break;
        case GLFW_KEY_PAUSE: str = "PAUSE";
        break;
        case GLFW_KEY_F1: str = "F1";
        break;
        case GLFW_KEY_F2: str = "F2";
        break;
        case GLFW_KEY_F3: str = "F3";
        break;
        case GLFW_KEY_F4: str = "F4";
        break;
        case GLFW_KEY_F5: str = "F5";
        break;
        case GLFW_KEY_F6: str = "F6";
        break;
        case GLFW_KEY_F7: str = "F7";
        break;
        case GLFW_KEY_F8: str = "F8";
        break;
        case GLFW_KEY_F9: str = "F9";
        break;
        case GLFW_KEY_F10: str = "F10";
        break;
        case GLFW_KEY_F11: str = "F11";
        break;
        case GLFW_KEY_F12: str = "F12";
        break;
        case GLFW_KEY_F13: str = "F13";
        break;
        case GLFW_KEY_F14: str = "F14";
        break;
        case GLFW_KEY_F15: str = "F15";
        break;
        case GLFW_KEY_F16: str = "F16";
        break;
        case GLFW_KEY_F17: str = "F17";
        break;
        case GLFW_KEY_F18: str = "F18";
        break;
        case GLFW_KEY_F19: str = "F19";
        break;
        case GLFW_KEY_F20: str = "F20";
        break;
        case GLFW_KEY_F21: str = "F21";
        break;
        case GLFW_KEY_F22: str = "F22";
        break;
        case GLFW_KEY_F23: str = "F23";
        break;
        case GLFW_KEY_F24: str = "F24";
        break;
        case GLFW_KEY_F25: str = "F25";
        break;
        case GLFW_KEY_KP_0: str = "KP0";
        break;
        case GLFW_KEY_KP_1: str = "KP1";
        break;
        case GLFW_KEY_KP_2: str = "KP2";
        break;
        case GLFW_KEY_KP_3: str = "KP3";
        break;
        case GLFW_KEY_KP_4: str = "KP4";
        break;
        case GLFW_KEY_KP_5: str = "KP5";
        break;
        case GLFW_KEY_KP_6: str = "KP6";
        break;
        case GLFW_KEY_KP_7: str = "KP7";
        break;
        case GLFW_KEY_KP_8: str = "KP8";
        break;
        case GLFW_KEY_KP_9: str = "KP9";
        break;
        case GLFW_KEY_KP_DECIMAL: str = "KPDEC";
        break;
        case GLFW_KEY_KP_DIVIDE: str = "KPDIV";
        break;
        case GLFW_KEY_KP_MULTIPLY: str = "KPMUL";
        break;
        case GLFW_KEY_KP_SUBTRACT: str = "KPSUB";
        break;
        case GLFW_KEY_KP_ADD: str = "KPADD";
        break;
        case GLFW_KEY_KP_ENTER: str = "KPENTER";
        break;
        case GLFW_KEY_KP_EQUAL: str = "KPEQ";
        break;
        case GLFW_KEY_LEFT_SHIFT: str = "SHIFT";
        break;
        case GLFW_KEY_LEFT_CONTROL: str = "CTRL";
        break;
        case GLFW_KEY_LEFT_ALT: str = "ALT";
        break;
        case GLFW_KEY_LEFT_SUPER: str = "SUPER";
        break;
        case GLFW_KEY_RIGHT_SHIFT: str = "RSHIFT";
        break;
        case GLFW_KEY_RIGHT_CONTROL: str = "RCTRL";
        break;
        case GLFW_KEY_RIGHT_ALT: str = "RALT";
        break;
        case GLFW_KEY_RIGHT_SUPER: str = "RSUPER";
        break;
        case GLFW_KEY_MENU: str = "MENU";
        break;
    }
    return str;
}

// ============================== Game lifecycle ==============================

void Game::toggleViewMode() {
    viewMode = (viewMode == ViewMode::Mode2D) ? ViewMode::Mode3D : ViewMode::Mode2D;
    std::cout << "[Render] View mode -> " << viewModeToString(viewMode) << "\n";
}

void Game::cycleCamera3DType() {
    switch (camera3DType) {
        case Camera3DType::OrthographicFixed:
            camera3DType = Camera3DType::PerspectiveFixed;
            break;
        case Camera3DType::PerspectiveFixed:
            camera3DType = Camera3DType::PerspectiveMobile;
            break;
        case Camera3DType::PerspectiveMobile:
            camera3DType = Camera3DType::FirstPerson;
            break;
        case Camera3DType::FirstPerson:
            camera3DType = Camera3DType::OrthographicFixed;
            break;
    }
    std::cout << "[Render] 3D camera -> " << camera3DTypeToString(camera3DType) << "\n";
}

void Game::init() {

    ensureRenderResources();

    // ========== INTRO ==========
    if (this->state == GAME_INTRO) {
        // Reproducir cinematica antes de empezar la partida
        this->state = GAME_CINEMATIC;
        this->currentCinematicType = CinematicType::Intro;
        this->nextStateAfterCinematic = GAME_MENU;
        std::string videoPath = resolveAssetPath("resources/video/Intro.mp4");
        cinematicPlayer.open(videoPath);
        return;
    }

    // ========== MENU ==========
    if (this->state == GAME_MENU) {
        menuScreen.initMenu();
        return;
    }

    // ========== JUEGO ==========
    if (this->state == GAME_PLAYING) {
        // Iniciar / recargar el nivel actual.
        loadLevel(currentLevelIndex, /*preserveLivesAndScore=*/false);
    }
}

// Lee teclas y aplica acciones (movimiento, animaci├│n y colocar bombas).
void Game::processInput() {
    // Alternar fullscreen.
    if (this->keys[GLFW_KEY_TAB] == GLFW_PRESS) {
        this->keys[GLFW_KEY_TAB] = GLFW_REPEAT; // Evitar m├║ltiples toggles por pulsaci├│n
        toggleFullscreen(this->window);
    }

    // ========== INTRO ==========
    if (this->state == GAME_INTRO) {
        return; // No procesamos input en intro (ver callback)
    }

    // ========== CINEMATICA ==========
    if (this->state == GAME_CINEMATIC) {
        if (this->keys[GLFW_KEY_SPACE] == GLFW_PRESS) {
            this->keys[GLFW_KEY_SPACE] = GLFW_REPEAT;
            cinematicPlayer.skip();
        }
        return;
    }

    // ========== MENU ==========
    if (this->state == GAME_MENU) {
        menuScreen.processInputMenu(this->keys);
        return;
    }

    // ========== JUEGO NORMAL ==========
    if (this->state == GAME_PLAYING) {
        // F1: alternar vista 2D/3D.
        if (this->keys[GLFW_KEY_F1] == GLFW_PRESS) {
            this->keys[GLFW_KEY_F1] = GLFW_REPEAT;
            toggleViewMode();
        }
        // F2: cambiar tipo de c├ímara 3D.
        if (this->keys[GLFW_KEY_F2] == GLFW_PRESS) {
            this->keys[GLFW_KEY_F2] = GLFW_REPEAT;
            cycleCamera3DType();
        }

        // F3: forzar el salto al siguiente nivel.
        if (this->keys[GLFW_KEY_F3] == GLFW_PRESS) {
            this->keys[GLFW_KEY_F3] = GLFW_REPEAT; // acci├│n de una sola pulsaci├│n
            std::cout << "[Debug] Forzando avance de nivel" << std::endl;
            advanceToNextLevel();
            return;
        }

        if (gPlayers.empty() || gPlayers[0] == nullptr) return;
        Player* p1 = gPlayers[0];

        // ======================= Jugador 1 =======================

        if (p1->isAlive()) {
            const bool up = (this->keys[GLFW_KEY_UP] >= GLFW_PRESS);
            const bool down = (this->keys[GLFW_KEY_DOWN] >= GLFW_PRESS);
            const bool left = (this->keys[GLFW_KEY_LEFT] >= GLFW_PRESS);
            const bool right = (this->keys[GLFW_KEY_RIGHT] >= GLFW_PRESS);

            const int pressedCount = (up ? 1 : 0) + (down ? 1 : 0) + (left ? 1 : 0) + (right ? 1 : 0);
            if (pressedCount == 0) {
                p1->isWalking = false;

                if (this->lastDirKey != GLFW_KEY_UNKNOWN) {
                    p1->facingDirKey = this->lastDirKey;
                }
            } else {
                GLint keyToUse = GLFW_KEY_UNKNOWN;
                if (pressedCount == 1) {
                    if (up) keyToUse = GLFW_KEY_UP;
                    if (down) keyToUse = GLFW_KEY_DOWN;
                    if (left) keyToUse = GLFW_KEY_LEFT;
                    if (right) keyToUse = GLFW_KEY_RIGHT;
                    this->lastDirKey = keyToUse;
                } else {
                    switch (this->lastDirKey) {
                        case GLFW_KEY_UP: if (up) keyToUse = GLFW_KEY_UP; break;
                        case GLFW_KEY_DOWN: if (down) keyToUse = GLFW_KEY_DOWN; break;
                        case GLFW_KEY_LEFT: if (left) keyToUse = GLFW_KEY_LEFT; break;
                        case GLFW_KEY_RIGHT: if (right) keyToUse = GLFW_KEY_RIGHT; break;
                    }
                    if (keyToUse == GLFW_KEY_UNKNOWN) return;
                }

                switch (keyToUse) {
                    case GLFW_KEY_UP:
                        p1->UpdateSprite(MOVE_UP, gameMap, this->deltaTime);
                        if (!p1->isWalking || p1->facingDirKey != GLFW_KEY_UP) {
                            p1->walkTimer = 0.0f; p1->walkPhase = 0;
                        }
                        p1->facingDirKey = GLFW_KEY_UP;
                        break;
                    case GLFW_KEY_DOWN:
                        p1->UpdateSprite(MOVE_DOWN, gameMap, this->deltaTime);
                        if (!p1->isWalking || p1->facingDirKey != GLFW_KEY_DOWN) {
                            p1->walkTimer = 0.0f; p1->walkPhase = 0;
                        }
                        p1->facingDirKey = GLFW_KEY_DOWN;
                        break;
                    case GLFW_KEY_LEFT:
                        p1->UpdateSprite(MOVE_LEFT, gameMap, this->deltaTime);
                        if (!p1->isWalking || p1->facingDirKey != GLFW_KEY_LEFT) {
                            p1->walkTimer = 0.0f; p1->walkPhase = 0;
                        }
                        p1->facingDirKey = GLFW_KEY_LEFT;
                        break;
                    case GLFW_KEY_RIGHT:
                        p1->UpdateSprite(MOVE_RIGHT, gameMap, this->deltaTime);
                        if (!p1->isWalking || p1->facingDirKey != GLFW_KEY_RIGHT) {
                            p1->walkTimer = 0.0f; p1->walkPhase = 0;
                        }
                        p1->facingDirKey = GLFW_KEY_RIGHT;
                        break;
                }

                p1->isWalking = true;
            }
        } else {
            p1->isWalking = false;
        }

        // ======================= Jugador 2 =======================
        if (this->mode == GameMode::TwoPlayers && gPlayers.size() >= 2 && gPlayers[1] != nullptr) {
            Player* p2 = gPlayers[1];

            if (!p2->isAlive()) {
                p2->isWalking = false;
            } else {

                const bool up2 = (this->keys[GLFW_KEY_W] >= GLFW_PRESS);
                const bool down2 = (this->keys[GLFW_KEY_S] >= GLFW_PRESS);
                const bool left2 = (this->keys[GLFW_KEY_A] >= GLFW_PRESS);
                const bool right2 = (this->keys[GLFW_KEY_D] >= GLFW_PRESS);

                const int pressedCount2 = (up2 ? 1 : 0) + (down2 ? 1 : 0) + (left2 ? 1 : 0) + (right2 ? 1 : 0);
                if (pressedCount2 == 0) {
                    p2->isWalking = false;

                    if (this->lastDirKeyP2 != GLFW_KEY_UNKNOWN) {
                        switch (this->lastDirKeyP2) {
                            case GLFW_KEY_W: p2->facingDirKey = GLFW_KEY_UP; break;
                            case GLFW_KEY_S: p2->facingDirKey = GLFW_KEY_DOWN; break;
                            case GLFW_KEY_A: p2->facingDirKey = GLFW_KEY_LEFT; break;
                            case GLFW_KEY_D: p2->facingDirKey = GLFW_KEY_RIGHT; break;
                        }
                    }
                } else {
                    GLint keyToUse2 = GLFW_KEY_UNKNOWN;
                    if (pressedCount2 == 1) {
                        if (up2) keyToUse2 = GLFW_KEY_W;
                        if (down2) keyToUse2 = GLFW_KEY_S;
                        if (left2) keyToUse2 = GLFW_KEY_A;
                        if (right2) keyToUse2 = GLFW_KEY_D;
                        this->lastDirKeyP2 = keyToUse2;
                    } else {
                        switch (this->lastDirKeyP2) {
                            case GLFW_KEY_W: if (up2) keyToUse2 = GLFW_KEY_W; break;
                            case GLFW_KEY_S: if (down2) keyToUse2 = GLFW_KEY_S; break;
                            case GLFW_KEY_A: if (left2) keyToUse2 = GLFW_KEY_A; break;
                            case GLFW_KEY_D: if (right2) keyToUse2 = GLFW_KEY_D; break;
                        }
                        if (keyToUse2 == GLFW_KEY_UNKNOWN) return;
                    }

                    GLint dir2 = GLFW_KEY_DOWN;
                    Move mov2 = MOVE_NONE;
                    switch (keyToUse2) {
                        case GLFW_KEY_W: dir2 = GLFW_KEY_UP; mov2 = MOVE_UP; break;
                        case GLFW_KEY_S: dir2 = GLFW_KEY_DOWN; mov2 = MOVE_DOWN; break;
                        case GLFW_KEY_A: dir2 = GLFW_KEY_LEFT; mov2 = MOVE_LEFT; break;
                        case GLFW_KEY_D: dir2 = GLFW_KEY_RIGHT; mov2 = MOVE_RIGHT; break;
                    }

                    p2->UpdateSprite(mov2, gameMap, this->deltaTime);
                    if (!p2->isWalking || p2->facingDirKey != dir2) {
                        p2->walkTimer = 0.0f;
                        p2->walkPhase = 0;
                    }
                    p2->facingDirKey = dir2;
                    p2->isWalking = true;
                }
            }
        }

        // ======================= Bombas (Bot├│n 1) =======================
        if (p1->isAlive() && !p1->isGameOver() && this->keys[GLFW_KEY_RIGHT_CONTROL] == GLFW_PRESS) {
            this->keys[GLFW_KEY_RIGHT_CONTROL] = GLFW_REPEAT; // Evitar m├║ltiples bombas por pulsaci├│n

            if (p1->canPlaceBomb()) {
                int bombRow, bombCol;
                gameMap->ndcToGrid(p1->position, bombRow, bombCol);

                // Comprobar que no hay ya una bomba en este tile
                bool alreadyHasBomb = false;
                for (auto* b : gBombs) {
                    if (b->state != BombState::DONE && b->gridRow == bombRow && b->gridCol == bombCol) {
                        alreadyHasBomb = true;
                        break;
                    }
                }

                if (!alreadyHasBomb) {
                    glm::vec2 tileCenter = gameMap->gridToNDC(bombRow, bombCol);
                    Bomb* bomb = new Bomb(tileCenter, bombRow, bombCol,
                                        /*owner=*/p1,
                                        /*power=*/p1->explosionPower,
                                        /*remote=*/p1->hasRemoteControl);
                    gBombs.push_back(bomb);
                    p1->activeBombs++;
                }
            }
        }

        // Detonar (Bot├│n 2)
        if (p1->isAlive() && p1->hasRemoteControl && this->keys[GLFW_KEY_RIGHT_ALT] == GLFW_PRESS) {
            this->keys[GLFW_KEY_RIGHT_ALT] = GLFW_REPEAT;
            // Detonar la bomba M├üS ANTIGUA del jugador (una por una, estilo Arcade)
            for (auto* b : gBombs) {
                if (b && b->ownerIndex == p1->playerId && b->state == BombState::FUSE) {
                    b->detonate();
                    break; // Solo una por pulsaci├│n
                }
            }
        }

        // Bombas / detonar del jugador 2.
        if (this->mode == GameMode::TwoPlayers && gPlayers.size() >= 2 && gPlayers[1] != nullptr) {
            Player* p2 = gPlayers[1];
            if (p2->isAlive() && !p2->isGameOver() && this->keys[GLFW_KEY_X] == GLFW_PRESS) {
                this->keys[GLFW_KEY_X] = GLFW_REPEAT;

                if (p2->canPlaceBomb()) {
                    int bombRow, bombCol;
                    gameMap->ndcToGrid(p2->position, bombRow, bombCol);

                    bool alreadyHasBomb = false;
                    for (auto* b : gBombs) {
                        if (b->state != BombState::DONE && b->gridRow == bombRow && b->gridCol == bombCol) {
                            alreadyHasBomb = true;
                            break;
                        }
                    }

                    if (!alreadyHasBomb) {
                        glm::vec2 tileCenter = gameMap->gridToNDC(bombRow, bombCol);
                        Bomb* bomb = new Bomb(tileCenter, bombRow, bombCol,
                                            /*owner=*/p2,
                                            /*power=*/p2->explosionPower,
                                            /*remote=*/p2->hasRemoteControl);
                        gBombs.push_back(bomb);
                        p2->activeBombs++;
                    }
                }
            }

            // Detonar (Bot├│n 2)
            if (p2->isAlive() && p2->hasRemoteControl && this->keys[GLFW_KEY_Z] == GLFW_PRESS) {
                this->keys[GLFW_KEY_Z] = GLFW_REPEAT;
                for (auto* b : gBombs) {
                    if (b && b->ownerIndex == p2->playerId && b->state == BombState::FUSE) {
                        b->detonate();
                        break;
                    }
                }
            }
        }
    }
}

// Tick de l├│gica: mapa, enemigos, bombas (da├▒o) y contacto enemigo-jugador.
void Game::update() {
    float deltaTime = this->deltaTime;

    // ========== MENU ==========
    if (this->state == GAME_MENU) {
        menuScreen.updateMenu(deltaTime);
        if (menuScreen.shouldStartGame()) {
            GameMode selectedMode = menuScreen.getSelectedMode();
            if (selectedMode == GameMode::TwoPlayers) {
                // Reproducir cinematica antes de empezar la partida
                this->mode = selectedMode;
                this->state = GAME_CINEMATIC;
                this->currentCinematicType = CinematicType::HistoryStart;
                this->nextStateAfterCinematic = GAME_PLAYING;
                std::string videoPath = resolveAssetPath("resources/video/HistoryIntro.mp4");
                cinematicPlayer.open(videoPath);
                menuScreen.resetTransition();
            } else {
                startNewRun(selectedMode);
            }
        }
        return;
    }

    // ========== CINEMATICA ==========
    if (this->state == GAME_CINEMATIC) {
        cinematicPlayer.update(deltaTime);
        if (cinematicPlayer.isFinished()) {
            cinematicPlayer.close();
            if (currentCinematicType == CinematicType::Intro) {
                this->state = GAME_MENU;
                this->init();
            } else if (currentCinematicType == CinematicType::HistoryStart) {
                startNewRun(mode); // Actualiza el state a GAME_PLAYING e inicia la partida con el modo seleccionado previamente.
            }
        }
        return;
    }

    // ========== RESTO DEL JUEGO ==========
    if (gameMap) {
        gameMap->update(deltaTime);
    }

    // Actualizar enemigos (l├│gica o animaci├│n de muerte)
    const std::size_t enemiesToUpdate = gEnemies.size();
    for (std::size_t enemyIndex = 0; enemyIndex < enemiesToUpdate; ++enemyIndex) {
        Enemy* enemy = gEnemies[enemyIndex];
        if (!enemy) continue;
        enemy->setDeltaTime(deltaTime);

        if (enemy->lifeState == EnemyLifeState::Alive) {
    
        // Notificar si hay bombas cercanas
        for (auto* b : gBombs) {
            float dist = glm::distance(enemy->position, b->position);
            if (gameMap && dist < gameMap->getTileSize() * 2.5f) {
                enemy->notifyBombNearby(b->position);
            }
        }
        
        enemy->Update();
        } else if (enemy->lifeState == EnemyLifeState::Dying) {
            enemy->updateDeath(deltaTime);
        }
    }

    // Limpiar enemigos que ya terminaron de morir
    for (auto it = gEnemies.begin(); it != gEnemies.end(); ) {
        Enemy* e = *it;
        if (e && e->lifeState == EnemyLifeState::Dead) {
            delete e;
            it = gEnemies.erase(it);
        } else {
            ++it;
        }
    }

    // Actualizar jugador
    for (auto* p : gPlayers) {
        if (!p) continue;
        const PlayerLifeState prevLifeState = p->lifeState;
        p->deltaTime = deltaTime;
        p->Update();

        // Si acaba de respawnear:
        // - El Detonator se pierde al morir (ver Player::respawn).
        // - Para evitar bombas "atascadas" (remoteControlled=true sin posibilidad de detonarlas),
        //   convertimos sus bombas remotas a mecha normal.
        if (prevLifeState != PlayerLifeState::Alive && p->lifeState == PlayerLifeState::Alive && !p->hasRemoteControl) {
            for (auto* b : gBombs) {
                if (!b) continue;
                if (b->state == BombState::FUSE && b->ownerIndex == p->playerId && b->remoteControlled) {
                    b->remoteControlled = false;
                }
            }
        }

        // Comprobar si el jugador recoge un power-up
        if (p->isAlive() && gameMap) {
            int pr, pc;
            gameMap->ndcToGrid(p->position, pr, pc);
            gameMap->tryCollectPowerUp(pr, pc, p);
        }
    }

    // Actualizar bombas + aplicar da├▒o por explosi├│n
    for (auto it = gBombs.begin(); it != gBombs.end(); ) {
        Bomb* b = *it;

        // Marcar si el due├▒o ya abandon├│ la casilla (entonces bloquea tambi├®n para ├®l)
        if (b && !b->ownerLeftTile) {
            Player* owner = nullptr;
            if (b->ownerIndex >= 0 && (std::size_t)b->ownerIndex < gPlayers.size()) {
                owner = gPlayers[b->ownerIndex];
            }
            if (!owner || !owner->isAlive() || !isSameTile(gameMap, owner->position, b->position)) {
                b->ownerLeftTile = true;
            }
        }

        bool justExploded = b->Update(deltaTime);

        if (b->state == BombState::EXPLODING) {
            // Explosi├│n: si el fuego toca a una entidad, muere.
            for (auto* p : gPlayers) {
                if (!p || !p->isAlive()) continue;
                if (explosionHitsEntity(gameMap, b, p->position)) {
                    p->killByExplosion();
                }
            }
            for (auto* enemy : gEnemies) {
                if (!enemy || enemy->lifeState != EnemyLifeState::Alive) continue;
                if (explosionHitsEntity(gameMap, b, enemy->position)) {
                    if (enemy->takeDamage(gEnemyAtlas, 999)) {
                        // Puntuaci├│n: s├│lo suma una vez cuando el enemigo pasa de Alive -> Dying.
                        // `takeDamage` devuelve true justo en ese cambio de estado.
                        if (b && b->ownerIndex >= 0 && b->ownerIndex < (int)playerScores.size()) {
                            playerScores[b->ownerIndex] += enemy->scoreValue;
                        }
                    }
                }
            }
            // Explosi├│n: si toca a otra bomba que a├║n no ha explotado, la detona.
            for (auto* otherB : gBombs) {
                if (otherB && otherB != b && otherB->state == BombState::FUSE) {
                    if (explosionHitsEntity(gameMap, b, otherB->position)) {
                        otherB->detonate();
                    }
                }
            }
        }

        if (justExploded) {
            delete b;
            it = gBombs.erase(it);
        } else {
            ++it;
        }
    }

    // Colisi├│n enemigo Ôåö jugador: el jugador muere y respawnea.
    for (auto* enemy : gEnemies) {
        if (!enemy || enemy->lifeState != EnemyLifeState::Alive) continue;
        for (auto* p : gPlayers) {
            if (!p || !p->isAlive()) continue;
            if (overlapsEnemyPlayer(gameMap, enemy->position, p->position)) {
                p->killByEnemy();
            }
        }
    }

    // ========== Transiciones: Game Over / Next Level ==========
    if (this->state == GAME_PLAYING) {
        if (allPlayersOutOfLives()) {
            // No hay pantalla de Game Over: volver al men├║.
            returnToMenuFromGame(/*resetRun=*/true);
            return;
        }

        // Si se ha completado el nivel, esperamos un momento antes de avanzar.
        // Esto permite ver la animaci├│n de muerte del ├║ltimo enemigo y que el jugador reaccione.
        if (!pendingLevelAdvance) {
            if (allEnemiesCleared()) {
                pendingLevelAdvance = true;
                levelAdvanceTimer = 0.0f;
            }
        } else {
            levelAdvanceTimer += deltaTime;
            if (levelAdvanceTimer >= levelAdvanceDelaySeconds) {
                pendingLevelAdvance = false;
                levelAdvanceTimer = 0.0f;

                // Pasar de nivel: se conservan vidas y puntuaci├│n; se reinician stats.
                advanceToNextLevel();
                return;
            }
        }
    }
}

// Renderiza mapa, bombas, jugadores y enemigos.
void Game::render() {
    // ========== MENU ==========
    if (this->state == GAME_MENU) {
        glUseProgram(shader);
        menuScreen.renderMenu(VAO, shader, uniformModel, uniformProjection, uniformTexture,
                                     uniformUvRect, uniformTintColor, uniformFlipX, WIDTH, HEIGHT);
        glUseProgram(0);
        return;
    }

    // ========== CINEMATICA ==========
    if (this->state == GAME_CINEMATIC) {
        cinematicPlayer.render(VAO, shader, uniformModel, uniformProjection, uniformTexture,
                               uniformUvRect, uniformTintColor, uniformFlipX, WIDTH, HEIGHT);
        return;
    }

    if (this->viewMode == ViewMode::Mode3D && shader3D != 0 && cubeVAO != 0 && gameMap != nullptr) {
        glEnable(GL_DEPTH_TEST);
        glUseProgram(shader3D);

        const float aspect = (float)WIDTH / (float)HEIGHT;
        glm::vec3 cameraPos(0.0f, 16.0f, 12.0f);
        glm::vec3 cameraTarget(0.0f, 0.0f, 0.0f);
        glm::vec3 up(0.0f, 1.0f, 0.0f);

        if (camera3DType == Camera3DType::OrthographicFixed) {
            cameraPos = glm::vec3(0.0f, 20.0f, 0.01f);
            up = glm::vec3(0.0f, 0.0f, -1.0f);
        } else if (camera3DType == Camera3DType::PerspectiveMobile) {
            const float t = (float)glfwGetTime();
            cameraPos = glm::vec3(std::sin(t * 0.35f) * 10.0f, 14.0f, std::cos(t * 0.35f) * 10.0f);
        } else if (camera3DType == Camera3DType::FirstPerson && !gPlayers.empty() && gPlayers[0] != nullptr) {
            int playerRow = 0;
            int playerCol = 0;
            gameMap->ndcToGrid(gPlayers[0]->position, playerRow, playerCol);
            if (playerRow >= 0 && playerCol >= 0 && playerRow < gameMap->getRows() && playerCol < gameMap->getCols()) {
                cameraPos = gridToWorld3D(gameMap, playerRow, playerCol, 0.75f);
                glm::vec3 forward(0.0f, 0.0f, 1.0f);
                switch (gPlayers[0]->facingDirKey) {
                    case GLFW_KEY_UP:    forward = glm::vec3(0.0f, 0.0f, -1.0f); break;
                    case GLFW_KEY_DOWN:  forward = glm::vec3(0.0f, 0.0f, 1.0f); break;
                    case GLFW_KEY_LEFT:  forward = glm::vec3(-1.0f, 0.0f, 0.0f); break;
                    case GLFW_KEY_RIGHT: forward = glm::vec3(1.0f, 0.0f, 0.0f); break;
                }
                cameraTarget = cameraPos + forward * 2.0f;
            }
        }

        const glm::mat4 view = glm::lookAt(cameraPos, cameraTarget, up);
        glm::mat4 projection(1.0f);
        if (camera3DType == Camera3DType::OrthographicFixed) {
            const float halfW = (float)gameMap->getCols() * 0.55f;
            const float halfH = (float)gameMap->getRows() * 0.55f;
            projection = glm::ortho(-halfW, halfW, -halfH, halfH, 0.1f, 100.0f);
        } else {
            projection = glm::perspective(glm::radians(55.0f), aspect, 0.1f, 100.0f);
        }

        glUniformMatrix4fv(uniform3DView, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(uniform3DProjection, 1, GL_FALSE, glm::value_ptr(projection));

        auto drawCube3D = [&](const glm::vec3& center, const glm::vec3& scale, const glm::vec3& color) {
            glm::mat4 model(1.0f);
            model = glm::translate(model, center);
            model = glm::scale(model, scale);
            glUniformMatrix4fv(uniform3DModel, 1, GL_FALSE, glm::value_ptr(model));
            glUniform3fv(uniform3DColor, 1, glm::value_ptr(color));
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        };

        glBindVertexArray(cubeVAO);

        // Mapa: cubos por tile.
        for (int r = 0; r < gameMap->getRows(); ++r) {
            for (int c = 0; c < gameMap->getCols(); ++c) {
                const bool walkable = gameMap->isWalkable(r, c);
                const bool destructible = gameMap->isDestructible(r, c);

                float h = 0.10f;
                glm::vec3 color(0.24f, 0.26f, 0.27f);
                if (!walkable) {
                    h = destructible ? 0.70f : 1.00f;
                    color = destructible ? glm::vec3(0.75f, 0.45f, 0.20f)
                                        : glm::vec3(0.35f, 0.35f, 0.40f);
                }

                const glm::vec3 center = gridToWorld3D(gameMap, r, c, h * 0.5f);
                drawCube3D(center, glm::vec3(0.95f, h, 0.95f), color);
            }
        }

        // Jugadores como cajas 3D.
        for (std::size_t i = 0; i < gPlayers.size(); ++i) {
            Player* p = gPlayers[i];
            if (!p) continue;

            int r = 0;
            int c = 0;
            gameMap->ndcToGrid(p->position, r, c);
            if (r < 0 || c < 0 || r >= gameMap->getRows() || c >= gameMap->getCols()) continue;

            const glm::vec3 center = gridToWorld3D(gameMap, r, c, 0.55f);
            glm::vec3 color = (i == 0) ? glm::vec3(0.92f, 0.92f, 0.92f)
                                        : glm::vec3(0.88f, 0.18f, 0.18f);
            if (p->invincible) {
                float hz = 8.0f;
                if (p->invincibilityFromPowerUp) {
                    hz = (p->invincibilityTimer > 4.0f) ? 6.0f : 18.0f;
                }
                const float t = (float)glfwGetTime();
                const int phase = (int)(t * hz);
                if (phase % 2 == 0) {
                    color = glm::vec3(1.0f, 1.0f, 1.0f);
                }
            }
            drawCube3D(center, glm::vec3(0.60f, 1.10f, 0.60f), color);
        }

        // Enemigos como cajas 3D.
        for (auto* enemy : gEnemies) {
            if (!enemy || enemy->lifeState == EnemyLifeState::Dead) continue;

            int r = 0;
            int c = 0;
            gameMap->ndcToGrid(enemy->position, r, c);
            if (r < 0 || c < 0 || r >= gameMap->getRows() || c >= gameMap->getCols()) continue;

            const glm::vec3 center = gridToWorld3D(gameMap, r, c, 0.45f);
            drawCube3D(center, glm::vec3(0.60f, 0.90f, 0.60f), glm::vec3(0.19f, 0.68f, 0.27f));
        }

        // Bombas y explosiones como primitivas 3D.
        for (auto* b : gBombs) {
            if (!b || b->state == BombState::DONE) continue;

            if (b->state == BombState::FUSE) {
                const glm::vec3 center = gridToWorld3D(gameMap, b->gridRow, b->gridCol, 0.30f);
                drawCube3D(center, glm::vec3(0.45f, 0.45f, 0.45f), glm::vec3(0.10f, 0.10f, 0.10f));
            } else {
                for (std::size_t i = 0; i < b->explosionSegments.size(); ++i) {
                    const ExplosionSegment& seg = b->explosionSegments[i];
                    int r = 0;
                    int c = 0;
                    gameMap->ndcToGrid(seg.pos, r, c);
                    if (r < 0 || c < 0 || r >= gameMap->getRows() || c >= gameMap->getCols()) continue;

                    const bool warmColor = (((int)i + b->animFrame) % 2 == 0);
                    const glm::vec3 color = warmColor ? glm::vec3(1.00f, 0.15f, 0.10f)
                                                      : glm::vec3(1.00f, 0.92f, 0.10f);
                    const glm::vec3 center = gridToWorld3D(gameMap, r, c, 0.35f);
                    drawCube3D(center, glm::vec3(0.50f, 0.50f, 0.50f), color);
                }
            }
        }

        glBindVertexArray(0);
        glUseProgram(0);
        return;
    }

    glDisable(GL_DEPTH_TEST);

    glUseProgram(shader);

    // Calcular proyeccion ortografica para mantener el aspect ratio en NDC original
    float aspect = (float)WIDTH / (float)HEIGHT;
    glm::mat4 projection = glm::ortho(-aspect, aspect, -1.0f, 1.0f, -1.0f, 1.0f);
    glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, glm::value_ptr(projection));

    // Texture unit 0
    glUniform1i(uniformTexture, 0);

    // Por defecto, no aplicar flash blanco (solo lo usa el jugador en invulnerabilidad).
    glUniform1f(uniformWhiteFlash, 0.0f);

    // === 1. Renderizar mapa (fondo) ===
    gameMap->render(VAO, mapTexture, uniformModel, uniformUvRect, uniformTintColor, uniformFlipX);

    // === 1.1 Renderizar power-ups revelados (encima del suelo, debajo de bombas) ===
    gameMap->renderPowerUps(VAO, uniformModel, uniformUvRect, uniformTintColor, uniformFlipX);
    gameMap->renderHud(VAO, hudTexture, uniformModel, uniformUvRect);

    // === 1.5. Renderizar bombas (entre mapa y jugadores) ===
    if (!gBombs.empty()) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mapTexture); // Las bombas usan la misma textura que el mapa
        glBindVertexArray(VAO);

        for (auto* b : gBombs) {
            if (b && b->state != BombState::DONE) {
                b->Draw();
            }
        }

        glBindVertexArray(0);
    }

    // === 2. Renderizar jugador (encima del mapa) ===
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);

    glBindVertexArray(VAO);

    for (auto* p : gPlayers) {
        if (!p) continue;
        p->Draw();
    }

    // Importante: `Player::Draw()` puede dejar `whiteFlash=1` (parpadeo).
    // Reseteamos antes de dibujar otros sprites con el mismo shader.
    glUniform1f(uniformWhiteFlash, 0.0f);

    glBindVertexArray(0);

    // === 3. Renderizar enemigos ===
    if (enemyTexture != 0 && !gEnemies.empty()) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, enemyTexture);
        glBindVertexArray(VAO);

        for (auto enemy : gEnemies) {
            enemy->Draw();
        }

        glBindVertexArray(0);
    }

    glUseProgram(0);
}

// Cambiar pantalla de fullscreen a windowed (y viceversa)
void Game::toggleFullscreen(GLFWwindow* window) {

    if (glfwGetWindowMonitor(window) == nullptr) {
        // Cambiar a fullscreen
        glfwGetWindowPos(window, &windowedXPos, &windowedYPos);
        glfwGetWindowSize(window, &WIDTH, &HEIGHT);

        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    } else {
        // Pasar a windowed
        // Valores fixeados para windowed
        WIDTH = 1280; 
        HEIGHT = 720;
        glfwSetWindowMonitor(window, nullptr, windowedXPos, windowedYPos, WIDTH, HEIGHT, 0);
    }
}
