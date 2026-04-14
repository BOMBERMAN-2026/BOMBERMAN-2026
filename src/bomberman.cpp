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
#include "resource_manager.hpp"
#include "glb_loader.hpp"
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
#include <algorithm>
#include <cmath>
#include <utility>
#include <fstream>
#include <sstream>

GameMap* gameMap;

// Atlases y texturas

SpriteAtlas gPlayerAtlas; // No est├ítico para usarlo en player.cpp
SpriteAtlas gEnemyAtlas; // No est├ítico para usarlo en enemigos .cpp
SpriteAtlas gScoreboardAtlas; // Atlas para el scoreboard/HUD 
SpriteAtlas gBombAtlas; // Atlas para las bombas (misma sprite sheet del stage)

GLuint mapTexture;
GLuint horizonTexture;
GLuint enemyTexture = 0;
GLuint scoreboardTexture = 0; // Textura del scoreboard/HUD 

// Vectores de entidades
std::vector<Player*> gPlayers;
std::vector<Enemy*> gEnemies;
std::vector<Bomb*> gBombs;

// ============================== OpenGL: estado global ==============================

// Global variables for OpenGL
GLuint VAO, VBO, EBO, shader, uniformModel, uniformProjection, uniformTexture, uniformTintColor, uniformUvRect, uniformFlipX, uniformWhiteFlash;

GLuint texture;

// ============================== Shaders (vertex/fragment) ==============================
static const char* kSpriteVertexShaderPath = "shaders/sprite.vs";
static const char* kSpriteFragmentShaderPath = "shaders/sprite.frag";
static const char* kModel3DVertexShaderPath = "shaders/model3D.vs";
static const char* kModel3DFragmentShaderPath = "shaders/model3D.frag";
static const char* kModel3DTexturedVertexShaderPath = "shaders/model3D_textured.vs";
static const char* kModel3DTexturedFragmentShaderPath = "shaders/model3D_textured.frag";
static const char* kPlayerGlbPath = "models/3D/cartoon astronaut 3d model.glb";
static const char* kLeonGlbPath = "models/3D/monstrous creature 3d model.glb";
static const char* kFantasmaGlbPath = "models/3D/ghost character 3d model.glb";
static const char* kBebeGlbPath = "models/3D/cartoon creature 3d model.glb";
static const char* kBabosaGlbPath = "models/3D/poop character 3d model.glb";
static const char* kBombGlbPath = "models/3D/bomb 3d model.glb";
static const char* kFlameGlbPath = "models/3D/fiery flame 3d model.glb";
static const char* kFlamePowerUpGlbPath = "models/3D/fireball 3d model.glb";
static const char* kSpeedPowerUpGlbPath = "models/3D/roller skate 3d model.glb";
static const char* kKingBomberGlbPath = "models/3D/king bomber 3d model.glb";
static const char* kDronAzulGlbPath = "models/3D/blue robot 3d model.glb";
static const char* kDronRosaGlbPath = "models/3D/pink robot 3d model.glb";
static const char* kDronVerdeGlbPath = "models/3D/green robot 3d model.glb";
static const char* kDronAmarilloGlbPath = "models/3D/yellow robot 3d model.glb";
static const char* kSolGlbPath = "models/3D/cartoon sun star 3d model.glb";
static const char* kDragonGlbPath = "models/3D/teal creature 3d model.glb";
static const char* kHorizonBackgroundCandidates[] = {
    "models/3D/Fondo3D.jpeg",
    "models/Fondo3D.jpeg",
    "build/Fondo3D.jpeg",
    "build/WhatsApp Image 2026-04-08 at 11.06.16.jpeg"
};

GLuint cubeVAO = 0;
GLuint cubeVBO = 0;
GLuint cubeEBO = 0;
GLsizei cubeIndexCount = 0;
GLuint sphereVAO = 0;
GLuint sphereVBO = 0;
GLuint sphereEBO = 0;
GLsizei sphereIndexCount = 0;
GLuint actorGlbVAO = 0;
GLuint actorGlbVBO = 0;
GLuint actorGlbEBO = 0;
GLsizei actorGlbIndexCount = 0;
GLuint actorGlbTexture = 0;
GLuint leonGlbVAO = 0;
GLuint leonGlbVBO = 0;
GLuint leonGlbEBO = 0;
GLsizei leonGlbIndexCount = 0;
GLuint leonGlbTexture = 0;
GLuint fantasmaGlbVAO = 0;
GLuint fantasmaGlbVBO = 0;
GLuint fantasmaGlbEBO = 0;
GLsizei fantasmaGlbIndexCount = 0;
GLuint fantasmaGlbTexture = 0;
GLuint bebeGlbVAO = 0;
GLuint bebeGlbVBO = 0;
GLuint bebeGlbEBO = 0;
GLsizei bebeGlbIndexCount = 0;
GLuint bebeGlbTexture = 0;
GLuint babosaGlbVAO = 0;
GLuint babosaGlbVBO = 0;
GLuint babosaGlbEBO = 0;
GLsizei babosaGlbIndexCount = 0;
GLuint babosaGlbTexture = 0;
GLuint bombGlbVAO = 0;
GLuint bombGlbVBO = 0;
GLuint bombGlbEBO = 0;
GLsizei bombGlbIndexCount = 0;
GLuint bombGlbTexture = 0;
GLuint flameGlbVAO = 0;
GLuint flameGlbVBO = 0;
GLuint flameGlbEBO = 0;
GLsizei flameGlbIndexCount = 0;
GLuint flameGlbTexture = 0;
GLuint flamePowerUpGlbVAO = 0;
GLuint flamePowerUpGlbVBO = 0;
GLuint flamePowerUpGlbEBO = 0;
GLsizei flamePowerUpGlbIndexCount = 0;
GLuint flamePowerUpGlbTexture = 0;
GLuint speedPowerUpGlbVAO = 0;
GLuint speedPowerUpGlbVBO = 0;
GLuint speedPowerUpGlbEBO = 0;
GLsizei speedPowerUpGlbIndexCount = 0;
GLuint speedPowerUpGlbTexture = 0;
GLuint kingBomberGlbVAO = 0;
GLuint kingBomberGlbVBO = 0;
GLuint kingBomberGlbEBO = 0;
GLsizei kingBomberGlbIndexCount = 0;
GLuint kingBomberGlbTexture = 0;
GLuint dronAzulGlbVAO = 0;
GLuint dronAzulGlbVBO = 0;
GLuint dronAzulGlbEBO = 0;
GLsizei dronAzulGlbIndexCount = 0;
GLuint dronAzulGlbTexture = 0;
GLuint dronRosaGlbVAO = 0;
GLuint dronRosaGlbVBO = 0;
GLuint dronRosaGlbEBO = 0;
GLsizei dronRosaGlbIndexCount = 0;
GLuint dronRosaGlbTexture = 0;
GLuint dronVerdeGlbVAO = 0;
GLuint dronVerdeGlbVBO = 0;
GLuint dronVerdeGlbEBO = 0;
GLsizei dronVerdeGlbIndexCount = 0;
GLuint dronVerdeGlbTexture = 0;
GLuint dronAmarilloGlbVAO = 0;
GLuint dronAmarilloGlbVBO = 0;
GLuint dronAmarilloGlbEBO = 0;
GLsizei dronAmarilloGlbIndexCount = 0;
GLuint dronAmarilloGlbTexture = 0;
GLuint solGlbVAO = 0;
GLuint solGlbVBO = 0;
GLuint solGlbEBO = 0;
GLsizei solGlbIndexCount = 0;
GLuint solGlbTexture = 0;
GLuint dragonGlbVAO = 0;
GLuint dragonGlbVBO = 0;
GLuint dragonGlbEBO = 0;
GLsizei dragonGlbIndexCount = 0;
GLuint dragonGlbTexture = 0;
GLuint shader3D = 0;
GLuint shader3DTextured = 0;
GLuint uniform3DModel = 0;
GLuint uniform3DView = 0;
GLuint uniform3DProjection = 0;
GLuint uniform3DColor = 0;
GLuint uniform3DLightPos = 0;
GLuint uniform3DViewPos = 0;
GLuint uniform3DLightColor = 0;
GLuint uniform3DAmbientStrength = 0;
GLuint uniform3DSpecularStrength = 0;
GLuint uniform3DShininess = 0;
GLuint uniform3DTexturedModel = 0;
GLuint uniform3DTexturedView = 0;
GLuint uniform3DTexturedProjection = 0;
GLuint uniform3DTexturedSampler = 0;
GLuint uniform3DTexturedLightPos = 0;
GLuint uniform3DTexturedViewPos = 0;
GLuint uniform3DTexturedLightColor = 0;
GLuint uniform3DTexturedAmbientStrength = 0;
GLuint uniform3DTexturedSpecularStrength = 0;
GLuint uniform3DTexturedShininess = 0;

static constexpr float kDefaultPlayerSpeed = 0.4f;
static constexpr glm::vec2 kDefaultPlayerSize(0.2f, 0.2f);

static const char* viewModeToString(ViewMode mode) {
    return (mode == ViewMode::Mode3D) ? "3D" : "2D";
}

static const char* camera3DTypeToString(Camera3DType type) {
    switch (type) {
        case Camera3DType::OrthographicFixed: return "AereaFija";
        case Camera3DType::PerspectiveFixed: return "IsometricaFija";
        case Camera3DType::PerspectiveMobile: return "Seguimiento";
        case Camera3DType::FirstPerson: return "PrimeraPersona";
        default: return "Unknown";
    }
}

static glm::vec3 facingKeyToForward(int facingKey)
{
    switch (facingKey) {
        case GLFW_KEY_UP: return glm::vec3(0.0f, 0.0f, -1.0f);
        case GLFW_KEY_DOWN: return glm::vec3(0.0f, 0.0f, 1.0f);
        case GLFW_KEY_LEFT: return glm::vec3(-1.0f, 0.0f, 0.0f);
        case GLFW_KEY_RIGHT: return glm::vec3(1.0f, 0.0f, 0.0f);
        default: return glm::vec3(0.0f, 0.0f, 1.0f);
    }
}

static float facingKeyToYawRadians(int facingKey)
{
    switch (facingKey) {
        case GLFW_KEY_UP: return 0.0f;
        case GLFW_KEY_LEFT: return 1.57079632679f;
        case GLFW_KEY_DOWN: return 3.14159265359f;
        case GLFW_KEY_RIGHT: return -1.57079632679f;
        default: return 0.0f;
    }
}

static float enemyDirectionToYawRadians(EnemyDirection direction)
{
    switch (direction) {
        case EnemyDirection::UP: return 0.0f;
        case EnemyDirection::LEFT: return 1.57079632679f;
        case EnemyDirection::DOWN: return 3.14159265359f;
        case EnemyDirection::RIGHT: return -1.57079632679f;
        default: return 0.0f;
    }
}

static glm::vec3 firstPersonLookToForward(float yawRadians, float pitchRadians)
{
    const float cosPitch = std::cos(pitchRadians);
    return glm::normalize(glm::vec3(
        -std::sin(yawRadians) * cosPitch,
        std::sin(pitchRadians),
        -std::cos(yawRadians) * cosPitch
    ));
}

static int yawToControlQuadrant(float yawRadians)
{
    const float kTwoPi = 6.28318530718f;
    const float kHalfPi = 1.57079632679f;
    const float kQuarterPi = 0.78539816339f;

    float wrapped = std::fmod(yawRadians, kTwoPi);
    if (wrapped < 0.0f) {
        wrapped += kTwoPi;
    }

    return ((int)std::floor((wrapped + kQuarterPi) / kHalfPi)) % 4;
}

static GLint remapDirectionFor3DCamera(const Game* game, GLint inputDirKey)
{
    if (!game || game->viewMode != ViewMode::Mode3D) {
        return inputDirKey;
    }

    float yawForControls = 0.0f;
    if (game->camera3DType == Camera3DType::PerspectiveFixed ||
        game->camera3DType == Camera3DType::PerspectiveMobile) {
        yawForControls = game->getCameraOrbitYaw();
    } else if (game->camera3DType == Camera3DType::FirstPerson) {
        yawForControls = game->getFirstPersonYaw();
    } else {
        return inputDirKey;
    }

    const int quadrant = yawToControlQuadrant(yawForControls);
    switch (quadrant) {
        case 0:
            return inputDirKey;
        case 1:
            switch (inputDirKey) {
                case GLFW_KEY_UP: return GLFW_KEY_LEFT;
                case GLFW_KEY_RIGHT: return GLFW_KEY_UP;
                case GLFW_KEY_DOWN: return GLFW_KEY_RIGHT;
                case GLFW_KEY_LEFT: return GLFW_KEY_DOWN;
                default: return inputDirKey;
            }
        case 2:
            switch (inputDirKey) {
                case GLFW_KEY_UP: return GLFW_KEY_DOWN;
                case GLFW_KEY_RIGHT: return GLFW_KEY_LEFT;
                case GLFW_KEY_DOWN: return GLFW_KEY_UP;
                case GLFW_KEY_LEFT: return GLFW_KEY_RIGHT;
                default: return inputDirKey;
            }
        case 3:
            switch (inputDirKey) {
                case GLFW_KEY_UP: return GLFW_KEY_RIGHT;
                case GLFW_KEY_RIGHT: return GLFW_KEY_DOWN;
                case GLFW_KEY_DOWN: return GLFW_KEY_LEFT;
                case GLFW_KEY_LEFT: return GLFW_KEY_UP;
                default: return inputDirKey;
            }
        default:
            return inputDirKey;
    }
}

static Move directionKeyToMove(GLint directionKey)
{
    switch (directionKey) {
        case GLFW_KEY_UP: return MOVE_UP;
        case GLFW_KEY_DOWN: return MOVE_DOWN;
        case GLFW_KEY_LEFT: return MOVE_LEFT;
        case GLFW_KEY_RIGHT: return MOVE_RIGHT;
        default: return MOVE_NONE;
    }
}

static std::string buildWindowTitle(ViewMode viewMode, Camera3DType camera3DType)
{
    std::ostringstream title;
    title << "BOMBERMAN 2026 - Vista " << viewModeToString(viewMode);
    if (viewMode == ViewMode::Mode3D) {
        title << " | Camara: " << camera3DTypeToString(camera3DType);
    }
    title << " | F1 Vista | F2 Camara | TAB/F11 Fullscreen | F10 Minimizar";
    return title.str();
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
    MeshResource* cubeMesh = ResourceManager::createMesh("cube", []() -> MeshResource {
        MeshResource mesh;

        const GLfloat n = 0.57735026919f;
        const GLfloat vertices[] = {
            -0.5f, -0.5f, -0.5f, -n, -n, -n,
             0.5f, -0.5f, -0.5f,  n, -n, -n,
             0.5f,  0.5f, -0.5f,  n,  n, -n,
            -0.5f,  0.5f, -0.5f, -n,  n, -n,
            -0.5f, -0.5f,  0.5f, -n, -n,  n,
             0.5f, -0.5f,  0.5f,  n, -n,  n,
             0.5f,  0.5f,  0.5f,  n,  n,  n,
            -0.5f,  0.5f,  0.5f, -n,  n,  n
        };

        const GLuint indices[] = {
            0, 1, 2, 2, 3, 0,
            4, 5, 6, 6, 7, 4,
            0, 4, 7, 7, 3, 0,
            1, 5, 6, 6, 2, 1,
            3, 2, 6, 6, 7, 3,
            0, 1, 5, 5, 4, 0
        };

        glGenVertexArrays(1, &mesh.vao);
        glBindVertexArray(mesh.vao);

        glGenBuffers(1, &mesh.vbo);
        glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glGenBuffers(1, &mesh.ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (GLvoid*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);
        mesh.indexCount = 36;
        return mesh;
    });

    if (!cubeMesh) {
        return;
    }

    cubeVAO = cubeMesh->vao;
    cubeVBO = cubeMesh->vbo;
    cubeEBO = cubeMesh->ebo;
    cubeIndexCount = cubeMesh->indexCount;
}

void CreateSphere()
{
    MeshResource* sphereMesh = ResourceManager::createMesh("sphere", []() -> MeshResource {
        MeshResource mesh;

        const unsigned int stacks = 12;
        const unsigned int slices = 16;
        const float kPi = 3.14159265359f;

        std::vector<GLfloat> vertices;
        std::vector<GLuint> indices;
        vertices.reserve((stacks + 1) * (slices + 1) * 6);
        indices.reserve(stacks * slices * 6);

        for (unsigned int i = 0; i <= stacks; ++i) {
            const float v = (float)i / (float)stacks;
            const float phi = v * kPi;
            const float y = std::cos(phi) * 0.5f;
            const float ringRadius = std::sin(phi) * 0.5f;

            for (unsigned int j = 0; j <= slices; ++j) {
                const float u = (float)j / (float)slices;
                const float theta = u * (2.0f * kPi);
                const float x = ringRadius * std::cos(theta);
                const float z = ringRadius * std::sin(theta);
                const glm::vec3 normal = glm::normalize(glm::vec3(x, y, z));

                vertices.push_back(x);
                vertices.push_back(y);
                vertices.push_back(z);
                vertices.push_back(normal.x);
                vertices.push_back(normal.y);
                vertices.push_back(normal.z);
            }
        }

        for (unsigned int i = 0; i < stacks; ++i) {
            for (unsigned int j = 0; j < slices; ++j) {
                const GLuint first = i * (slices + 1) + j;
                const GLuint second = first + slices + 1;

                indices.push_back(first);
                indices.push_back(second);
                indices.push_back(first + 1);

                indices.push_back(first + 1);
                indices.push_back(second);
                indices.push_back(second + 1);
            }
        }

        glGenVertexArrays(1, &mesh.vao);
        glBindVertexArray(mesh.vao);

        glGenBuffers(1, &mesh.vbo);
        glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GLfloat), vertices.data(), GL_STATIC_DRAW);

        glGenBuffers(1, &mesh.ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (GLvoid*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);
        mesh.indexCount = static_cast<GLsizei>(indices.size());
        return mesh;
    });

    if (!sphereMesh) {
        return;
    }

    sphereVAO = sphereMesh->vao;
    sphereVBO = sphereMesh->vbo;
    sphereEBO = sphereMesh->ebo;
    sphereIndexCount = sphereMesh->indexCount;
}

static bool createTexturedGlbModel(const std::string& meshCacheKey,
                                   const std::string& modelPath,
                                   GLuint& outVao,
                                   GLuint& outVbo,
                                   GLuint& outEbo,
                                   GLsizei& outIndexCount,
                                   GLuint& outTexture)
{
    TexturedMeshData meshData;
    std::string loadError;
    if (!loadGlbTexturedMesh(modelPath, meshData, &loadError)) {
        std::cerr << "[Render] Aviso: no se pudo cargar GLB ('" << modelPath
                  << "'): " << loadError << "\n";
        return false;
    }

    if (meshData.vertices.empty() || meshData.indices.empty()) {
        std::cerr << "[Render] Aviso: GLB sin datos renderizables: " << modelPath << "\n";
        return false;
    }

    MeshResource* actorMesh = ResourceManager::createMesh(meshCacheKey, [meshData]() -> MeshResource {
        MeshResource mesh;

        glGenVertexArrays(1, &mesh.vao);
        glBindVertexArray(mesh.vao);

        glGenBuffers(1, &mesh.vbo);
        glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     meshData.vertices.size() * sizeof(GLfloat),
                     meshData.vertices.data(),
                     GL_STATIC_DRAW);

        glGenBuffers(1, &mesh.ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     meshData.indices.size() * sizeof(GLuint),
                     meshData.indices.data(),
                     GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(6 * sizeof(GLfloat)));
        glEnableVertexAttribArray(2);

        glBindVertexArray(0);
        mesh.indexCount = static_cast<GLsizei>(meshData.indices.size());
        return mesh;
    });

    if (!actorMesh) {
        std::cerr << "[Render] Aviso: no se pudo crear mesh OpenGL para GLB: " << modelPath << "\n";
        return false;
    }

    outVao = actorMesh->vao;
    outVbo = actorMesh->vbo;
    outEbo = actorMesh->ebo;
    outIndexCount = actorMesh->indexCount;

    if (outTexture != 0) {
        glDeleteTextures(1, &outTexture);
        outTexture = 0;
    }

    glGenTextures(1, &outTexture);
    glBindTexture(GL_TEXTURE_2D, outTexture);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA,
                 meshData.textureWidth,
                 meshData.textureHeight,
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 meshData.baseColorRgba.data());
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    return true;
}

void CreateActorGlbModel(const std::string& modelPath)
{
    (void)createTexturedGlbModel("actorGLB",
                                 modelPath,
                                 actorGlbVAO,
                                 actorGlbVBO,
                                 actorGlbEBO,
                                 actorGlbIndexCount,
                                 actorGlbTexture);
}

void CreateLeonGlbModel(const std::string& modelPath)
{
    (void)createTexturedGlbModel("leonGLB",
                                 modelPath,
                                 leonGlbVAO,
                                 leonGlbVBO,
                                 leonGlbEBO,
                                 leonGlbIndexCount,
                                 leonGlbTexture);
}

void CreateFantasmaGlbModel(const std::string& modelPath)
{
    (void)createTexturedGlbModel("fantasmaGLB",
                                 modelPath,
                                 fantasmaGlbVAO,
                                 fantasmaGlbVBO,
                                 fantasmaGlbEBO,
                                 fantasmaGlbIndexCount,
                                 fantasmaGlbTexture);
}

void CreateBebeGlbModel(const std::string& modelPath)
{
    (void)createTexturedGlbModel("bebeGLB",
                                 modelPath,
                                 bebeGlbVAO,
                                 bebeGlbVBO,
                                 bebeGlbEBO,
                                 bebeGlbIndexCount,
                                 bebeGlbTexture);
}

void CreateBabosaGlbModel(const std::string& modelPath)
{
    (void)createTexturedGlbModel("babosaGLB",
                                 modelPath,
                                 babosaGlbVAO,
                                 babosaGlbVBO,
                                 babosaGlbEBO,
                                 babosaGlbIndexCount,
                                 babosaGlbTexture);
}

void CreateBombGlbModel(const std::string& modelPath)
{
    (void)createTexturedGlbModel("bombGLB",
                                 modelPath,
                                 bombGlbVAO,
                                 bombGlbVBO,
                                 bombGlbEBO,
                                 bombGlbIndexCount,
                                 bombGlbTexture);
}

void CreateFlameGlbModel(const std::string& modelPath)
{
    (void)createTexturedGlbModel("flameGLB",
                                 modelPath,
                                 flameGlbVAO,
                                 flameGlbVBO,
                                 flameGlbEBO,
                                 flameGlbIndexCount,
                                 flameGlbTexture);
}

void CreateFlamePowerUpGlbModel(const std::string& modelPath)
{
    (void)createTexturedGlbModel("flamePowerUpGLB",
                                 modelPath,
                                 flamePowerUpGlbVAO,
                                 flamePowerUpGlbVBO,
                                 flamePowerUpGlbEBO,
                                 flamePowerUpGlbIndexCount,
                                 flamePowerUpGlbTexture);
}

void CreateSpeedPowerUpGlbModel(const std::string& modelPath)
{
    (void)createTexturedGlbModel("speedPowerUpGLB",
                                 modelPath,
                                 speedPowerUpGlbVAO,
                                 speedPowerUpGlbVBO,
                                 speedPowerUpGlbEBO,
                                 speedPowerUpGlbIndexCount,
                                 speedPowerUpGlbTexture);
}

void CreateKingBomberGlbModel(const std::string& modelPath)
{
    (void)createTexturedGlbModel("kingBomberGLB",
                                 modelPath,
                                 kingBomberGlbVAO,
                                 kingBomberGlbVBO,
                                 kingBomberGlbEBO,
                                 kingBomberGlbIndexCount,
                                 kingBomberGlbTexture);
}

void CreateDronAzulGlbModel(const std::string& modelPath)
{
    (void)createTexturedGlbModel("dronAzulGLB",
                                 modelPath,
                                 dronAzulGlbVAO,
                                 dronAzulGlbVBO,
                                 dronAzulGlbEBO,
                                 dronAzulGlbIndexCount,
                                 dronAzulGlbTexture);
}

void CreateDronRosaGlbModel(const std::string& modelPath)
{
    (void)createTexturedGlbModel("dronRosaGLB",
                                 modelPath,
                                 dronRosaGlbVAO,
                                 dronRosaGlbVBO,
                                 dronRosaGlbEBO,
                                 dronRosaGlbIndexCount,
                                 dronRosaGlbTexture);
}

void CreateDronVerdeGlbModel(const std::string& modelPath)
{
    (void)createTexturedGlbModel("dronVerdeGLB",
                                 modelPath,
                                 dronVerdeGlbVAO,
                                 dronVerdeGlbVBO,
                                 dronVerdeGlbEBO,
                                 dronVerdeGlbIndexCount,
                                 dronVerdeGlbTexture);
}

void CreateDronAmarilloGlbModel(const std::string& modelPath)
{
    (void)createTexturedGlbModel("dronAmarilloGLB",
                                 modelPath,
                                 dronAmarilloGlbVAO,
                                 dronAmarilloGlbVBO,
                                 dronAmarilloGlbEBO,
                                 dronAmarilloGlbIndexCount,
                                 dronAmarilloGlbTexture);
}

void CreateSolGlbModel(const std::string& modelPath)
{
    (void)createTexturedGlbModel("solGLB",
                                 modelPath,
                                 solGlbVAO,
                                 solGlbVBO,
                                 solGlbEBO,
                                 solGlbIndexCount,
                                 solGlbTexture);
}

void CreateDragonGlbModel(const std::string& modelPath)
{
    (void)createTexturedGlbModel("dragonGLB",
                                 modelPath,
                                 dragonGlbVAO,
                                 dragonGlbVBO,
                                 dragonGlbEBO,
                                 dragonGlbIndexCount,
                                 dragonGlbTexture);
}

void Compile3DShaders()
{
    const std::string resolvedVertexPath = resolveAssetPath(kModel3DVertexShaderPath);
    const std::string resolvedFragmentPath = resolveAssetPath(kModel3DFragmentShaderPath);

    shader3D = ResourceManager::loadShader("model3D", resolvedVertexPath, resolvedFragmentPath);
    if (shader3D == 0) {
        std::cerr << "Error creando/cargando programa shader 3D\n";
        return;
    }

    uniform3DModel = glGetUniformLocation(shader3D, "model");
    uniform3DView = glGetUniformLocation(shader3D, "view");
    uniform3DProjection = glGetUniformLocation(shader3D, "projection");
    uniform3DColor = glGetUniformLocation(shader3D, "objectColor");
    uniform3DLightPos = glGetUniformLocation(shader3D, "lightPos");
    uniform3DViewPos = glGetUniformLocation(shader3D, "viewPos");
    uniform3DLightColor = glGetUniformLocation(shader3D, "lightColor");
    uniform3DAmbientStrength = glGetUniformLocation(shader3D, "ambientStrength");
    uniform3DSpecularStrength = glGetUniformLocation(shader3D, "specularStrength");
    uniform3DShininess = glGetUniformLocation(shader3D, "shininess");
}

void Compile3DTexturedShaders()
{
    const std::string resolvedVertexPath = resolveAssetPath(kModel3DTexturedVertexShaderPath);
    const std::string resolvedFragmentPath = resolveAssetPath(kModel3DTexturedFragmentShaderPath);

    shader3DTextured = ResourceManager::loadShader("model3DTextured", resolvedVertexPath, resolvedFragmentPath);
    if (shader3DTextured == 0) {
        std::cerr << "Error creando/cargando programa shader 3D texturizado\n";
        return;
    }

    uniform3DTexturedModel = glGetUniformLocation(shader3DTextured, "model");
    uniform3DTexturedView = glGetUniformLocation(shader3DTextured, "view");
    uniform3DTexturedProjection = glGetUniformLocation(shader3DTextured, "projection");
    uniform3DTexturedSampler = glGetUniformLocation(shader3DTextured, "baseColorTex");
    uniform3DTexturedLightPos = glGetUniformLocation(shader3DTextured, "lightPos");
    uniform3DTexturedViewPos = glGetUniformLocation(shader3DTextured, "viewPos");
    uniform3DTexturedLightColor = glGetUniformLocation(shader3DTextured, "lightColor");
    uniform3DTexturedAmbientStrength = glGetUniformLocation(shader3DTextured, "ambientStrength");
    uniform3DTexturedSpecularStrength = glGetUniformLocation(shader3DTextured, "specularStrength");
    uniform3DTexturedShininess = glGetUniformLocation(shader3DTextured, "shininess");
}

static glm::vec3 gridToWorld3D(const GameMap* map, int row, int col, float y)
{
    const float worldX = (float)col - ((float)map->getCols() * 0.5f) + 0.5f;
    const float worldZ = (float)row - ((float)map->getRows() * 0.5f) + 0.5f;
    return glm::vec3(worldX, y, worldZ);
}

static glm::vec3 ndcToWorld3D(const GameMap* map, const glm::vec2& ndc, float y)
{
    if (!map) {
        return glm::vec3(0.0f, y, 0.0f);
    }

    const glm::vec2 cell00 = map->gridToNDC(0, 0);
    const glm::vec2 cell01 = (map->getCols() > 1)
        ? map->gridToNDC(0, 1)
        : glm::vec2(cell00.x + map->getTileSize(), cell00.y);
    const glm::vec2 cell10 = (map->getRows() > 1)
        ? map->gridToNDC(1, 0)
        : glm::vec2(cell00.x, cell00.y - map->getTileSize());

    const float dx = (std::abs(cell01.x - cell00.x) > 0.0001f) ? (cell01.x - cell00.x) : map->getTileSize();
    const float dy = (std::abs(cell10.y - cell00.y) > 0.0001f) ? (cell10.y - cell00.y) : -map->getTileSize();

    const float col = (ndc.x - cell00.x) / dx;
    const float row = (ndc.y - cell00.y) / dy;

    const float worldX = col - ((float)map->getCols() * 0.5f) + 0.5f;
    const float worldZ = row - ((float)map->getRows() * 0.5f) + 0.5f;
    return glm::vec3(worldX, y, worldZ);
}

static void renderFirstPersonMiniMap2D(const GameMap* map, int width, int height)
{
    if (!map || shader == 0 || VAO == 0 || mapTexture == 0) {
        return;
    }

    const int rows = map->getRows();
    const int cols = map->getCols();
    if (rows <= 0 || cols <= 0 || width <= 0 || height <= 0) {
        return;
    }

    const float safeHeight = (float)height;
    const float aspect = std::max(0.01f, (float)width / safeHeight);
    const float mapRatio = (float)cols / (float)rows;

    const float margin = 0.06f;
    float panelHeight = 0.56f;
    float panelWidth = panelHeight * mapRatio;

    const float maxPanelWidth = std::max(0.40f, aspect * 0.65f);
    if (panelWidth > maxPanelWidth) {
        panelWidth = maxPanelWidth;
        panelHeight = panelWidth / std::max(0.2f, mapRatio);
    }

    const float panelRight = aspect - margin;
    const float panelTop = 1.0f - margin;
    const float panelLeft = panelRight - panelWidth;
    const float panelBottom = panelTop - panelHeight;
    const float panelCenterX = (panelLeft + panelRight) * 0.5f;
    const float panelCenterY = (panelTop + panelBottom) * 0.5f;
    const float panelTileW = panelWidth / (float)cols;
    const float panelTileH = panelHeight / (float)rows;

    glm::vec4 frameUv(0.0f, 0.0f, 1.0f, 1.0f);
    if (!map->getUvRectForSpriteId(10, frameUv)) {
        map->getUvRectForTile(0, 0, frameUv);
    }

    auto drawQuadNdc = [&](float centerX,
                           float centerY,
                           float halfW,
                           float halfH,
                           GLuint texId,
                           const glm::vec4& uvRect,
                           const glm::vec4& tint,
                           float flipX = 0.0f) {
        if (texId == 0) {
            return;
        }

        glm::mat4 model(1.0f);
        model = glm::translate(model, glm::vec3(centerX, centerY, 0.0f));
        model = glm::scale(model, glm::vec3(halfW, halfH, 1.0f));

        glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
        glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));
        glUniform4fv(uniformTintColor, 1, glm::value_ptr(tint));
        glUniform1f(uniformFlipX, flipX);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texId);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    };

    auto spriteAspect = [](const SpriteAtlas& atlas, const std::string& spriteName) {
        auto it = atlas.sprites.find(spriteName);
        if (it == atlas.sprites.end() || it->second.h <= 0) {
            return 1.0f;
        }
        const float raw = (float)it->second.w / (float)it->second.h;
        return std::max(0.35f, std::min(2.50f, raw));
    };

    glDisable(GL_DEPTH_TEST);
    glUseProgram(shader);

    const glm::mat4 projection = glm::ortho(-aspect, aspect, -1.0f, 1.0f, -1.0f, 1.0f);
    glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, glm::value_ptr(projection));
    glUniform1i(uniformTexture, 0);
    glBindVertexArray(VAO);

    // Marco del panel.
    drawQuadNdc(panelCenterX,
                panelCenterY,
                panelWidth * 0.5f + 0.02f,
                panelHeight * 0.5f + 0.02f,
                mapTexture,
                frameUv,
                glm::vec4(0.05f, 0.05f, 0.05f, 0.96f));

    drawQuadNdc(panelCenterX,
                panelCenterY,
                panelWidth * 0.5f,
                panelHeight * 0.5f,
                mapTexture,
                frameUv,
                glm::vec4(0.28f, 0.28f, 0.28f, 1.0f));

    // Recorta todo lo que se dibuje para no salir del minimapa.
    auto clampInt = [](int v, int lo, int hi) {
        return std::max(lo, std::min(v, hi));
    };

    const float leftNdc = panelLeft / aspect;
    const float rightNdc = panelRight / aspect;
    const float bottomNdc = panelBottom;
    const float topNdc = panelTop;

    int scissorLeft = (int)std::floor(((leftNdc * 0.5f) + 0.5f) * (float)width);
    int scissorRight = (int)std::ceil(((rightNdc * 0.5f) + 0.5f) * (float)width);
    int scissorBottom = (int)std::floor(((bottomNdc * 0.5f) + 0.5f) * (float)height);
    int scissorTop = (int)std::ceil(((topNdc * 0.5f) + 0.5f) * (float)height);

    scissorLeft = clampInt(scissorLeft, 0, width);
    scissorRight = clampInt(scissorRight, 0, width);
    scissorBottom = clampInt(scissorBottom, 0, height);
    scissorTop = clampInt(scissorTop, 0, height);

    const int scissorWidth = std::max(0, scissorRight - scissorLeft);
    const int scissorHeight = std::max(0, scissorTop - scissorBottom);
    const bool hasValidScissor = (scissorWidth > 0 && scissorHeight > 0);

    if (hasValidScissor) {
        glEnable(GL_SCISSOR_TEST);
        glScissor(scissorLeft, scissorBottom, scissorWidth, scissorHeight);
    }

    // Tile a tile con las UV reales del atlas del mapa.
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            glm::vec4 tileUv(0.0f, 0.0f, 1.0f, 1.0f);
            if (!map->getUvRectForTile(row, col, tileUv)) {
                continue;
            }

            const float x = panelLeft + ((float)col + 0.5f) * panelTileW;
            const float y = panelTop - ((float)row + 0.5f) * panelTileH;
            drawQuadNdc(x,
                        y,
                        panelTileW * 0.5f,
                        panelTileH * 0.5f,
                        mapTexture,
                        tileUv,
                        glm::vec4(1.0f));
        }
    }

    const glm::vec2 cell00 = map->gridToNDC(0, 0);
    const glm::vec2 cell01 = (cols > 1)
        ? map->gridToNDC(0, 1)
        : glm::vec2(cell00.x + map->getTileSize(), cell00.y);
    const glm::vec2 cell10 = (rows > 1)
        ? map->gridToNDC(1, 0)
        : glm::vec2(cell00.x, cell00.y - map->getTileSize());

    const float dx = (std::abs(cell01.x - cell00.x) > 0.0001f) ? (cell01.x - cell00.x) : map->getTileSize();
    const float dy = (std::abs(cell10.y - cell00.y) > 0.0001f) ? (cell10.y - cell00.y) : -map->getTileSize();

    auto ndcToMiniMap = [&](const glm::vec2& ndcPos, float& outX, float& outY) {
        const float colF = (ndcPos.x - cell00.x) / dx;
        const float rowF = (ndcPos.y - cell00.y) / dy;

        if (colF < -0.5f || colF > (float)cols - 0.5f || rowF < -0.5f || rowF > (float)rows - 0.5f) {
            return false;
        }

        const float xNorm = (colF + 0.5f) / (float)cols;
        const float yNorm = (rowF + 0.5f) / (float)rows;

        outX = panelLeft + xNorm * panelWidth;
        outY = panelTop - yNorm * panelHeight;
        return true;
    };

    auto drawMiniActor = [&](const glm::vec2& ndcPos,
                             GLuint texId,
                             const glm::vec4& uvRect,
                             float actorAspect,
                             float actorFlipX = 0.0f) {
        float x = 0.0f;
        float y = 0.0f;
        if (!ndcToMiniMap(ndcPos, x, y)) {
            return;
        }

        float halfH = std::max(panelTileH * 0.82f, panelHeight * 0.0220f);
        float halfW = halfH * actorAspect;

        halfW = std::min(halfW, panelTileW * 1.55f);
        halfH = std::min(halfH, panelTileH * 1.55f);

        // Eleva un poco para que los pies coincidan mejor con la celda.
        const float yOffset = halfH * 0.05f;
        drawQuadNdc(x, y + yOffset, halfW, halfH, texId, uvRect, glm::vec4(1.0f), actorFlipX);
    };

    // Enemigos: usar siempre el sprite estático frontal (abajo.0) en el minimapa.
    for (auto* enemy : gEnemies) {
        if (!enemy || enemy->lifeState != EnemyLifeState::Alive || enemyTexture == 0) {
            continue;
        }

        glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
        std::string chosenSprite;

        // Obtener el base del sprite del enemigo.
        std::string base = enemy->spriteBaseId;
        if (base.empty() && !enemy->currentSpriteName.empty()) {
            const std::size_t dotPos = enemy->currentSpriteName.find('.');
            base = (dotPos == std::string::npos)
                ? enemy->currentSpriteName
                : enemy->currentSpriteName.substr(0, dotPos);
        }

        if (!base.empty()) {
            const std::string frontName = base + ".abajo.0";
            const std::string frontAlt = base + ".frente.0";
            const std::string sideName = base + ".derecha.0";

            if (getUvRectForSprite(gEnemyAtlas, frontName, uvRect)) {
                chosenSprite = frontName;
            } else if (getUvRectForSprite(gEnemyAtlas, frontAlt, uvRect)) {
                chosenSprite = frontAlt;
            } else if (getUvRectForSprite(gEnemyAtlas, sideName, uvRect)) {
                chosenSprite = sideName;
            }
        }

        if (chosenSprite.empty()) {
            continue;
        }

        drawMiniActor(enemy->position, enemyTexture, uvRect,
                      spriteAspect(gEnemyAtlas, chosenSprite), 0.0f);
    }

    // Jugadores: usar siempre el sprite estático frontal (abajo.0) en el minimapa.
    for (Player* player : gPlayers) {
        if (!player || !player->isAlive() || texture == 0) {
            continue;
        }

        glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
        std::string chosenSprite;

        // Siempre usar el sprite frontal estático.
        const std::string frontName = player->spritePrefix + ".abajo.0";
        if (getUvRectForSprite(gPlayerAtlas, frontName, uvRect)) {
            chosenSprite = frontName;
        } else {
            const std::string sideName = player->spritePrefix + ".derecha.0";
            if (getUvRectForSprite(gPlayerAtlas, sideName, uvRect)) {
                chosenSprite = sideName;
            } else {
                continue;
            }
        }

        drawMiniActor(player->position, texture, uvRect,
                      spriteAspect(gPlayerAtlas, chosenSprite), 0.0f);
    }

    if (hasValidScissor) {
        glDisable(GL_SCISSOR_TEST);
    }

    glBindVertexArray(0);
    glUseProgram(0);
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
    const std::string resolvedVertexPath = resolveAssetPath(kSpriteVertexShaderPath);
    const std::string resolvedFragmentPath = resolveAssetPath(kSpriteFragmentShaderPath);

    shader = ResourceManager::loadShader("sprite2D", resolvedVertexPath, resolvedFragmentPath);
    if (shader == 0)
    {
        std::cerr << "Error creating shader program!\n";
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

static GLuint loadHorizonTextureWithFallback() {
    GLuint textureId = 0;
    for (const char* candidatePath : kHorizonBackgroundCandidates) {
        const std::string resolvedPath = resolveAssetPath(candidatePath);
        textureId = ResourceManager::loadTexture("horizon3D", resolvedPath, LoadTexture);
        if (textureId != 0) {
            glBindTexture(GL_TEXTURE_2D, textureId);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            return textureId;
        }
    }

    std::cerr << "[Render] Aviso: no se pudo cargar fondo 3D. Rutas probadas:\n";
    for (const char* candidatePath : kHorizonBackgroundCandidates) {
        std::cerr << "  - " << resolveAssetPath(candidatePath) << "\n";
    }
    return 0;
}

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
    CreateSphere();
    CreateActorGlbModel(resolveAssetPath(kPlayerGlbPath));
    CreateLeonGlbModel(resolveAssetPath(kLeonGlbPath));
    CreateFantasmaGlbModel(resolveAssetPath(kFantasmaGlbPath));
    CreateBebeGlbModel(resolveAssetPath(kBebeGlbPath));
    CreateBabosaGlbModel(resolveAssetPath(kBabosaGlbPath));
    CreateBombGlbModel(resolveAssetPath(kBombGlbPath));
    CreateFlameGlbModel(resolveAssetPath(kFlameGlbPath));
    CreateFlamePowerUpGlbModel(resolveAssetPath(kFlamePowerUpGlbPath));
    CreateSpeedPowerUpGlbModel(resolveAssetPath(kSpeedPowerUpGlbPath));
    CreateKingBomberGlbModel(resolveAssetPath(kKingBomberGlbPath));
    CreateDronAzulGlbModel(resolveAssetPath(kDronAzulGlbPath));
    CreateDronRosaGlbModel(resolveAssetPath(kDronRosaGlbPath));
    CreateDronVerdeGlbModel(resolveAssetPath(kDronVerdeGlbPath));
    CreateDronAmarilloGlbModel(resolveAssetPath(kDronAmarilloGlbPath));
    CreateSolGlbModel(resolveAssetPath(kSolGlbPath));
    CreateDragonGlbModel(resolveAssetPath(kDragonGlbPath));
    Compile3DShaders();
    Compile3DTexturedShaders();

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

    // Textura del HUD (fija)
    {
        const std::string scoreboardAtlasPath = resolveAssetPath("resources/sprites/atlases/SpriteAtlasScoreboard.json");
        if (!loadSpriteAtlasMinimal(scoreboardAtlasPath, gScoreboardAtlas)) {
            std::cerr << "Error cargando atlas del scoreboard: " << scoreboardAtlasPath << std::endl;
            std::exit(EXIT_FAILURE);
        }

        const std::string scoreboardTexPath = resolveAssetPath(gScoreboardAtlas.imagePath);  // Usa la ruta del JSON
        scoreboardTexture = LoadTexture(scoreboardTexPath.c_str());
        if (scoreboardTexture == 0) {
            std::cerr << "Error cargando textura del scoreboard: " << scoreboardTexPath << std::endl;
            std::exit(EXIT_FAILURE);
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

    int stageNum = levelToStage[levelIndex];
    std::string stageNumStr = std::to_string(stageNum);

    currentGameLevel = mapNumeration[levelIndex];
    levelTimeRemaining = 121.0f;

    if (mapTexture != 0) {
        glDeleteTextures(1, &mapTexture);
        mapTexture = 0;
    }

    std::string mapTexPath = resolveAssetPath("resources/sprites/mapas/Stage" + stageNumStr + "/sprites-Stage" + stageNumStr + ".png");
    mapTexture = LoadTexture(mapTexPath.c_str());
    if (mapTexture == 0) {
        std::cerr << "Error cargando textura del mapa: " << mapTexPath << std::endl;
        std::exit(EXIT_FAILURE);
    }

    std::string bombAtlasPath = resolveAssetPath("resources/sprites/atlases/SpriteAtlasStage" + stageNumStr + ".json");
    if (!loadSpriteAtlasMinimal(bombAtlasPath, gBombAtlas)) {
        std::cerr << "Error cargando atlas bombas: " << bombAtlasPath << std::endl;
    }

    if (!gameMap->loadFromFile(levelSequence[levelIndex])) {
        std::cerr << "Error cargando mapa: " << levelSequence[levelIndex] << std::endl;
        std::exit(EXIT_FAILURE);
    }

    std::string mapAtlasPath = "resources/sprites/atlases/SpriteAtlasStage" + stageNumStr + ".json";
    if (!gameMap->loadAtlas(mapAtlasPath)) {
        std::cerr << "Error cargando atlas del mapa: " << mapAtlasPath << std::endl;
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
                    auto* e = new KingBomber(pos, kDefaultPlayerSize, /*speed=*/kDefaultPlayerSpeed);
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

// Arranca una partida nueva desde nivel_01 (índice 0).
void Game::startNewRun(GameMode newMode) {
    mode = newMode;
    currentLevelIndex = 0;
    currentLevelHadEnemies = false;
    playerScores.clear();

    // Transicionar a CINEMATIC para reproducir cinemática del primer nivel antes de cargar
    this->state = GAME_CINEMATIC;
    this->currentCinematicType = CinematicType::LevelStart;
    this->nextStateAfterCinematic = GAME_PLAYING;
    this->loadLevelPending = true;  // Flag para cargar nivel después de cinemática

    // Abrir el video de la cinemática del primer nivel
    std::string videoPath = resolveAssetPath(levelCinematicSequence[currentLevelIndex]);
    cinematicPlayer.open(videoPath);

    // Por si el menú dejó la marca de transición activa.
    menuScreen.resetTransition();
}

// Avanza al siguiente nivel (si existe) preservando el progreso definido en `loadLevel`.
void Game::advanceToNextLevel() {
    const int nextIndex = currentLevelIndex + 1;
    if (nextIndex >= (int)levelSequence.size()) {
        // No hay ranking ni pantalla de victoria: volver al menú.
        if (mode == GameMode::TwoPlayers) {
            // Reproducir cinematica fin de historia antes de volver a menu.
            this->state = GAME_CINEMATIC;
            this->currentCinematicType = CinematicType::HistoryEnd;
            this->nextStateAfterCinematic = GAME_MENU;
            std::string videoPath = resolveAssetPath("resources/video/HistoryEnd.mp4");
            cinematicPlayer.open(videoPath);
        } else {
            returnToMenuFromGame(/*resetRun=*/true);
        }
        return;
    }

    currentLevelIndex = nextIndex;
    // Transicionar a CINEMATIC para reproducir cinemática del siguiente nivel antes de cargar
    this->state = GAME_CINEMATIC;
    this->currentCinematicType = CinematicType::LevelStart;
    this->nextStateAfterCinematic = GAME_PLAYING;
    this->loadLevelPending = true;  // Flag para cargar nivel después de cinemática

    // Abrir el video de la cinemática del nivel
    std::string videoPath = resolveAssetPath(levelCinematicSequence[currentLevelIndex]);
    cinematicPlayer.open(videoPath);
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

// ============================== Game lifecycle ==============================

void Game::toggleViewMode() {
    viewMode = (viewMode == ViewMode::Mode2D) ? ViewMode::Mode3D : ViewMode::Mode2D;
    surpriseHorizonVisible3D = false;
    surpriseKey3TapCount = 0;
    surpriseKey3LastTapTime = -10.0;

    const bool shouldCaptureFirstPersonMouse =
        (viewMode == ViewMode::Mode3D && camera3DType == Camera3DType::FirstPerson && window != nullptr);
    if (window != nullptr) {
        glfwSetInputMode(window, GLFW_CURSOR, shouldCaptureFirstPersonMouse ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    }
    firstPersonCursorLocked = shouldCaptureFirstPersonMouse;
    firstPersonMouseInitialized = false;

    refreshWindowTitle();
    std::cout << "[Render] View mode -> " << viewModeToString(viewMode) << "\n";
}

void Game::cycleCamera3DType() {
    switch (camera3DType) {
        case Camera3DType::OrthographicFixed:
        case Camera3DType::FirstPerson:
            camera3DType = Camera3DType::PerspectiveFixed;
            break;
        case Camera3DType::PerspectiveFixed:
            camera3DType = Camera3DType::PerspectiveMobile;
            break;
        case Camera3DType::PerspectiveMobile:
            camera3DType = Camera3DType::FirstPerson;
            if (!gPlayers.empty() && gPlayers[0] != nullptr) {
                firstPersonYaw = facingKeyToYawRadians(gPlayers[0]->facingDirKey);
            }
            firstPersonPitch = -0.18f;
            firstPersonMouseInitialized = false;
            break;
    }

    const bool shouldCaptureFirstPersonMouse =
        (viewMode == ViewMode::Mode3D && camera3DType == Camera3DType::FirstPerson && window != nullptr);
    if (window != nullptr) {
        glfwSetInputMode(window, GLFW_CURSOR, shouldCaptureFirstPersonMouse ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    }
    firstPersonCursorLocked = shouldCaptureFirstPersonMouse;

    refreshWindowTitle();
    std::cout << "[Render] 3D camera -> " << camera3DTypeToString(camera3DType) << "\n";
}

void Game::refreshWindowTitle() const {
    if (window == nullptr) {
        return;
    }
    const std::string title = buildWindowTitle(viewMode, camera3DType);
    glfwSetWindowTitle(window, title.c_str());
}

Game::~Game() {
    for (auto* b : gBombs) {
        delete b;
    }
    gBombs.clear();

    for (auto* enemy : gEnemies) {
        delete enemy;
    }
    gEnemies.clear();

    for (auto* p : gPlayers) {
        delete p;
    }
    gPlayers.clear();

    if (gameMap != nullptr) {
        delete gameMap;
        gameMap = nullptr;
    }

    if (EBO != 0) {
        glDeleteBuffers(1, &EBO);
        EBO = 0;
    }
    if (VBO != 0) {
        glDeleteBuffers(1, &VBO);
        VBO = 0;
    }
    if (VAO != 0) {
        glDeleteVertexArrays(1, &VAO);
        VAO = 0;
    }

    if (actorGlbTexture != 0) {
        glDeleteTextures(1, &actorGlbTexture);
        actorGlbTexture = 0;
    }
    if (leonGlbTexture != 0) {
        glDeleteTextures(1, &leonGlbTexture);
        leonGlbTexture = 0;
    }
    if (fantasmaGlbTexture != 0) {
        glDeleteTextures(1, &fantasmaGlbTexture);
        fantasmaGlbTexture = 0;
    }
    if (bebeGlbTexture != 0) {
        glDeleteTextures(1, &bebeGlbTexture);
        bebeGlbTexture = 0;
    }
    if (babosaGlbTexture != 0) {
        glDeleteTextures(1, &babosaGlbTexture);
        babosaGlbTexture = 0;
    }
    if (bombGlbTexture != 0) {
        glDeleteTextures(1, &bombGlbTexture);
        bombGlbTexture = 0;
    }
    if (flameGlbTexture != 0) {
        glDeleteTextures(1, &flameGlbTexture);
        flameGlbTexture = 0;
    }
    if (flamePowerUpGlbTexture != 0) {
        glDeleteTextures(1, &flamePowerUpGlbTexture);
        flamePowerUpGlbTexture = 0;
    }
    if (speedPowerUpGlbTexture != 0) {
        glDeleteTextures(1, &speedPowerUpGlbTexture);
        speedPowerUpGlbTexture = 0;
    }
    if (kingBomberGlbTexture != 0) {
        glDeleteTextures(1, &kingBomberGlbTexture);
        kingBomberGlbTexture = 0;
    }
    if (dronAzulGlbTexture != 0) {
        glDeleteTextures(1, &dronAzulGlbTexture);
        dronAzulGlbTexture = 0;
    }
    if (dronRosaGlbTexture != 0) {
        glDeleteTextures(1, &dronRosaGlbTexture);
        dronRosaGlbTexture = 0;
    }
    if (dronVerdeGlbTexture != 0) {
        glDeleteTextures(1, &dronVerdeGlbTexture);
        dronVerdeGlbTexture = 0;
    }
    if (dronAmarilloGlbTexture != 0) {
        glDeleteTextures(1, &dronAmarilloGlbTexture);
        dronAmarilloGlbTexture = 0;
    }
    if (solGlbTexture != 0) {
        glDeleteTextures(1, &solGlbTexture);
        solGlbTexture = 0;
    }
    if (dragonGlbTexture != 0) {
        glDeleteTextures(1, &dragonGlbTexture);
        dragonGlbTexture = 0;
    }

    ResourceManager::clear();

    texture = 0;
    mapTexture = 0;
    horizonTexture = 0;
    enemyTexture = 0;

    shader = 0;
    shader3D = 0;
    shader3DTextured = 0;
    cubeVAO = cubeVBO = cubeEBO = 0;
    cubeIndexCount = 0;
    sphereVAO = sphereVBO = sphereEBO = 0;
    sphereIndexCount = 0;
    actorGlbVAO = actorGlbVBO = actorGlbEBO = 0;
    actorGlbIndexCount = 0;
    actorGlbTexture = 0;
    leonGlbVAO = leonGlbVBO = leonGlbEBO = 0;
    leonGlbIndexCount = 0;
    leonGlbTexture = 0;
    fantasmaGlbVAO = fantasmaGlbVBO = fantasmaGlbEBO = 0;
    fantasmaGlbIndexCount = 0;
    fantasmaGlbTexture = 0;
    bebeGlbVAO = bebeGlbVBO = bebeGlbEBO = 0;
    bebeGlbIndexCount = 0;
    bebeGlbTexture = 0;
    babosaGlbVAO = babosaGlbVBO = babosaGlbEBO = 0;
    babosaGlbIndexCount = 0;
    babosaGlbTexture = 0;
    bombGlbVAO = bombGlbVBO = bombGlbEBO = 0;
    bombGlbIndexCount = 0;
    bombGlbTexture = 0;
    flameGlbVAO = flameGlbVBO = flameGlbEBO = 0;
    flameGlbIndexCount = 0;
    flameGlbTexture = 0;
    flamePowerUpGlbVAO = flamePowerUpGlbVBO = flamePowerUpGlbEBO = 0;
    flamePowerUpGlbIndexCount = 0;
    flamePowerUpGlbTexture = 0;
    speedPowerUpGlbVAO = speedPowerUpGlbVBO = speedPowerUpGlbEBO = 0;
    speedPowerUpGlbIndexCount = 0;
    speedPowerUpGlbTexture = 0;
    kingBomberGlbVAO = kingBomberGlbVBO = kingBomberGlbEBO = 0;
    kingBomberGlbIndexCount = 0;
    kingBomberGlbTexture = 0;
    dronAzulGlbVAO = dronAzulGlbVBO = dronAzulGlbEBO = 0;
    dronAzulGlbIndexCount = 0;
    dronAzulGlbTexture = 0;
    dronRosaGlbVAO = dronRosaGlbVBO = dronRosaGlbEBO = 0;
    dronRosaGlbIndexCount = 0;
    dronRosaGlbTexture = 0;
    dronVerdeGlbVAO = dronVerdeGlbVBO = dronVerdeGlbEBO = 0;
    dronVerdeGlbIndexCount = 0;
    dronVerdeGlbTexture = 0;
    dronAmarilloGlbVAO = dronAmarilloGlbVBO = dronAmarilloGlbEBO = 0;
    dronAmarilloGlbIndexCount = 0;
    dronAmarilloGlbTexture = 0;
    solGlbVAO = solGlbVBO = solGlbEBO = 0;
    solGlbIndexCount = 0;
    solGlbTexture = 0;
    dragonGlbVAO = dragonGlbVBO = dragonGlbEBO = 0;
    dragonGlbIndexCount = 0;
    dragonGlbTexture = 0;
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
        loadLevel(currentLevelIndex, /*preserveLivesAndScore=*/false);
    }

    if (!gPlayers.empty() && gPlayers[0] != nullptr) {
        firstPersonYaw = facingKeyToYawRadians(gPlayers[0]->facingDirKey);
    }
    firstPersonPitch = -0.18f;
    firstPersonMouseInitialized = false;
    firstPersonCursorLocked = false;
    if (window != nullptr) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    refreshWindowTitle();
}

// Lee teclas y aplica acciones (movimiento, animaci├│n y colocar bombas).
void Game::processInput() {
    // Atajos globales de ventana (también disponibles en intro/menu).
    if (this->keys[GLFW_KEY_TAB] == GLFW_PRESS || this->keys[GLFW_KEY_F11] == GLFW_PRESS) {
        this->keys[GLFW_KEY_TAB] = GLFW_REPEAT;
        this->keys[GLFW_KEY_F11] = GLFW_REPEAT;
        toggleFullscreen(this->window);
    }

    if (this->keys[GLFW_KEY_F10] == GLFW_PRESS && this->window != nullptr) {
        this->keys[GLFW_KEY_F10] = GLFW_REPEAT;
        glfwIconifyWindow(this->window);
    }

    if (this->state == GAME_INTRO) {
        if (this->window != nullptr && this->firstPersonCursorLocked) {
            glfwSetInputMode(this->window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            this->firstPersonCursorLocked = false;
            this->firstPersonMouseInitialized = false;
        }
        return;
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
        if (this->window != nullptr && this->firstPersonCursorLocked) {
            glfwSetInputMode(this->window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            this->firstPersonCursorLocked = false;
            this->firstPersonMouseInitialized = false;
        }
        menuScreen.processInputMenu(this->keys);
        return;
    }

    const bool shouldCaptureFirstPersonMouse =
        (this->viewMode == ViewMode::Mode3D &&
         this->camera3DType == Camera3DType::FirstPerson &&
         this->window != nullptr);

    const bool keepScreenFacingForPerspective3D =
        (this->viewMode == ViewMode::Mode3D &&
         (this->camera3DType == Camera3DType::PerspectiveFixed ||
          this->camera3DType == Camera3DType::PerspectiveMobile));

    if (this->window != nullptr) {
        if (shouldCaptureFirstPersonMouse && !this->firstPersonCursorLocked) {
            glfwSetInputMode(this->window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            this->firstPersonCursorLocked = true;
            this->firstPersonMouseInitialized = false;
        } else if (!shouldCaptureFirstPersonMouse && this->firstPersonCursorLocked) {
            glfwSetInputMode(this->window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            this->firstPersonCursorLocked = false;
            this->firstPersonMouseInitialized = false;
        }
    }

    if (this->state != GAME_PLAYING) return;

    if (this->viewMode != ViewMode::Mode3D) {
        this->surpriseKey3TapCount = 0;
        this->surpriseKey3LastTapTime = -10.0;
    } else if (this->keys[GLFW_KEY_3] == GLFW_PRESS) {
        this->keys[GLFW_KEY_3] = GLFW_REPEAT;

        const double now = glfwGetTime();
        const double kDoubleTapWindowSeconds = 0.45;

        if ((now - this->surpriseKey3LastTapTime) <= kDoubleTapWindowSeconds) {
            this->surpriseKey3TapCount += 1;
        } else {
            this->surpriseKey3TapCount = 1;
        }
        this->surpriseKey3LastTapTime = now;

        if (this->surpriseKey3TapCount >= 2) {
            if (horizonTexture == 0) {
                horizonTexture = loadHorizonTextureWithFallback();
            }

            if (horizonTexture != 0) {
                this->surpriseHorizonVisible3D = true;
                std::cout << "[Render] Fondo 3D sorpresa revelado con tecla 3\n";
            } else {
                std::cout << "[Render] No se pudo revelar el fondo 3D (textura no disponible)\n";
            }

            this->surpriseKey3TapCount = 0;
            this->surpriseKey3LastTapTime = -10.0;
        }
    }

    if (shouldCaptureFirstPersonMouse) {
        double mouseX = 0.0;
        double mouseY = 0.0;
        glfwGetCursorPos(this->window, &mouseX, &mouseY);

        if (!this->firstPersonMouseInitialized) {
            this->firstPersonLastMouseX = mouseX;
            this->firstPersonLastMouseY = mouseY;
            this->firstPersonMouseInitialized = true;
        } else {
            const double deltaX = mouseX - this->firstPersonLastMouseX;
            const double deltaY = mouseY - this->firstPersonLastMouseY;
            this->firstPersonLastMouseX = mouseX;
            this->firstPersonLastMouseY = mouseY;

            this->firstPersonYaw -= (float)deltaX * 0.0030f;
            this->firstPersonPitch -= (float)deltaY * 0.0022f;
            this->firstPersonPitch = std::max(-1.30f, std::min(1.10f, this->firstPersonPitch));

            const float kPi = 3.14159265359f;
            const float kTwoPi = 6.28318530718f;
            if (this->firstPersonYaw > kPi) {
                this->firstPersonYaw -= kTwoPi;
            } else if (this->firstPersonYaw < -kPi) {
                this->firstPersonYaw += kTwoPi;
            }
        }
    }

    if (gPlayers.empty() || gPlayers[0] == nullptr) return;
    Player* p1 = gPlayers[0];

    // ======================= Jugador 1 (blanco): Flechas =======================

    if (p1->isAlive()) {
        const bool up = (this->keys[GLFW_KEY_UP] >= GLFW_PRESS);
        const bool down = (this->keys[GLFW_KEY_DOWN] >= GLFW_PRESS);
        const bool left = (this->keys[GLFW_KEY_LEFT] >= GLFW_PRESS);
        const bool right = (this->keys[GLFW_KEY_RIGHT] >= GLFW_PRESS);

        const int pressedCount = (up ? 1 : 0) + (down ? 1 : 0) + (left ? 1 : 0) + (right ? 1 : 0);
        if (pressedCount == 0) {
            p1->isWalking = false;

            if (this->lastDirKey != GLFW_KEY_UNKNOWN) {
                const GLint idleFacingDir = keepScreenFacingForPerspective3D
                    ? this->lastDirKey
                    : remapDirectionFor3DCamera(this, this->lastDirKey);
                p1->facingDirKey = idleFacingDir;
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
                if (keyToUse == GLFW_KEY_UNKNOWN) {
                    if (up) keyToUse = GLFW_KEY_UP;
                    else if (down) keyToUse = GLFW_KEY_DOWN;
                    else if (left) keyToUse = GLFW_KEY_LEFT;
                    else if (right) keyToUse = GLFW_KEY_RIGHT;
                }
            }

            if (keyToUse != GLFW_KEY_UNKNOWN) {
                const GLint mappedDir = remapDirectionFor3DCamera(this, keyToUse);
                const Move mappedMove = directionKeyToMove(mappedDir);
                if (mappedMove != MOVE_NONE) {
                    p1->UpdateSprite(mappedMove, gameMap, this->deltaTime);

                    const GLint facingDir = keepScreenFacingForPerspective3D
                        ? keyToUse
                        : mappedDir;
                    if (!p1->isWalking || p1->facingDirKey != facingDir) {
                        p1->walkTimer = 0.0f;
                        p1->walkPhase = 0;
                    }
                    p1->facingDirKey = facingDir;
                    p1->isWalking = true;
                } else {
                    p1->isWalking = false;
                }
            } else {
                p1->isWalking = false;
            }
        }
    } else {
        p1->isWalking = false;
    }

    // ======================= Jugador 2 (rojo): WASD =======================
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
                    GLint screenDir2 = GLFW_KEY_DOWN;
                    switch (this->lastDirKeyP2) {
                        case GLFW_KEY_W: screenDir2 = GLFW_KEY_UP; break;
                        case GLFW_KEY_S: screenDir2 = GLFW_KEY_DOWN; break;
                        case GLFW_KEY_A: screenDir2 = GLFW_KEY_LEFT; break;
                        case GLFW_KEY_D: screenDir2 = GLFW_KEY_RIGHT; break;
                    }
                    const GLint idleFacingDir2 = keepScreenFacingForPerspective3D
                        ? screenDir2
                        : remapDirectionFor3DCamera(this, screenDir2);
                    p2->facingDirKey = idleFacingDir2;
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
                    if (keyToUse2 == GLFW_KEY_UNKNOWN) {
                        if (up2) keyToUse2 = GLFW_KEY_W;
                        else if (down2) keyToUse2 = GLFW_KEY_S;
                        else if (left2) keyToUse2 = GLFW_KEY_A;
                        else if (right2) keyToUse2 = GLFW_KEY_D;
                    }
                }

                if (keyToUse2 != GLFW_KEY_UNKNOWN) {
                    GLint dir2Screen = GLFW_KEY_DOWN;
                    switch (keyToUse2) {
                        case GLFW_KEY_W: dir2Screen = GLFW_KEY_UP; break;
                        case GLFW_KEY_S: dir2Screen = GLFW_KEY_DOWN; break;
                        case GLFW_KEY_A: dir2Screen = GLFW_KEY_LEFT; break;
                        case GLFW_KEY_D: dir2Screen = GLFW_KEY_RIGHT; break;
                    }

                    const GLint dir2 = remapDirectionFor3DCamera(this, dir2Screen);
                    const Move mov2 = directionKeyToMove(dir2);

                    if (mov2 != MOVE_NONE) {
                        p2->UpdateSprite(mov2, gameMap, this->deltaTime);

                        const GLint facingDir2 = keepScreenFacingForPerspective3D
                            ? dir2Screen
                            : dir2;
                        if (!p2->isWalking || p2->facingDirKey != facingDir2) {
                            p2->walkTimer = 0.0f;
                            p2->walkPhase = 0;
                        }
                        p2->facingDirKey = facingDir2;
                        p2->isWalking = true;
                    } else {
                        p2->isWalking = false;
                    }
                } else {
                    p2->isWalking = false;
                }
            }
        }
    }

    // Debug: forzar avance al siguiente nivel.
    if (this->keys[GLFW_KEY_F3] == GLFW_PRESS) {
        this->keys[GLFW_KEY_F3] = GLFW_REPEAT;
        advanceToNextLevel();
        return;
    }

    // ======================= Bombas (Jugador 1) =======================
    if (p1->isAlive() && !p1->isGameOver() && this->keys[GLFW_KEY_RIGHT_CONTROL] == GLFW_PRESS) {
        this->keys[GLFW_KEY_RIGHT_CONTROL] = GLFW_REPEAT;

        if (p1->canPlaceBomb()) {
            int bombRow, bombCol;
            gameMap->ndcToGrid(p1->position, bombRow, bombCol);

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

    if (p1->isAlive() && p1->hasRemoteControl && this->keys[GLFW_KEY_RIGHT_ALT] == GLFW_PRESS) {
        this->keys[GLFW_KEY_RIGHT_ALT] = GLFW_REPEAT;
        for (auto* b : gBombs) {
            if (b && b->ownerIndex == p1->playerId && b->state == BombState::FUSE) {
                b->detonate();
                break;
            }
        }
    }

    // ======================= Bombas (Jugador 2) =======================
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

    // Rotacion orbital de camaras 3D no-first-person con arrastre del raton.
    const bool canOrbitCamera = (this->viewMode == ViewMode::Mode3D
                              && (this->camera3DType == Camera3DType::PerspectiveFixed ||
                                  this->camera3DType == Camera3DType::PerspectiveMobile)
                              && this->window != nullptr);
    if (canOrbitCamera) {
        double mouseX = 0.0;
        double mouseY = 0.0;
        glfwGetCursorPos(this->window, &mouseX, &mouseY);

        const bool dragPressed =
            (glfwGetMouseButton(this->window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) ||
            (glfwGetMouseButton(this->window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS);

        if (dragPressed) {
            if (!this->cameraOrbitDragging) {
                this->cameraOrbitDragging = true;
                this->cameraOrbitLastMouseX = mouseX;
            } else {
                const double deltaX = mouseX - this->cameraOrbitLastMouseX;
                this->cameraOrbitLastMouseX = mouseX;
                this->cameraOrbitYaw -= (float)deltaX * 0.0080f;
            }
        } else {
            this->cameraOrbitDragging = false;
        }
    } else {
        this->cameraOrbitDragging = false;
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
            } else if (currentCinematicType == CinematicType::HistoryEnd) {
                // No hay rankings ni nada, así que simplemente volvemos al menú.
                returnToMenuFromGame(/*resetRun=*/true);
            } else if (currentCinematicType == CinematicType::LevelStart) {
                // Después de la cinemática del nivel, cargar el nivel y transicionar a GAME_PLAYING
                if (loadLevelPending) {
                    bool preserve = (currentLevelIndex > 0); // Solo preservar vidas/puntuación a partir del nivel 2
                    loadLevel(currentLevelIndex, /*preserveLivesAndScore=*/preserve);
                    loadLevelPending = false;
                    this->state = GAME_PLAYING;
                }
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
                // Cinematica final de historia si el modo de juego corresponde
                advanceToNextLevel();
                return;
            }
        }
    }

    // Decrementar el timer del nivel
    levelTimeRemaining -= deltaTime;
    if (levelTimeRemaining < 0.0f) levelTimeRemaining = 0.0f;
}

// Renderiza mapa, bombas, jugadores y enemigos en 3D.
void Game::render3D() {
    glEnable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (surpriseHorizonVisible3D && horizonTexture != 0 && shader != 0 && VAO != 0) {
        glDepthMask(GL_FALSE);
        glDisable(GL_DEPTH_TEST);

        glUseProgram(shader);
        const glm::mat4 identity(1.0f);
        const glm::vec4 fullUv(0.0f, 0.0f, 1.0f, 1.0f);
        const glm::vec4 fullTint(1.0f, 1.0f, 1.0f, 1.0f);

        glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, glm::value_ptr(identity));
        glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(identity));
        glUniform4fv(uniformUvRect, 1, glm::value_ptr(fullUv));
        glUniform4fv(uniformTintColor, 1, glm::value_ptr(fullTint));
        glUniform1f(uniformFlipX, 0.0f);
        glUniform1i(uniformTexture, 0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, horizonTexture);
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
    }

    glUseProgram(shader3D);

    const float safeHeight = (HEIGHT > 0) ? (float)HEIGHT : 1.0f;
    const float aspect = std::max(0.01f, (float)WIDTH / safeHeight);
    const float mapHalfWidth = std::max(2.0f, (float)gameMap->getCols() * 0.5f);
    const float mapHalfDepth = std::max(2.0f, (float)gameMap->getRows() * 0.5f);
    const float mapRadius = std::max(mapHalfWidth, mapHalfDepth);
    const glm::vec3 mapCenter(0.0f, 0.0f, 0.0f);

    glm::vec3 trackedPlayerCenter = mapCenter + glm::vec3(0.0f, 0.55f, 0.0f);
    glm::vec3 trackedPlayerForward(0.0f, 0.0f, 1.0f);
    if (!gPlayers.empty() && gPlayers[0] != nullptr) {
        trackedPlayerCenter = ndcToWorld3D(gameMap, gPlayers[0]->position, 0.55f);
        trackedPlayerForward = facingKeyToForward(gPlayers[0]->facingDirKey);
    }

    glm::vec3 cameraPos(mapRadius * 0.70f, mapRadius * 1.55f + 3.0f, mapRadius * 1.25f + 2.5f);
    glm::vec3 cameraTarget(mapCenter);
    glm::vec3 up(0.0f, 1.0f, 0.0f);

    if (camera3DType == Camera3DType::OrthographicFixed) {
        cameraPos = glm::vec3(0.0f, mapRadius * 2.4f + 2.0f, 0.01f);
        cameraTarget = mapCenter;
        up = glm::vec3(0.0f, 0.0f, -1.0f);
    } else if (camera3DType == Camera3DType::PerspectiveFixed) {
        const float orbitRadius = mapRadius * 1.10f + 2.6f;
        const float cameraHeight = mapRadius * 1.35f + 3.0f;
        cameraPos = glm::vec3(std::sin(cameraOrbitYaw) * orbitRadius,
                              cameraHeight,
                              std::cos(cameraOrbitYaw) * orbitRadius);
        cameraTarget = mapCenter;
    } else if (camera3DType == Camera3DType::PerspectiveMobile) {
        // Camara de seguimiento orbital: mantiene foco en jugador y rota con el yaw de camara.
        const float followDistance = 6.8f;
        const float followHeight = 4.2f;
        const glm::vec3 followOffset(std::sin(cameraOrbitYaw) * followDistance,
                                     followHeight,
                                     std::cos(cameraOrbitYaw) * followDistance);
        cameraPos = trackedPlayerCenter + followOffset;
        cameraTarget = trackedPlayerCenter + glm::vec3(0.0f, 0.75f, 0.0f);
    } else if (camera3DType == Camera3DType::FirstPerson) {
        // Altura de ojos mas baja para mantener los muros delante y evitar ver por encima.
        const glm::vec3 firstPersonForward = firstPersonLookToForward(this->firstPersonYaw, this->firstPersonPitch);
        const glm::vec3 eye = trackedPlayerCenter + glm::vec3(0.0f, 0.34f, 0.0f) - firstPersonForward * 0.10f;
        cameraPos = eye;
        cameraTarget = eye + firstPersonForward * 2.8f;
    }

    const glm::mat4 view = glm::lookAt(cameraPos, cameraTarget, up);
    glm::mat4 projection(1.0f);
    if (camera3DType == Camera3DType::OrthographicFixed) {
        const float safeAspect = std::max(aspect, 0.01f);
        float halfH = std::max(4.0f, (float)gameMap->getRows() * 0.38f);
        float halfW = halfH * safeAspect;
        const float minHalfW = std::max(4.0f, (float)gameMap->getCols() * 0.38f);
        if (halfW < minHalfW) {
            halfW = minHalfW;
            halfH = halfW / safeAspect;
        }
        projection = glm::ortho(-halfW, halfW, -halfH, halfH, 0.1f, 100.0f);
    } else {
        float fovDegrees = 55.0f;
        if (camera3DType == Camera3DType::PerspectiveFixed) {
            fovDegrees = 50.0f;
        } else if (camera3DType == Camera3DType::PerspectiveMobile) {
            fovDegrees = 62.0f;
        } else if (camera3DType == Camera3DType::FirstPerson) {
            fovDegrees = 72.0f;
        }
        projection = glm::perspective(glm::radians(fovDegrees), aspect, 0.05f, 140.0f);
    }

    const glm::vec3 keyLightPos = mapCenter + glm::vec3(mapRadius * 0.30f, mapRadius * 1.85f + 3.5f, mapRadius * 0.24f);
    const glm::vec3 keyLightColor(1.0f, 0.97f, 0.92f);

    glUniformMatrix4fv(uniform3DView, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(uniform3DProjection, 1, GL_FALSE, glm::value_ptr(projection));
    glUniform3fv(uniform3DLightPos, 1, glm::value_ptr(keyLightPos));
    glUniform3fv(uniform3DViewPos, 1, glm::value_ptr(cameraPos));
    glUniform3fv(uniform3DLightColor, 1, glm::value_ptr(keyLightColor));
    glUniform1f(uniform3DAmbientStrength, 0.34f);
    glUniform1f(uniform3DSpecularStrength, 0.38f);
    glUniform1f(uniform3DShininess, 24.0f);

    auto drawMesh3D = [&](GLuint vao, GLsizei indexCount, const glm::vec3& center, const glm::vec3& scale, const glm::vec3& color) {
        if (vao == 0 || indexCount <= 0) {
            return;
        }

        glm::mat4 model(1.0f);
        model = glm::translate(model, center);
        model = glm::scale(model, scale);
        glUniformMatrix4fv(uniform3DModel, 1, GL_FALSE, glm::value_ptr(model));
        glUniform3fv(uniform3DColor, 1, glm::value_ptr(color));
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    };

    auto drawSpriteBillboard3D = [&](GLuint texId,
                                     const glm::vec4& uvRect,
                                     const glm::vec3& feetPos,
                                     float width,
                                     float height,
                                     float flipX,
                                     const glm::vec4& tint) {
        if (texId == 0 || shader == 0 || VAO == 0) {
            return;
        }

        const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
        glm::vec3 toCamera = cameraPos - feetPos;
        if (glm::length(toCamera) < 0.0001f) {
            toCamera = glm::vec3(0.0f, 0.0f, 1.0f);
        }

        const glm::vec3 viewDir = glm::normalize(toCamera);
        glm::vec3 right = glm::cross(worldUp, viewDir);
        if (glm::length(right) < 0.0001f) {
            right = glm::vec3(1.0f, 0.0f, 0.0f);
        } else {
            right = glm::normalize(right);
        }
        const glm::vec3 forward = glm::normalize(glm::cross(right, worldUp));

        const float halfW = width * 0.5f;
        const float halfH = height * 0.5f;

        glm::mat4 model(1.0f);
        model[0] = glm::vec4(right * halfW, 0.0f);
        model[1] = glm::vec4(worldUp * halfH, 0.0f);
        model[2] = glm::vec4(forward * 0.5f, 0.0f);
        model[3] = glm::vec4(feetPos + worldUp * halfH, 1.0f);

        glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
        glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));
        glUniform1f(uniformFlipX, flipX);
        glUniform4fv(uniformTintColor, 1, glm::value_ptr(tint));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texId);
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    };

    // Mapa: cubos por tile.
    for (int r = 0; r < gameMap->getRows(); ++r) {
        for (int c = 0; c < gameMap->getCols(); ++c) {
            const bool walkable = gameMap->isWalkable(r, c);
            const bool destructible = gameMap->isDestructible(r, c);
            const bool checker = (((r + c) % 2) == 0);

            float h = 0.08f;
            glm::vec3 color = checker ? glm::vec3(0.20f, 0.22f, 0.24f)
                                      : glm::vec3(0.23f, 0.25f, 0.27f);
            if (!walkable) {
                h = 1.00f;
                color = destructible
                    ? (checker ? glm::vec3(0.78f, 0.47f, 0.22f) : glm::vec3(0.72f, 0.42f, 0.18f))
                    : (checker ? glm::vec3(0.36f, 0.37f, 0.43f) : glm::vec3(0.32f, 0.33f, 0.39f));
            }

            const glm::vec3 center = gridToWorld3D(gameMap, r, c, h * 0.5f);
            drawMesh3D(cubeVAO, cubeIndexCount, center, glm::vec3(0.95f, h, 0.95f), color);
        }
    }

    auto drawSpriteTileQuad3D = [&](const glm::vec3& center,
                                    float halfW,
                                    float halfH,
                                    float yawDegrees,
                                    const glm::vec4& uvRect,
                                    const glm::vec4& tint) {
        glm::mat4 model(1.0f);
        model = glm::translate(model, center);
        model = glm::rotate(model, glm::radians(yawDegrees), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(halfW, halfH, 1.0f));

        glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
        glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));
        glUniform4fv(uniformTintColor, 1, glm::value_ptr(tint));
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    };

    // Decal de sprite Stage1 sobre suelos, techos y laterales de bloques del mapa.
    glUseProgram(shader);
    const glm::mat4 spriteProjection3DMap = projection * view;
    glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, glm::value_ptr(spriteProjection3DMap));
    glUniform1i(uniformTexture, 0);
    glUniform1f(uniformFlipX, 0.0f);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mapTexture);
    glBindVertexArray(VAO);

    glm::vec4 indestructibleSideUv(0.0f, 0.0f, 1.0f, 1.0f);
    const bool hasIndestructibleSideUv = gameMap->getUvRectForSpriteId(8, indestructibleSideUv);

    const int mapRows = gameMap->getRows();
    const int mapCols = gameMap->getCols();
    const float topHalfSize = 0.475f;
    const float sideOutward = 0.0035f;

    auto sideIsVisible = [&](int row, int col) {
        if (row < 0 || row >= mapRows || col < 0 || col >= mapCols) {
            return true;
        }
        return gameMap->isWalkable(row, col);
    };

    for (int r = 0; r < gameMap->getRows(); ++r) {
        for (int c = 0; c < gameMap->getCols(); ++c) {
            const bool walkable = gameMap->isWalkable(r, c);
            const BlockType blockType = gameMap->getBlockType(r, c);
            const bool isBorderTile = (r == 0 || c == 0 || r == mapRows - 1 || c == mapCols - 1);
            const float h = walkable ? 0.08f : 1.00f;

            glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
            if (!gameMap->getUvRectForTile(r, c, uvRect)) {
                continue;
            }

            // Cara superior (incluye suelos en 3D).
            {
                const glm::vec3 centerTop = gridToWorld3D(gameMap, r, c, h + sideOutward);
                glm::mat4 model(1.0f);
                model = glm::translate(model, centerTop);
                model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
                model = glm::scale(model, glm::vec3(topHalfSize, topHalfSize, 1.0f));

                glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
                glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));
                glUniform4fv(uniformTintColor, 1, glm::value_ptr(glm::vec4(1.0f)));
                glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            }

            // Caras laterales para bloques no walkable.
            if (!walkable) {
                glm::vec4 sideUv = uvRect;
                if (blockType == BlockType::BARRIER && isBorderTile && hasIndestructibleSideUv) {
                    sideUv = indestructibleSideUv;
                }

                const float halfH = h * 0.5f;
                const glm::vec3 tileCenter = gridToWorld3D(gameMap, r, c, halfH);
                const glm::vec4 sideTint(1.0f, 1.0f, 1.0f, 0.98f);

                if (sideIsVisible(r - 1, c)) {
                    drawSpriteTileQuad3D(tileCenter + glm::vec3(0.0f, 0.0f, -topHalfSize - sideOutward), topHalfSize, halfH, 180.0f, sideUv, sideTint);
                }
                if (sideIsVisible(r + 1, c)) {
                    drawSpriteTileQuad3D(tileCenter + glm::vec3(0.0f, 0.0f, topHalfSize + sideOutward), topHalfSize, halfH, 0.0f, sideUv, sideTint);
                }
                if (sideIsVisible(r, c - 1)) {
                    drawSpriteTileQuad3D(tileCenter + glm::vec3(-topHalfSize - sideOutward, 0.0f, 0.0f), topHalfSize, halfH, 90.0f, sideUv, sideTint);
                }
                if (sideIsVisible(r, c + 1)) {
                    drawSpriteTileQuad3D(tileCenter + glm::vec3(topHalfSize + sideOutward, 0.0f, 0.0f), topHalfSize, halfH, -90.0f, sideUv, sideTint);
                }
            }
        }
    }

    glUseProgram(shader3D);
    glUniformMatrix4fv(uniform3DView, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(uniform3DProjection, 1, GL_FALSE, glm::value_ptr(projection));
    glUniform3fv(uniform3DLightPos, 1, glm::value_ptr(keyLightPos));
    glUniform3fv(uniform3DViewPos, 1, glm::value_ptr(cameraPos));
    glUniform3fv(uniform3DLightColor, 1, glm::value_ptr(keyLightColor));
    glUniform1f(uniform3DAmbientStrength, 0.34f);
    glUniform1f(uniform3DSpecularStrength, 0.38f);
    glUniform1f(uniform3DShininess, 24.0f);

    const bool hasSphereMesh = (sphereVAO != 0 && sphereIndexCount > 0);
    const GLuint sphereOrCubeVAO = hasSphereMesh ? sphereVAO : cubeVAO;
    const GLsizei sphereOrCubeIndexCount = hasSphereMesh ? sphereIndexCount : cubeIndexCount;

    // Jugadores: sombra en suelo (el sprite real se dibuja después como billboard 2.5D).
    for (std::size_t i = 0; i < gPlayers.size(); ++i) {
        Player* p = gPlayers[i];
        if (!p || !p->isAlive()) continue;

        const glm::vec3 feet = ndcToWorld3D(gameMap, p->position, 0.02f);
        drawMesh3D(sphereOrCubeVAO, sphereOrCubeIndexCount, glm::vec3(feet.x, 0.03f, feet.z), glm::vec3(0.46f, 0.02f, 0.46f), glm::vec3(0.05f, 0.05f, 0.05f));
    }

    // Enemigos: sombra en suelo (el sprite real se dibuja después como billboard 2.5D).
    for (auto* enemy : gEnemies) {
        if (!enemy || enemy->lifeState != EnemyLifeState::Alive) continue;

        float shadowRadius = 0.44f;
        if (dynamic_cast<KingBomber*>(enemy) != nullptr) {
            shadowRadius = 0.56f;
        } else if (dynamic_cast<DronBombardero*>(enemy) != nullptr) {
            shadowRadius = 0.28f;
        }

        const glm::vec3 feet = ndcToWorld3D(gameMap, enemy->position, 0.02f);
        drawMesh3D(sphereOrCubeVAO,
                   sphereOrCubeIndexCount,
                   glm::vec3(feet.x, 0.03f, feet.z),
                   glm::vec3(shadowRadius, 0.02f, shadowRadius),
                   glm::vec3(0.05f, 0.05f, 0.05f));
    }

    const GLuint fuseMesh = sphereOrCubeVAO;
    const GLsizei fuseMeshIndexCount = sphereOrCubeIndexCount;
    const bool canRenderBombGlb =
        (bombGlbVAO != 0 && bombGlbIndexCount > 0 && bombGlbTexture != 0 && shader3DTextured != 0);
    const bool canRenderFlameGlb =
        (flameGlbVAO != 0 && flameGlbIndexCount > 0 && flameGlbTexture != 0 && shader3DTextured != 0);
    const bool canRenderFlamePowerUpGlb =
        (flamePowerUpGlbVAO != 0 && flamePowerUpGlbIndexCount > 0 && flamePowerUpGlbTexture != 0 && shader3DTextured != 0);
    const bool canRenderSpeedPowerUpGlb =
        (speedPowerUpGlbVAO != 0 && speedPowerUpGlbIndexCount > 0 && speedPowerUpGlbTexture != 0 && shader3DTextured != 0);
    const bool canRenderKingBomberGlb =
        (kingBomberGlbVAO != 0 && kingBomberGlbIndexCount > 0 && kingBomberGlbTexture != 0 && shader3DTextured != 0);
    const bool canRenderDronAzulGlb =
        (dronAzulGlbVAO != 0 && dronAzulGlbIndexCount > 0 && dronAzulGlbTexture != 0 && shader3DTextured != 0);
    const bool canRenderDronRosaGlb =
        (dronRosaGlbVAO != 0 && dronRosaGlbIndexCount > 0 && dronRosaGlbTexture != 0 && shader3DTextured != 0);
    const bool canRenderDronVerdeGlb =
        (dronVerdeGlbVAO != 0 && dronVerdeGlbIndexCount > 0 && dronVerdeGlbTexture != 0 && shader3DTextured != 0);
    const bool canRenderDronAmarilloGlb =
        (dronAmarilloGlbVAO != 0 && dronAmarilloGlbIndexCount > 0 && dronAmarilloGlbTexture != 0 && shader3DTextured != 0);
    const bool canRenderAnyDronGlb =
        (canRenderDronAzulGlb || canRenderDronRosaGlb || canRenderDronVerdeGlb || canRenderDronAmarilloGlb);

    auto usesFireSprite = [](const Enemy* enemy) -> bool {
        return enemy && enemy->currentSpriteName.find(".fuego.") != std::string::npos;
    };

    auto hiddenByKingBlink = [](const Enemy* enemy) -> bool {
        if (!enemy) return false;
        if (!IsKingPreBattleLockActive() || IsKingPreBattleBlinkVisible()) return false;
        return (dynamic_cast<const KingBomber*>(enemy) != nullptr) ||
               (dynamic_cast<const DronBombardero*>(enemy) != nullptr);
    };

    auto resolveDroneRenderResource = [&](const Enemy* enemy,
                                          GLuint& outVao,
                                          GLsizei& outIndexCount,
                                          GLuint& outTexture) -> bool {
        outVao = 0;
        outIndexCount = 0;
        outTexture = 0;

        if (!enemy) return false;

        std::string skin;
        if (const auto* drone = dynamic_cast<const DronBombardero*>(enemy)) {
            skin = drone->getSkinBase();
        }
        if (skin.empty()) {
            skin = enemy->spriteBaseId;
        }
        if (skin.empty()) {
            const std::size_t dot = enemy->currentSpriteName.find('.');
            skin = (dot == std::string::npos) ? enemy->currentSpriteName : enemy->currentSpriteName.substr(0, dot);
        }

        if (skin == "dronazul" && canRenderDronAzulGlb) {
            outVao = dronAzulGlbVAO;
            outIndexCount = dronAzulGlbIndexCount;
            outTexture = dronAzulGlbTexture;
            return true;
        }
        if (skin == "dronrosa" && canRenderDronRosaGlb) {
            outVao = dronRosaGlbVAO;
            outIndexCount = dronRosaGlbIndexCount;
            outTexture = dronRosaGlbTexture;
            return true;
        }
        if (skin == "dronverde" && canRenderDronVerdeGlb) {
            outVao = dronVerdeGlbVAO;
            outIndexCount = dronVerdeGlbIndexCount;
            outTexture = dronVerdeGlbTexture;
            return true;
        }
        if (skin == "dronamarillo" && canRenderDronAmarilloGlb) {
            outVao = dronAmarilloGlbVAO;
            outIndexCount = dronAmarilloGlbIndexCount;
            outTexture = dronAmarilloGlbTexture;
            return true;
        }

        return false;
    };

    // Bombas y explosiones en 3D.
    for (auto* b : gBombs) {
        if (!b || b->state == BombState::DONE) continue;

        if (b->state == BombState::FUSE) {
            if (canRenderBombGlb) {
                const glm::vec3 feet = ndcToWorld3D(gameMap, b->position, 0.02f);
                drawMesh3D(sphereOrCubeVAO,
                           sphereOrCubeIndexCount,
                           glm::vec3(feet.x, 0.03f, feet.z),
                           glm::vec3(0.34f, 0.02f, 0.34f),
                           glm::vec3(0.05f, 0.05f, 0.05f));
                continue;
            }

            const glm::vec3 center = ndcToWorld3D(gameMap, b->position, 0.30f);
            drawMesh3D(fuseMesh, fuseMeshIndexCount, center, glm::vec3(0.45f, 0.45f, 0.45f), glm::vec3(0.08f, 0.08f, 0.08f));
            drawMesh3D(cubeVAO, cubeIndexCount, center + glm::vec3(0.0f, 0.34f, 0.0f), glm::vec3(0.06f, 0.18f, 0.06f), glm::vec3(0.95f, 0.70f, 0.20f));
        } else {
            if (canRenderFlameGlb) {
                for (std::size_t i = 0; i < b->explosionSegments.size(); ++i) {
                    const ExplosionSegment& seg = b->explosionSegments[i];
                    const bool isCenter = (seg.baseName == "explosion");
                    const bool isEnd = (seg.baseName == "explosion_end");
                    const float shadowSize = isCenter ? 0.42f : (isEnd ? 0.30f : 0.35f);
                    const glm::vec3 feet = ndcToWorld3D(gameMap, seg.pos, 0.02f);
                    drawMesh3D(sphereOrCubeVAO,
                               sphereOrCubeIndexCount,
                               glm::vec3(feet.x, 0.03f, feet.z),
                               glm::vec3(shadowSize, 0.02f, shadowSize),
                               glm::vec3(0.06f, 0.04f, 0.04f));
                }
                continue;
            }

            for (std::size_t i = 0; i < b->explosionSegments.size(); ++i) {
                const ExplosionSegment& seg = b->explosionSegments[i];
                const bool warmColor = (((int)i + b->animFrame) % 2 == 0);
                const glm::vec3 color = warmColor ? glm::vec3(1.00f, 0.15f, 0.10f)
                                                  : glm::vec3(1.00f, 0.92f, 0.10f);
                const glm::vec3 center = ndcToWorld3D(gameMap, seg.pos, 0.35f);
                const float pulse = 0.44f + 0.08f * std::sin(((float)b->animFrame * 1.15f) + ((float)i * 0.70f));
                drawMesh3D(fuseMesh, fuseMeshIndexCount, center, glm::vec3(pulse, pulse, pulse), color);
            }
        }
    }

    if (canRenderFlameGlb) {
        for (auto* enemy : gEnemies) {
            auto* dragon = dynamic_cast<DragonJoven*>(enemy);
            if (!dragon || dragon->lifeState != EnemyLifeState::Alive || !dragon->isFiringAttack()) {
                continue;
            }

            const int activeSegments = dragon->getActiveFireSegmentCount();
            if (activeSegments <= 0) {
                continue;
            }

            const auto& fireSegments = dragon->getFireSegments();
            const std::size_t drawCount = std::min<std::size_t>((std::size_t)activeSegments, fireSegments.size());
            for (std::size_t i = 0; i < drawCount; ++i) {
                const ExplosionSegment& seg = fireSegments[i];
                const bool isEnd = (seg.baseName == "explosion_end");
                const float shadowSize = isEnd ? 0.30f : 0.35f;
                const glm::vec3 feet = ndcToWorld3D(gameMap, seg.pos, 0.02f);
                drawMesh3D(sphereOrCubeVAO,
                           sphereOrCubeIndexCount,
                           glm::vec3(feet.x, 0.03f, feet.z),
                           glm::vec3(shadowSize, 0.02f, shadowSize),
                           glm::vec3(0.06f, 0.04f, 0.04f));
            }
        }
    }

    // Explosion de glitter al recoger power-up en modo 3D.
    for (int r = 0; r < gameMap->getRows(); ++r) {
        for (int c = 0; c < gameMap->getCols(); ++c) {
            PowerUpType pickupType;
            float pickupFxT = 0.0f;
            if (!gameMap->getPowerUpPickupFx(r, c, pickupType, pickupFxT)) {
                continue;
            }
            if (pickupType != PowerUpType::BombUp && pickupType != PowerUpType::FireUp && pickupType != PowerUpType::SpeedUp) {
                continue;
            }

            const float easeOut = 1.0f - (1.0f - pickupFxT) * (1.0f - pickupFxT);
            const float intensity = 1.0f - pickupFxT;
            const float sparkleTwist = pickupFxT * 7.2f;
            const float corePulse = 1.0f + 0.22f * std::sin(pickupFxT * 26.0f);
            const float coreRadius = (0.16f + 0.44f * easeOut) * corePulse;
            const glm::vec3 center = gridToWorld3D(gameMap, r, c, 0.14f + 0.24f * easeOut);

            const glm::vec3 coreColor(0.75f + 0.95f * intensity,
                                      0.75f + 0.95f * intensity,
                                      0.75f + 0.95f * intensity);

            drawMesh3D(sphereOrCubeVAO,
                       sphereOrCubeIndexCount,
                       center,
                       glm::vec3(coreRadius, coreRadius, coreRadius),
                       coreColor);

            // Aro de onda expansiva que pierde intensidad segun avanza el efecto.
            const float ringRadius = 0.24f + 1.20f * easeOut;
            const float ringThickness = std::max(0.006f, 0.05f * intensity);
            const glm::vec3 ringColor(0.55f + 1.05f * intensity,
                                      0.55f + 1.05f * intensity,
                                      0.55f + 1.05f * intensity);
            drawMesh3D(sphereOrCubeVAO,
                       sphereOrCubeIndexCount,
                       center + glm::vec3(0.0f, 0.015f, 0.0f),
                       glm::vec3(ringRadius, ringThickness, ringRadius),
                       ringColor);

            // Particulas glitter: chispas dorado/blanco con dispersión radial y desvanecido.
            const int sparkleCount = 12;
            const float kTwoPi = 6.28318530718f;
            for (int i = 0; i < sparkleCount; ++i) {
                const float seed = ((float)(r + 1) * 12.9898f)
                                 + ((float)(c + 1) * 78.233f)
                                 + ((float)(i + 1) * 37.719f)
                                 + ((float)((int)pickupType + 1) * 19.171f);
                const float hashBase = std::sin(seed) * 43758.5453f;
                const float hash01 = hashBase - std::floor(hashBase);
                const float angle = hash01 * kTwoPi + sparkleTwist + ((float)i * 0.18f);

                const float radial = (0.14f + 0.30f * hash01) * (0.22f + 1.35f * easeOut);
                const float lift = 0.05f + 0.30f * easeOut + 0.05f * std::sin((pickupFxT * 13.0f) + ((float)i * 0.7f));
                const float twinkle = 0.62f + 0.38f * std::sin((pickupFxT * 38.0f) + ((float)i * 2.9f));
                const float sparkleIntensity = std::max(0.0f, twinkle * intensity);
                const float sparkleSize = (0.024f + 0.028f * hash01) * (0.24f + 0.90f * intensity);

                const glm::vec3 sparklePos = center + glm::vec3(std::cos(angle) * radial,
                                                                lift,
                                                                std::sin(angle) * radial);
                const glm::vec3 sparkleColor(0.70f + 1.00f * sparkleIntensity,
                                             0.62f + 0.95f * sparkleIntensity,
                                             0.48f + 0.82f * sparkleIntensity);

                drawMesh3D(sphereOrCubeVAO,
                           sphereOrCubeIndexCount,
                           sparklePos,
                           glm::vec3(sparkleSize, sparkleSize, sparkleSize),
                           sparkleColor);
            }
        }
    }

    const bool canRenderFlameUpGlb = canRenderFlamePowerUpGlb || canRenderFlameGlb;
    const bool canRenderSpeedUpGlb = canRenderSpeedPowerUpGlb;
    const bool canRenderKingBomber3D = canRenderKingBomberGlb;
    const bool canRenderDrones3D = canRenderAnyDronGlb;

    const bool canRenderPlayerGlb =
        (actorGlbVAO != 0 && actorGlbIndexCount > 0 && actorGlbTexture != 0 && shader3DTextured != 0);
    const bool canRenderLeonGlb =
        (leonGlbVAO != 0 && leonGlbIndexCount > 0 && leonGlbTexture != 0 && shader3DTextured != 0);
    const bool canRenderFantasmaGlb =
        (fantasmaGlbVAO != 0 && fantasmaGlbIndexCount > 0 && fantasmaGlbTexture != 0 && shader3DTextured != 0);
    const bool canRenderBebeGlb =
        (bebeGlbVAO != 0 && bebeGlbIndexCount > 0 && bebeGlbTexture != 0 && shader3DTextured != 0);
    const bool canRenderBabosaGlb =
        (babosaGlbVAO != 0 && babosaGlbIndexCount > 0 && babosaGlbTexture != 0 && shader3DTextured != 0);
    const bool canRenderSolGlb =
        (solGlbVAO != 0 && solGlbIndexCount > 0 && solGlbTexture != 0 && shader3DTextured != 0);
    const bool canRenderDragonGlb =
        (dragonGlbVAO != 0 && dragonGlbIndexCount > 0 && dragonGlbTexture != 0 && shader3DTextured != 0);

    if (canRenderPlayerGlb || canRenderLeonGlb || canRenderFantasmaGlb || canRenderBebeGlb || canRenderBabosaGlb || canRenderSolGlb || canRenderDragonGlb || canRenderKingBomber3D || canRenderDrones3D || canRenderBombGlb || canRenderFlameGlb || canRenderFlameUpGlb || canRenderSpeedUpGlb) {
        const GLboolean wasBlendEnabled = glIsEnabled(GL_BLEND);
        if (wasBlendEnabled) {
            glDisable(GL_BLEND);
        }

        glUseProgram(shader3DTextured);
        glUniformMatrix4fv(uniform3DTexturedView, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(uniform3DTexturedProjection, 1, GL_FALSE, glm::value_ptr(projection));
        glUniform1i(uniform3DTexturedSampler, 0);
        glUniform3fv(uniform3DTexturedLightPos, 1, glm::value_ptr(keyLightPos));
        glUniform3fv(uniform3DTexturedViewPos, 1, glm::value_ptr(cameraPos));
        glUniform3fv(uniform3DTexturedLightColor, 1, glm::value_ptr(keyLightColor));
        glUniform1f(uniform3DTexturedAmbientStrength, 0.30f);
        glUniform1f(uniform3DTexturedSpecularStrength, 0.24f);
        glUniform1f(uniform3DTexturedShininess, 28.0f);

        if (canRenderBombGlb) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, bombGlbTexture);

            for (auto* b : gBombs) {
                if (!b || b->state != BombState::FUSE) {
                    continue;
                }

                const float animT = b->remoteControlled
                    ? ((float)b->animStep * 0.55f)
                    : (b->fuseTimer * 2.8f);
                const float bob = 0.018f * std::sin(animT * 4.0f);
                const float pulse = 1.00f + 0.05f * std::sin(animT * 7.0f);

                glm::mat4 model(1.0f);
                model = glm::translate(model, ndcToWorld3D(gameMap, b->position, 0.08f) + glm::vec3(0.0f, bob, 0.0f));
                model = glm::rotate(model, animT * 0.65f, glm::vec3(0.0f, 1.0f, 0.0f));
                model = glm::scale(model, glm::vec3(0.92f * pulse, 0.92f * pulse, 0.92f * pulse));

                glUniformMatrix4fv(uniform3DTexturedModel, 1, GL_FALSE, glm::value_ptr(model));
                glBindVertexArray(bombGlbVAO);
                glDrawElements(GL_TRIANGLES, bombGlbIndexCount, GL_UNSIGNED_INT, 0);
            }

            // Power-up Bomb Up en 3D: usa el mismo modelo GLB de bomba y rota sobre su propio eje.
            const float now = (float)glfwGetTime();
            const float kBombUpSpinSpeed = 1.85f;
            const float kBombUpScale = 0.74f;

            for (int r = 0; r < gameMap->getRows(); ++r) {
                for (int c = 0; c < gameMap->getCols(); ++c) {
                    PowerUpType puType;
                    if (!gameMap->getVisiblePowerUpType(r, c, puType) || puType != PowerUpType::BombUp) {
                        continue;
                    }

                    glm::mat4 model(1.0f);
                    model = glm::translate(model, gridToWorld3D(gameMap, r, c, 0.11f));
                    model = glm::rotate(model, now * kBombUpSpinSpeed, glm::vec3(0.0f, 1.0f, 0.0f));
                    model = glm::scale(model, glm::vec3(kBombUpScale, kBombUpScale, kBombUpScale));

                    glUniformMatrix4fv(uniform3DTexturedModel, 1, GL_FALSE, glm::value_ptr(model));
                    glBindVertexArray(bombGlbVAO);
                    glDrawElements(GL_TRIANGLES, bombGlbIndexCount, GL_UNSIGNED_INT, 0);
                }
            }
        }

        if (canRenderFlameUpGlb) {
            const bool useDedicatedFlamePowerUpMesh = canRenderFlamePowerUpGlb;
            const GLuint flameUpVao = useDedicatedFlamePowerUpMesh ? flamePowerUpGlbVAO : flameGlbVAO;
            const GLsizei flameUpIndexCount = useDedicatedFlamePowerUpMesh ? flamePowerUpGlbIndexCount : flameGlbIndexCount;
            const GLuint flameUpTexture = useDedicatedFlamePowerUpMesh ? flamePowerUpGlbTexture : flameGlbTexture;

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, flameUpTexture);

            const float now = (float)glfwGetTime();
            const float kFlameUpSpinSpeed = 2.05f;
            const float kFlameUpScale = useDedicatedFlamePowerUpMesh ? 0.66f : 0.74f;

            for (int r = 0; r < gameMap->getRows(); ++r) {
                for (int c = 0; c < gameMap->getCols(); ++c) {
                    PowerUpType puType;
                    if (!gameMap->getVisiblePowerUpType(r, c, puType) || puType != PowerUpType::FireUp) {
                        continue;
                    }

                    const float bob = 0.02f * std::sin((now * 3.2f) + ((float)r * 0.45f) + ((float)c * 0.40f));

                    glm::mat4 model(1.0f);
                    model = glm::translate(model, gridToWorld3D(gameMap, r, c, 0.12f) + glm::vec3(0.0f, bob, 0.0f));
                    model = glm::rotate(model, now * kFlameUpSpinSpeed, glm::vec3(0.0f, 1.0f, 0.0f));
                    model = glm::scale(model, glm::vec3(kFlameUpScale, kFlameUpScale, kFlameUpScale));

                    glUniformMatrix4fv(uniform3DTexturedModel, 1, GL_FALSE, glm::value_ptr(model));
                    glBindVertexArray(flameUpVao);
                    glDrawElements(GL_TRIANGLES, flameUpIndexCount, GL_UNSIGNED_INT, 0);
                }
            }
        }

        if (canRenderSpeedUpGlb) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, speedPowerUpGlbTexture);

            const float now = (float)glfwGetTime();
            const float kSpeedUpSpinSpeed = 2.35f;
            const float kSpeedUpScale = 0.76f;

            for (int r = 0; r < gameMap->getRows(); ++r) {
                for (int c = 0; c < gameMap->getCols(); ++c) {
                    PowerUpType puType;
                    if (!gameMap->getVisiblePowerUpType(r, c, puType) || puType != PowerUpType::SpeedUp) {
                        continue;
                    }

                    const float bob = 0.022f * std::sin((now * 3.6f) + ((float)r * 0.42f) + ((float)c * 0.33f));

                    glm::mat4 model(1.0f);
                    model = glm::translate(model, gridToWorld3D(gameMap, r, c, 0.11f) + glm::vec3(0.0f, bob, 0.0f));
                    model = glm::rotate(model, now * kSpeedUpSpinSpeed, glm::vec3(0.0f, 1.0f, 0.0f));
                    model = glm::scale(model, glm::vec3(kSpeedUpScale, kSpeedUpScale, kSpeedUpScale));

                    glUniformMatrix4fv(uniform3DTexturedModel, 1, GL_FALSE, glm::value_ptr(model));
                    glBindVertexArray(speedPowerUpGlbVAO);
                    glDrawElements(GL_TRIANGLES, speedPowerUpGlbIndexCount, GL_UNSIGNED_INT, 0);
                }
            }
        }

        if (canRenderFlameGlb) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, flameGlbTexture);

            const glm::vec3 flameLightColor(1.0f, 0.83f, 0.58f);
            glUniform3fv(uniform3DTexturedLightColor, 1, glm::value_ptr(flameLightColor));
            glUniform1f(uniform3DTexturedAmbientStrength, 0.56f);
            glUniform1f(uniform3DTexturedSpecularStrength, 0.10f);
            glUniform1f(uniform3DTexturedShininess, 12.0f);

            const float now = (float)glfwGetTime();
            const float kPiHalf = 1.57079632679f;

            for (auto* b : gBombs) {
                if (!b || b->state != BombState::EXPLODING) {
                    continue;
                }

                for (std::size_t i = 0; i < b->explosionSegments.size(); ++i) {
                    const ExplosionSegment& seg = b->explosionSegments[i];
                    const bool isCenter = (seg.baseName == "explosion");
                    const bool isEnd = (seg.baseName == "explosion_end");

                    const float baseScale = isCenter ? 1.02f : (isEnd ? 0.72f : 0.84f);
                    const float animT = now * 14.0f + ((float)b->animFrame * 1.75f) + ((float)i * 0.95f);
                    const float flicker = 1.0f + 0.12f * std::sin(animT);
                    const float bob = 0.06f + 0.02f * std::sin(animT * 0.85f);

                    glm::mat4 model(1.0f);
                    model = glm::translate(model, ndcToWorld3D(gameMap, seg.pos, 0.08f) + glm::vec3(0.0f, bob, 0.0f));
                    model = glm::rotate(model, seg.rotation + kPiHalf, glm::vec3(0.0f, 1.0f, 0.0f));
                    model = glm::scale(model, glm::vec3(baseScale * flicker, baseScale * flicker, baseScale * flicker));

                    glUniformMatrix4fv(uniform3DTexturedModel, 1, GL_FALSE, glm::value_ptr(model));
                    glBindVertexArray(flameGlbVAO);
                    glDrawElements(GL_TRIANGLES, flameGlbIndexCount, GL_UNSIGNED_INT, 0);
                }
            }

            for (auto* enemy : gEnemies) {
                auto* dragon = dynamic_cast<DragonJoven*>(enemy);
                if (!dragon || dragon->lifeState != EnemyLifeState::Alive || !dragon->isFiringAttack()) {
                    continue;
                }

                const int activeSegments = dragon->getActiveFireSegmentCount();
                if (activeSegments <= 0) {
                    continue;
                }

                const auto& fireSegments = dragon->getFireSegments();
                const std::size_t drawCount = std::min<std::size_t>((std::size_t)activeSegments, fireSegments.size());
                for (std::size_t i = 0; i < drawCount; ++i) {
                    const ExplosionSegment& seg = fireSegments[i];
                    const bool isEnd = (seg.baseName == "explosion_end");

                    const float baseScale = isEnd ? 0.72f : 0.84f;
                    const float animT = now * 14.0f + ((float)i * 0.95f)
                        + (dragon->position.x * 0.40f) + (dragon->position.y * 0.45f);
                    const float flicker = 1.0f + 0.12f * std::sin(animT);
                    const float bob = 0.06f + 0.02f * std::sin(animT * 0.85f);

                    glm::mat4 model(1.0f);
                    model = glm::translate(model, ndcToWorld3D(gameMap, seg.pos, 0.08f) + glm::vec3(0.0f, bob, 0.0f));
                    model = glm::rotate(model, seg.rotation + kPiHalf, glm::vec3(0.0f, 1.0f, 0.0f));
                    model = glm::scale(model, glm::vec3(baseScale * flicker, baseScale * flicker, baseScale * flicker));

                    glUniformMatrix4fv(uniform3DTexturedModel, 1, GL_FALSE, glm::value_ptr(model));
                    glBindVertexArray(flameGlbVAO);
                    glDrawElements(GL_TRIANGLES, flameGlbIndexCount, GL_UNSIGNED_INT, 0);
                }
            }

            glUniform3fv(uniform3DTexturedLightColor, 1, glm::value_ptr(keyLightColor));
            glUniform1f(uniform3DTexturedAmbientStrength, 0.30f);
            glUniform1f(uniform3DTexturedSpecularStrength, 0.24f);
            glUniform1f(uniform3DTexturedShininess, 28.0f);
        }

        if (canRenderPlayerGlb) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, actorGlbTexture);

            // Ajuste de orientacion base del modelo GLB.
            const float kPlayerModelYawOffset = 1.57079632679f;

            for (std::size_t i = 0; i < gPlayers.size(); ++i) {
                Player* p = gPlayers[i];
                if (!p || !p->isAlive()) continue;

                if (camera3DType == Camera3DType::FirstPerson && i == 0) {
                    continue;
                }

                GLint modelFacingDirKey = p->facingDirKey;
                if (this->camera3DType == Camera3DType::PerspectiveFixed ||
                    this->camera3DType == Camera3DType::PerspectiveMobile) {
                    modelFacingDirKey = remapDirectionFor3DCamera(this, modelFacingDirKey);
                }

                const glm::vec3 feet = ndcToWorld3D(gameMap, p->position, 0.08f);
                const float yaw = facingKeyToYawRadians(modelFacingDirKey) + kPlayerModelYawOffset;

                glm::mat4 model(1.0f);
                model = glm::translate(model, feet + glm::vec3(0.0f, 0.01f, 0.0f));
                model = glm::rotate(model, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
                model = glm::scale(model, glm::vec3(1.28f, 1.28f, 1.28f));

                glUniformMatrix4fv(uniform3DTexturedModel, 1, GL_FALSE, glm::value_ptr(model));
                glBindVertexArray(actorGlbVAO);
                glDrawElements(GL_TRIANGLES, actorGlbIndexCount, GL_UNSIGNED_INT, 0);
            }
        }

        if (canRenderLeonGlb) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, leonGlbTexture);

            // Ajuste de orientacion base del modelo GLB de Leon.
            const float kLeonModelYawOffset = 1.57079632679f;

            for (auto* enemy : gEnemies) {
                if (!enemy || enemy->lifeState != EnemyLifeState::Alive) continue;

                const bool isLeonEnemy =
                    (enemy->spriteBaseId == "leon") ||
                    (enemy->currentSpriteName.size() >= 5 && enemy->currentSpriteName.compare(0, 5, "leon.") == 0);
                if (!isLeonEnemy) continue;

                const glm::vec3 feet = ndcToWorld3D(gameMap, enemy->position, 0.08f);
                const float yaw = enemyDirectionToYawRadians(enemy->facing) + kLeonModelYawOffset;

                glm::mat4 model(1.0f);
                model = glm::translate(model, feet + glm::vec3(0.0f, 0.01f, 0.0f));
                model = glm::rotate(model, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
                model = glm::scale(model, glm::vec3(1.05f, 1.05f, 1.05f));

                glUniformMatrix4fv(uniform3DTexturedModel, 1, GL_FALSE, glm::value_ptr(model));
                glBindVertexArray(leonGlbVAO);
                glDrawElements(GL_TRIANGLES, leonGlbIndexCount, GL_UNSIGNED_INT, 0);
            }
        }

        if (canRenderFantasmaGlb) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, fantasmaGlbTexture);

            // Ajuste de orientacion base del modelo GLB de Fantasma Mortal.
            const float kFantasmaModelYawOffset = 1.57079632679f;

            for (auto* enemy : gEnemies) {
                if (!enemy || enemy->lifeState != EnemyLifeState::Alive) continue;

                const bool isFantasmaEnemy =
                    (enemy->spriteBaseId == "fantasma") ||
                    (enemy->currentSpriteName.size() >= 9 && enemy->currentSpriteName.compare(0, 9, "fantasma.") == 0);
                if (!isFantasmaEnemy) continue;

                const glm::vec3 feet = ndcToWorld3D(gameMap, enemy->position, 0.08f);
                const float yaw = enemyDirectionToYawRadians(enemy->facing) + kFantasmaModelYawOffset;

                glm::mat4 model(1.0f);
                model = glm::translate(model, feet + glm::vec3(0.0f, 0.01f, 0.0f));
                model = glm::rotate(model, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
                model = glm::scale(model, glm::vec3(1.08f, 1.08f, 1.08f));

                glUniformMatrix4fv(uniform3DTexturedModel, 1, GL_FALSE, glm::value_ptr(model));
                glBindVertexArray(fantasmaGlbVAO);
                glDrawElements(GL_TRIANGLES, fantasmaGlbIndexCount, GL_UNSIGNED_INT, 0);
            }
        }

        if (canRenderBebeGlb) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, bebeGlbTexture);

            // Ajuste de orientacion base del modelo GLB de Bebe Lloron.
            const float kBebeModelYawOffset = 1.57079632679f;

            for (auto* enemy : gEnemies) {
                if (!enemy || enemy->lifeState != EnemyLifeState::Alive) continue;

                const bool isBebeEnemy =
                    (enemy->spriteBaseId == "bebe") ||
                    (enemy->currentSpriteName.size() >= 5 && enemy->currentSpriteName.compare(0, 5, "bebe.") == 0);
                if (!isBebeEnemy) continue;

                const glm::vec3 feet = ndcToWorld3D(gameMap, enemy->position, 0.08f);
                const float yaw = enemyDirectionToYawRadians(enemy->facing) + kBebeModelYawOffset;

                glm::mat4 model(1.0f);
                model = glm::translate(model, feet + glm::vec3(0.0f, 0.01f, 0.0f));
                model = glm::rotate(model, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
                model = glm::scale(model, glm::vec3(0.98f, 0.98f, 0.98f));

                glUniformMatrix4fv(uniform3DTexturedModel, 1, GL_FALSE, glm::value_ptr(model));
                glBindVertexArray(bebeGlbVAO);
                glDrawElements(GL_TRIANGLES, bebeGlbIndexCount, GL_UNSIGNED_INT, 0);
            }
        }

        if (canRenderBabosaGlb) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, babosaGlbTexture);

            // Ajuste de orientacion base del modelo GLB de Babosa.
            const float kBabosaModelYawOffset = 1.57079632679f;

            for (auto* enemy : gEnemies) {
                if (!enemy || enemy->lifeState != EnemyLifeState::Alive) continue;

                const bool isBabosaEnemy =
                    (enemy->spriteBaseId == "babosa") ||
                    (enemy->currentSpriteName.size() >= 7 && enemy->currentSpriteName.compare(0, 7, "babosa.") == 0);
                if (!isBabosaEnemy) continue;

                const glm::vec3 feet = ndcToWorld3D(gameMap, enemy->position, 0.08f);
                const float yaw = enemyDirectionToYawRadians(enemy->facing) + kBabosaModelYawOffset;

                glm::mat4 model(1.0f);
                model = glm::translate(model, feet + glm::vec3(0.0f, 0.01f, 0.0f));
                model = glm::rotate(model, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
                model = glm::scale(model, glm::vec3(1.02f, 1.02f, 1.02f));

                glUniformMatrix4fv(uniform3DTexturedModel, 1, GL_FALSE, glm::value_ptr(model));
                glBindVertexArray(babosaGlbVAO);
                glDrawElements(GL_TRIANGLES, babosaGlbIndexCount, GL_UNSIGNED_INT, 0);
            }
        }

        if (canRenderSolGlb) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, solGlbTexture);

            const float now = (float)glfwGetTime();

            for (auto* enemy : gEnemies) {
                if (!enemy || enemy->lifeState != EnemyLifeState::Alive) continue;

                const bool isSolEnemy =
                    (enemy->spriteBaseId == "sol") ||
                    (enemy->currentSpriteName.size() >= 4 && enemy->currentSpriteName.compare(0, 4, "sol.") == 0);
                if (!isSolEnemy) continue;

                float modelScale = 1.12f;
                float hoverHeight = 1.62f;
                if (enemy->currentSpriteName.size() >= 12 && enemy->currentSpriteName.compare(0, 12, "sol.mediano.") == 0) {
                    modelScale = 0.90f;
                    hoverHeight = 1.48f;
                } else if (enemy->currentSpriteName.size() >= 9 && enemy->currentSpriteName.compare(0, 9, "sol.peque") == 0) {
                    modelScale = 0.72f;
                    hoverHeight = 1.34f;
                }

                modelScale *= 2.0f;

                const float phase = now * 2.4f + (enemy->position.x * 3.7f) + (enemy->position.y * 4.1f);
                const float bob = 0.08f * std::sin(phase);
                const float spin = now * 0.95f + phase * 0.18f;

                glm::mat4 model(1.0f);
                model = glm::translate(model, ndcToWorld3D(gameMap, enemy->position, hoverHeight + bob));
                model = glm::rotate(model, spin, glm::vec3(0.0f, 1.0f, 0.0f));
                model = glm::scale(model, glm::vec3(modelScale, modelScale, modelScale));

                glUniformMatrix4fv(uniform3DTexturedModel, 1, GL_FALSE, glm::value_ptr(model));
                glBindVertexArray(solGlbVAO);
                glDrawElements(GL_TRIANGLES, solGlbIndexCount, GL_UNSIGNED_INT, 0);
            }
        }

        if (canRenderDragonGlb) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, dragonGlbTexture);

            // Ajuste de orientacion base del modelo GLB de Dragon Joven.
            const float kDragonModelYawOffset = 1.57079632679f;

            for (auto* enemy : gEnemies) {
                if (!enemy || enemy->lifeState != EnemyLifeState::Alive) continue;

                const bool isDragonEnemy =
                    (enemy->spriteBaseId == "dragon") ||
                    (enemy->currentSpriteName.size() >= 7 && enemy->currentSpriteName.compare(0, 7, "dragon.") == 0);
                if (!isDragonEnemy) continue;

                const glm::vec3 feet = ndcToWorld3D(gameMap, enemy->position, 0.08f);
                const float yaw = enemyDirectionToYawRadians(enemy->facing) + kDragonModelYawOffset;

                glm::mat4 model(1.0f);
                model = glm::translate(model, feet + glm::vec3(0.0f, 0.04f, 0.0f));
                model = glm::rotate(model, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
                model = glm::scale(model, glm::vec3(1.12f, 1.12f, 1.12f));

                glUniformMatrix4fv(uniform3DTexturedModel, 1, GL_FALSE, glm::value_ptr(model));
                glBindVertexArray(dragonGlbVAO);
                glDrawElements(GL_TRIANGLES, dragonGlbIndexCount, GL_UNSIGNED_INT, 0);
            }
        }

        if (canRenderKingBomber3D) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, kingBomberGlbTexture);

            const float kKingModelYawOffset = 1.57079632679f;

            for (auto* enemy : gEnemies) {
                auto* king = dynamic_cast<KingBomber*>(enemy);
                if (!king || king->lifeState != EnemyLifeState::Alive) continue;
                if (hiddenByKingBlink(king)) continue;

                const glm::vec3 feet = ndcToWorld3D(gameMap, king->position, 0.08f);
                const float yaw = enemyDirectionToYawRadians(king->facing) + kKingModelYawOffset;

                glm::mat4 model(1.0f);
                model = glm::translate(model, feet + glm::vec3(0.0f, 0.03f, 0.0f));
                model = glm::rotate(model, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
                model = glm::scale(model, glm::vec3(1.54f, 1.54f, 1.54f));

                glUniformMatrix4fv(uniform3DTexturedModel, 1, GL_FALSE, glm::value_ptr(model));
                glBindVertexArray(kingBomberGlbVAO);
                glDrawElements(GL_TRIANGLES, kingBomberGlbIndexCount, GL_UNSIGNED_INT, 0);
            }
        }

        if (canRenderDrones3D) {
            const float kDronModelYawOffset = 1.57079632679f;

            for (auto* enemy : gEnemies) {
                auto* dron = dynamic_cast<DronBombardero*>(enemy);
                if (!dron || dron->lifeState != EnemyLifeState::Alive) continue;
                if (hiddenByKingBlink(dron)) continue;

                GLuint dronVao = 0;
                GLsizei dronIndexCount = 0;
                GLuint dronTexture = 0;
                if (!resolveDroneRenderResource(dron, dronVao, dronIndexCount, dronTexture)) {
                    continue;
                }

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, dronTexture);

                const glm::vec3 feet = ndcToWorld3D(gameMap, dron->position, 0.08f);
                const float yaw = enemyDirectionToYawRadians(dron->facing) + kDronModelYawOffset;

                glm::mat4 model(1.0f);
                model = glm::translate(model, feet + glm::vec3(0.0f, 0.015f, 0.0f));
                model = glm::rotate(model, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
                model = glm::scale(model, glm::vec3(0.66f, 0.66f, 0.66f));

                glUniformMatrix4fv(uniform3DTexturedModel, 1, GL_FALSE, glm::value_ptr(model));
                glBindVertexArray(dronVao);
                glDrawElements(GL_TRIANGLES, dronIndexCount, GL_UNSIGNED_INT, 0);
            }
        }

        if (wasBlendEnabled) {
            glEnable(GL_BLEND);
        }
    }

    // Enemigos siempre como billboards 2D en 3D.
    glUseProgram(shader);
    const glm::mat4 spriteProjection3D = projection * view;
    glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, glm::value_ptr(spriteProjection3D));
    glUniform1i(uniformTexture, 0);

    if (!canRenderPlayerGlb) {
        for (std::size_t i = 0; i < gPlayers.size(); ++i) {
            Player* p = gPlayers[i];
            if (!p || !p->isAlive()) continue;

            if (camera3DType == Camera3DType::FirstPerson && i == 0) {
                continue;
            }

            glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
            bool hasUv = getUvRectForSprite(gPlayerAtlas, p->currentSpriteName, uvRect);
            if (!hasUv) {
                const std::string fallback = p->spritePrefix + ".abajo.0";
                hasUv = getUvRectForSprite(gPlayerAtlas, fallback, uvRect);
            }

            if (hasUv) {
                const glm::vec3 feet = ndcToWorld3D(gameMap, p->position, 0.02f);
                drawSpriteBillboard3D(texture, uvRect, feet, 0.92f, 1.38f, p->flipX, glm::vec4(1.0f));
            }
        }
    }

    for (auto* enemy : gEnemies) {
        if (!enemy || enemy->lifeState != EnemyLifeState::Alive) continue;

        if (hiddenByKingBlink(enemy)) {
            continue;
        }

        const bool isLeonEnemy =
            (enemy->spriteBaseId == "leon") ||
            (enemy->currentSpriteName.size() >= 5 && enemy->currentSpriteName.compare(0, 5, "leon.") == 0);
        const bool isFantasmaEnemy =
            (enemy->spriteBaseId == "fantasma") ||
            (enemy->currentSpriteName.size() >= 9 && enemy->currentSpriteName.compare(0, 9, "fantasma.") == 0);
        const bool isBebeEnemy =
            (enemy->spriteBaseId == "bebe") ||
            (enemy->currentSpriteName.size() >= 5 && enemy->currentSpriteName.compare(0, 5, "bebe.") == 0);
        const bool isBabosaEnemy =
            (enemy->spriteBaseId == "babosa") ||
            (enemy->currentSpriteName.size() >= 7 && enemy->currentSpriteName.compare(0, 7, "babosa.") == 0);
        const bool isSolEnemy =
            (enemy->spriteBaseId == "sol") ||
            (enemy->currentSpriteName.size() >= 4 && enemy->currentSpriteName.compare(0, 4, "sol.") == 0);
        const bool isDragonEnemy =
            (enemy->spriteBaseId == "dragon") ||
            (enemy->currentSpriteName.size() >= 7 && enemy->currentSpriteName.compare(0, 7, "dragon.") == 0);
        const bool isKingBomberEnemy = (dynamic_cast<KingBomber*>(enemy) != nullptr);
        const bool isDronBombarderoEnemy = (dynamic_cast<DronBombardero*>(enemy) != nullptr);
        const bool enemyUsesFireSprite = usesFireSprite(enemy);
        bool hasResolvedDronModel = false;
        if (canRenderDrones3D && isDronBombarderoEnemy) {
            GLuint dronVao = 0;
            GLsizei dronIndexCount = 0;
            GLuint dronTexture = 0;
            hasResolvedDronModel = resolveDroneRenderResource(enemy, dronVao, dronIndexCount, dronTexture);
        }

        if ((canRenderKingBomber3D && isKingBomberEnemy && enemyUsesFireSprite) ||
            (hasResolvedDronModel && enemyUsesFireSprite)) {
            glm::vec4 fireUvRect(0.0f, 0.0f, 1.0f, 1.0f);
            if (getUvRectForSprite(gEnemyAtlas, enemy->currentSpriteName, fireUvRect)) {
                glm::vec3 fireFeet = ndcToWorld3D(gameMap, enemy->position, 0.02f);
                float fireWidth = 0.56f;
                float fireHeight = 0.74f;
                if (isKingBomberEnemy) {
                    fireFeet += glm::vec3(0.0f, 0.60f, 0.0f);
                    fireWidth = 0.92f;
                    fireHeight = 1.08f;
                } else {
                    fireFeet += glm::vec3(0.0f, 0.36f, 0.0f);
                }
                drawSpriteBillboard3D(enemyTexture, fireUvRect, fireFeet, fireWidth, fireHeight, enemy->flipX, glm::vec4(1.0f));
            }
        }

        if (canRenderLeonGlb && isLeonEnemy) {
            continue;
        }
        if (canRenderFantasmaGlb && isFantasmaEnemy) {
            continue;
        }
        if (canRenderBebeGlb && isBebeEnemy) {
            continue;
        }
        if (canRenderBabosaGlb && isBabosaEnemy) {
            continue;
        }
        if (canRenderSolGlb && isSolEnemy) {
            continue;
        }
        if (canRenderDragonGlb && isDragonEnemy) {
            continue;
        }
        if (canRenderKingBomber3D && isKingBomberEnemy) {
            continue;
        }
        if (hasResolvedDronModel) {
            continue;
        }

        glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
        bool hasUv = getUvRectForSprite(gEnemyAtlas, enemy->currentSpriteName, uvRect);
        if (!hasUv && !enemy->spriteBaseId.empty()) {
            hasUv = getUvRectForSprite(gEnemyAtlas, enemy->spriteBaseId + ".derecha.0", uvRect);
        }
        if (!hasUv && !enemy->spriteBaseId.empty()) {
            hasUv = getUvRectForSprite(gEnemyAtlas, enemy->spriteBaseId + ".abajo.0", uvRect);
        }

        if (hasUv) {
            const glm::vec3 feet = ndcToWorld3D(gameMap, enemy->position, 0.02f);
            drawSpriteBillboard3D(enemyTexture, uvRect, feet, 0.96f, 1.30f, enemy->flipX, glm::vec4(1.0f));
        }
    }

    glBindVertexArray(0);
    glUseProgram(0);

    if (camera3DType == Camera3DType::FirstPerson) {
        renderFirstPersonMiniMap2D(gameMap, WIDTH, HEIGHT);
    }
}

// Renderiza mapa, bombas, jugadores y enemigos en 2D.
void Game::render2D() {
    glDisable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(shader);

    // Calcular proyeccion ortografica para mantener el aspect ratio en NDC original
    const float safeHeight = (HEIGHT > 0) ? (float)HEIGHT : 1.0f;
    float aspect = (float)WIDTH / safeHeight;
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
    gameMap->renderHud(VAO, uniformModel, uniformUvRect, gScoreboardAtlas, scoreboardTexture, &playerScores, &gPlayers, &gEnemies, currentGameLevel, levelTimeRemaining,(mode == GameMode::OnePlayer || mode == GameMode::TwoPlayers) ? 0 : 1 );

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

void Game::render() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

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
        render3D();
    } else {
        render2D();
    }
}

void Game::onResize(int width, int height) {
    WIDTH = std::max(1, width);
    HEIGHT = std::max(1, height);

    if (gameMap) {
        float aspect = (float)WIDTH / (float)HEIGHT;
        gameMap->calculateTileMetrics(aspect);
    }
}

// Cambiar pantalla de fullscreen a windowed (y viceversa)
void Game::toggleFullscreen(GLFWwindow* window) {

    if (glfwGetWindowMonitor(window) == nullptr) {
        // Cambiar a fullscreen
        glfwGetWindowPos(window, &windowedXPos, &windowedYPos);

        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        onResize(mode->width, mode->height);
    } else {
        // Pasar a windowed
        // Valores fixeados para windowed
        WIDTH = 1280; 
        HEIGHT = 720;
        glfwSetWindowMonitor(window, nullptr, windowedXPos, windowedYPos, WIDTH, HEIGHT, 0);
        onResize(WIDTH, HEIGHT);
    }
}
