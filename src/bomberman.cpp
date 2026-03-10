#include "bomberman.hpp"
#include "player.hpp"
#include "sprite_atlas.hpp"
#include "game_map.hpp"

/*
 * bomberman.cpp
 * ------------
 * Implementación del bucle de juego (init/input/update/render) + render OpenGL simple.
 *
 * Funcionalidades principales:
 * - Inicializa OpenGL (VAO/VBO/EBO + shaders) y carga texturas.
 * - Carga el mapa (`GameMap`) y su atlas.
 * - Crea 1 o 2 jugadores según `GameMode`.
 * - Entrada: Jugador 1 con flechas, Jugador 2 con WASD.
 * - Render: mapa primero, jugadores encima (sprites desde SpriteAtlasPlayer).
 *
 * Nota:
 * - Este archivo es deliberadamente “monolítico” por ahora. Se organizan secciones
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

struct PlayerInstance {
    Player* player = nullptr;
    std::string spritePrefix; // p.ej. "jugadorblanco" o "jugadorrojo"

    std::string currentSpriteName;
    float flipX = 0.0f;

    // Estado de animación (caminar): 0,1,0,2,...
    GLint facingDirKey = GLFW_KEY_DOWN;
    bool isWalking = false;
    float walkTimer = 0.0f;
    int walkPhase = 0; // 0..3 -> {0,1,0,2}
};

static std::vector<PlayerInstance> gPlayers;
GameMap* gameMap;
GLuint mapTexture;

// ============================== OpenGL: estado global ==============================

// Global variables for OpenGL
GLuint VAO, VBO, EBO, shader, uniformModel, uniformProjection, uniformTexture, uniformTintColor, uniformUvRect, uniformFlipX;

GLuint texture;

// ============================== Shaders (vertex/fragment) ==============================
// Vertex Shader
static const char* vShaderSrc = R"(
#version 330
layout (location = 0) in vec3 pos;
layout (location = 1) in vec2 texCoord;
out vec2 TexCoord;
uniform mat4 model;
uniform mat4 projection;
uniform vec4 uvRect; // (u0, v0, u1, v1)
uniform float flipX; // 0.0 normal, 1.0 mirror horizontally
void main()
{
    gl_Position = projection * model * vec4(pos, 1.0);
    float tx = mix(texCoord.x, 1.0 - texCoord.x, flipX);
    TexCoord = vec2(
        mix(uvRect.x, uvRect.z, tx),
        mix(uvRect.y, uvRect.w, texCoord.y)
    );
}
)";

// Fragment Shader
static const char* fShaderSrc = R"(
#version 330
in vec2 TexCoord;
out vec4 color;
uniform sampler2D ourTexture;
uniform vec4 tintColor;
void main()
{
    vec4 texColor = texture(ourTexture, TexCoord);
    color = texColor * tintColor; // Apply tint color
})";

static SpriteAtlas gPlayerAtlas;
static constexpr float gWalkFrameInterval = 0.12f; // segundos por cambio de fase

static int walkPhaseToFrameIndex(int phase)
{
    switch (phase & 3) {
        case 0: return 0;
        case 1: return 1;
        case 2: return 0;
        case 3: return 2;
    }
    return 0;
}

// Construye el nombre del sprite a partir de dirección + frame y actualiza flipX.
static void setPlayerSpriteFromDirAndFrame(PlayerInstance& inst, GLint dirKey, int frameIndex)
{
    inst.flipX = 0.0f;

    if (frameIndex < 0) frameIndex = 0;
    if (frameIndex > 2) frameIndex = 0;

    switch (dirKey) {
        case GLFW_KEY_UP:
            inst.currentSpriteName = inst.spritePrefix + ".arriba." + std::to_string(frameIndex);
            break;
        case GLFW_KEY_DOWN:
            inst.currentSpriteName = inst.spritePrefix + ".abajo." + std::to_string(frameIndex);
            break;
        case GLFW_KEY_LEFT:
            // Reutilizamos los frames de la derecha espejando en X
            inst.currentSpriteName = inst.spritePrefix + ".derecha." + std::to_string(frameIndex);
            inst.flipX = 1.0f;
            break;
        case GLFW_KEY_RIGHT:
        default:
            inst.currentSpriteName = inst.spritePrefix + ".derecha." + std::to_string(frameIndex);
            break;
    }

    inst.facingDirKey = dirKey;
}

// ============================== OpenGL: helpers ==============================

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

void AddShader(GLuint program, const char* shaderCode, GLenum shaderType)
{
    GLuint shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, &shaderCode, nullptr);
    glCompileShader(shader);

    GLint result = 0;
    GLchar errorLog[1024] = { 0 };
    glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
    if (!result)
    {
        glGetShaderInfoLog(shader, sizeof(errorLog), nullptr, errorLog);
        printf("Error compiling the %d shader: '%s'\n", shaderType, errorLog);
        return;
    }

    glAttachShader(program, shader);
}

void CompileShaders()
{
    shader = glCreateProgram();
    if (!shader)
    {
        printf("Error creating shader program!\n");
        return;
    }

    AddShader(shader, vShaderSrc, GL_VERTEX_SHADER);
    AddShader(shader, fShaderSrc, GL_FRAGMENT_SHADER);

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
    uniformUvRect = glGetUniformLocation(shader, "uvRect");
    uniformFlipX = glGetUniformLocation(shader, "flipX");
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


// Copiada de Pengu, por si sirve
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

void Game::init() {

    CreateRectangle();
    CompileShaders();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Cargar atlas + textura del jugador (alpha) desde JSON
    const std::string atlasPath = resolveAssetPath("resources/sprites/atlases/SpriteAtlasPlayer.json");
    if (!loadSpriteAtlasMinimal(atlasPath, gPlayerAtlas))
    {
        std::cerr << "Error cargando atlas: " << atlasPath << std::endl;
        std::exit(EXIT_FAILURE);
    }

    // Sanity check: si no existen los sprites esperados, avisar (ayuda a detectar nombres distintos en el JSON)
    if (gPlayerAtlas.sprites.find("jugadorblanco.abajo.0") == gPlayerAtlas.sprites.end()) {
        std::cerr << "[SpriteAtlas] Aviso: no existe 'jugadorblanco.abajo.0' en el atlas."
                  << " Total sprites: " << gPlayerAtlas.sprites.size() << "\n";
        int shown = 0;
        for (const auto& p : gPlayerAtlas.sprites) {
            std::cerr << "  - " << p.first << "\n";
            if (++shown >= 10) break;
        }
    }

    const std::string texturePath = resolveAssetPath(gPlayerAtlas.imagePath);
    texture = LoadTexture(texturePath.c_str());
    if (texture == 0)
    {
        std::cerr << "Error cargando textura: " << texturePath << std::endl;
        std::exit(EXIT_FAILURE);
    }

    // Cargar mapa
    gameMap = new GameMap();
    if (!gameMap->loadFromFile("levels/level_01.txt"))
    {
        std::cerr << "Error cargando mapa" << std::endl;
        std::exit(EXIT_FAILURE);
    }

    // Cargar atlas del mapa (coordenadas de sprites)
    if (!gameMap->loadAtlas("resources/sprites/atlases/SpriteAtlasStage1.json"))
    {
        std::cerr << "Error cargando atlas del mapa" << std::endl;
        std::exit(EXIT_FAILURE);
    }

    // Calcular metricas del mapa (ahora que tenemos cols, rows y aspectRatio)
    float aspectRatio = (float)WIDTH / (float)HEIGHT;
    gameMap->calculateTileMetrics(aspectRatio);

    // Cargar textura de la sprite sheet del mapa
    const std::string mapTexPath = resolveAssetPath("resources/sprites/mapas/Stage1/sprites-Stage1.png");
    mapTexture = LoadTexture(mapTexPath.c_str());
    if (mapTexture == 0)
    {
        std::cerr << "Error cargando textura del mapa: " << mapTexPath << std::endl;
        std::exit(EXIT_FAILURE);
    }

    // Crear jugador(es) en la posicion de spawn del mapa
    gPlayers.clear();

    {
        PlayerInstance p1;
        p1.spritePrefix = "jugadorblanco";
        glm::vec2 spawnPos = gameMap->getSpawnPosition(0);
        p1.player = new Player(spawnPos, glm::vec2(0.2f, 0.2f), 0.0005f);
        p1.facingDirKey = GLFW_KEY_DOWN;
        p1.isWalking = false;
        p1.walkTimer = 0.0f;
        p1.walkPhase = 0;
        setPlayerSpriteFromDirAndFrame(p1, p1.facingDirKey, 0);
        gPlayers.push_back(p1);
    }

    if (this->mode == GameMode::TwoPlayers) {
        PlayerInstance p2;
        p2.spritePrefix = "jugadorrojo";
        glm::vec2 spawnPos = gameMap->getSpawnPosition(1);
        p2.player = new Player(spawnPos, glm::vec2(0.2f, 0.2f), 0.0005f);
        p2.facingDirKey = GLFW_KEY_DOWN;
        p2.isWalking = false;
        p2.walkTimer = 0.0f;
        p2.walkPhase = 0;
        setPlayerSpriteFromDirAndFrame(p2, p2.facingDirKey, 0);
        gPlayers.push_back(p2);
    }
}

// Procesa input y aplica movimiento/animación de jugadores.
void Game::processInput() {
    if (this->state != GAME_PLAYING) return;

    if (gPlayers.empty() || gPlayers[0].player == nullptr) return;
    PlayerInstance& p1 = gPlayers[0];

    // ======================= Jugador 1 (blanco): Flechas =======================

    const bool up = (this->keys[GLFW_KEY_UP] >= GLFW_PRESS);
    const bool down = (this->keys[GLFW_KEY_DOWN] >= GLFW_PRESS);
    const bool left = (this->keys[GLFW_KEY_LEFT] >= GLFW_PRESS);
    const bool right = (this->keys[GLFW_KEY_RIGHT] >= GLFW_PRESS);

    const int pressedCount = (up ? 1 : 0) + (down ? 1 : 0) + (left ? 1 : 0) + (right ? 1 : 0);
    if (pressedCount == 0) {
        // Al soltar todas las teclas, quedarse SIEMPRE en el frame .0 (idle)
        p1.isWalking = false;
        p1.walkTimer = 0.0f;
        p1.walkPhase = 0;

        // Mantener la última dirección a la que miraba
        if (this->lastDirKey != GLFW_KEY_UNKNOWN) {
            p1.facingDirKey = this->lastDirKey;
        }
        setPlayerSpriteFromDirAndFrame(p1, p1.facingDirKey, 0);
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
                p1.player->UpdateSprite(MOVE_UP, gameMap);
                if (!p1.isWalking || p1.facingDirKey != GLFW_KEY_UP) {
                    p1.walkTimer = 0.0f; p1.walkPhase = 0;
                }
                p1.facingDirKey = GLFW_KEY_UP;
                break;
            case GLFW_KEY_DOWN:
                p1.player->UpdateSprite(MOVE_DOWN, gameMap);
                if (!p1.isWalking || p1.facingDirKey != GLFW_KEY_DOWN) {
                    p1.walkTimer = 0.0f; p1.walkPhase = 0;
                }
                p1.facingDirKey = GLFW_KEY_DOWN;
                break;
            case GLFW_KEY_LEFT:
                p1.player->UpdateSprite(MOVE_LEFT, gameMap);
                if (!p1.isWalking || p1.facingDirKey != GLFW_KEY_LEFT) {
                    p1.walkTimer = 0.0f; p1.walkPhase = 0;
                }
                p1.facingDirKey = GLFW_KEY_LEFT;
                break;
            case GLFW_KEY_RIGHT:
                p1.player->UpdateSprite(MOVE_RIGHT, gameMap);
                if (!p1.isWalking || p1.facingDirKey != GLFW_KEY_RIGHT) {
                    p1.walkTimer = 0.0f; p1.walkPhase = 0;
                }
                p1.facingDirKey = GLFW_KEY_RIGHT;
                break;
        }

        // Si hay dirección pulsada, consideramos que está caminando
        p1.isWalking = true;
    }

    // ======================= Jugador 2 (rojo): WASD =======================
    if (this->mode == GameMode::TwoPlayers && gPlayers.size() >= 2 && gPlayers[1].player != nullptr) {
        PlayerInstance& p2 = gPlayers[1];

        const bool up2 = (this->keys[GLFW_KEY_W] >= GLFW_PRESS);
        const bool down2 = (this->keys[GLFW_KEY_S] >= GLFW_PRESS);
        const bool left2 = (this->keys[GLFW_KEY_A] >= GLFW_PRESS);
        const bool right2 = (this->keys[GLFW_KEY_D] >= GLFW_PRESS);

        const int pressedCount2 = (up2 ? 1 : 0) + (down2 ? 1 : 0) + (left2 ? 1 : 0) + (right2 ? 1 : 0);
        if (pressedCount2 == 0) {
            p2.isWalking = false;
            p2.walkTimer = 0.0f;
            p2.walkPhase = 0;

            // Mantener la última dirección WASD
            if (this->lastDirKeyP2 != GLFW_KEY_UNKNOWN) {
                switch (this->lastDirKeyP2) {
                    case GLFW_KEY_W: p2.facingDirKey = GLFW_KEY_UP; break;
                    case GLFW_KEY_S: p2.facingDirKey = GLFW_KEY_DOWN; break;
                    case GLFW_KEY_A: p2.facingDirKey = GLFW_KEY_LEFT; break;
                    case GLFW_KEY_D: p2.facingDirKey = GLFW_KEY_RIGHT; break;
                }
            }
            setPlayerSpriteFromDirAndFrame(p2, p2.facingDirKey, 0);
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

            // Mapear WASD a dirección estándar para sprites (UP/DOWN/LEFT/RIGHT)
            GLint dir2 = GLFW_KEY_DOWN;
            Move mov2 = MOVE_NONE;
            switch (keyToUse2) {
                case GLFW_KEY_W: dir2 = GLFW_KEY_UP; mov2 = MOVE_UP; break;
                case GLFW_KEY_S: dir2 = GLFW_KEY_DOWN; mov2 = MOVE_DOWN; break;
                case GLFW_KEY_A: dir2 = GLFW_KEY_LEFT; mov2 = MOVE_LEFT; break;
                case GLFW_KEY_D: dir2 = GLFW_KEY_RIGHT; mov2 = MOVE_RIGHT; break;
            }

            p2.player->UpdateSprite(mov2, gameMap);
            if (!p2.isWalking || p2.facingDirKey != dir2) {
                p2.walkTimer = 0.0f;
                p2.walkPhase = 0;
            }
            p2.facingDirKey = dir2;
            p2.isWalking = true;
        }
    }

}

// Actualiza timers/animación en función del deltaTime.
void Game::update() {
    static double lastTime = glfwGetTime();
    double currentTime = glfwGetTime();
    float deltaTime = static_cast<float>(currentTime - lastTime);
    lastTime = currentTime;

    if (gameMap) {
        gameMap->update(deltaTime);
    }

    // Animación de caminar por jugador: .0 .1 .0 .2 ...
    for (auto& inst : gPlayers) {
        if (!inst.player) continue;
        if (inst.isWalking) {
            inst.walkTimer += deltaTime;
            while (inst.walkTimer >= gWalkFrameInterval) {
                inst.walkTimer -= gWalkFrameInterval;
                inst.walkPhase = (inst.walkPhase + 1) & 3;
            }
            setPlayerSpriteFromDirAndFrame(inst, inst.facingDirKey, walkPhaseToFrameIndex(inst.walkPhase));
        } else {
            inst.walkTimer = 0.0f;
            inst.walkPhase = 0;
            setPlayerSpriteFromDirAndFrame(inst, inst.facingDirKey, 0);
        }
    }
}

// Renderiza mapa + jugadores (sprites) con un quad y UVs desde el atlas.
void Game::render() {

    glUseProgram(shader);

    // Calcular proyeccion ortografica para mantener el aspect ratio en NDC original
    float aspect = (float)WIDTH / (float)HEIGHT;
    glm::mat4 projection = glm::ortho(-aspect, aspect, -1.0f, 1.0f, -1.0f, 1.0f);
    glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, glm::value_ptr(projection));

    // Texture unit 0
    glUniform1i(uniformTexture, 0);

    // === 1. Renderizar mapa (fondo) ===
    gameMap->render(VAO, mapTexture, uniformModel, uniformUvRect, uniformTintColor, uniformFlipX);

    // === 2. Renderizar jugador (encima del mapa) ===
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);

    glBindVertexArray(VAO);

    // Escalar el sprite del jugador para que ocupe ~1 tile del mapa
    float halfTile = gameMap->getTileSize() / 2.0f;

    for (auto& inst : gPlayers) {
        if (!inst.player) continue;

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(inst.player->position, 0.0f));

        // UV del sprite actual (si falla, se pinta la textura completa)
        glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
        bool okUv = getUvRectForSprite(gPlayerAtlas, inst.currentSpriteName, uvRect);
        if (!okUv) {
            static bool warnedMissingSprite = false;
            if (!warnedMissingSprite) {
                warnedMissingSprite = true;
                std::cerr << "[SpriteAtlas] Sprite no encontrado: '" << inst.currentSpriteName
                          << "'. Usando fallback.\n";
            }

            const std::string fb0 = inst.spritePrefix + ".abajo.0";
            const std::string fb1 = inst.spritePrefix + ".derecha.0";
            const std::string fb2 = inst.spritePrefix + ".arriba.0";
            const std::string fallbacks[] = { fb0, fb1, fb2 };

            for (const auto& fb : fallbacks) {
                if (gPlayerAtlas.sprites.find(fb) != gPlayerAtlas.sprites.end()) {
                    inst.currentSpriteName = fb;
                    inst.facingDirKey = GLFW_KEY_DOWN;
                    inst.flipX = 0.0f;
                    if (fb.find("arriba") != std::string::npos) {
                        inst.facingDirKey = GLFW_KEY_UP;
                    } else if (fb.find("derecha") != std::string::npos) {
                        inst.facingDirKey = GLFW_KEY_RIGHT;
                    } else {
                        inst.facingDirKey = GLFW_KEY_DOWN;
                    }
                    inst.isWalking = false;
                    inst.walkTimer = 0.0f;
                    inst.walkPhase = 0;
                    okUv = getUvRectForSprite(gPlayerAtlas, inst.currentSpriteName, uvRect);
                    break;
                }
            }

            // Último recurso: coger el primer sprite del atlas y calcular UVs a mano
            if (!okUv && !gPlayerAtlas.sprites.empty() && gPlayerAtlas.imageWidth > 0 && gPlayerAtlas.imageHeight > 0) {
                const auto& any = *gPlayerAtlas.sprites.begin();
                inst.currentSpriteName = any.first;
                const SpriteFrame& f = any.second;
                float u0 = (static_cast<float>(f.x) + 0.5f) / static_cast<float>(gPlayerAtlas.imageWidth);
                float v0 = (static_cast<float>(f.y) + 0.5f) / static_cast<float>(gPlayerAtlas.imageHeight);
                float u1 = (static_cast<float>(f.x + f.w) - 0.5f) / static_cast<float>(gPlayerAtlas.imageWidth);
                float v1 = (static_cast<float>(f.y + f.h) - 0.5f) / static_cast<float>(gPlayerAtlas.imageHeight);
                uvRect = glm::vec4(u0, v0, u1, v1);
                okUv = true;
            }
        }

        // Escala en NDC: el jugador ocupa exactamente 1 tile
        const float playerScaleFactor = 1.0f;
        const float sx = halfTile * playerScaleFactor;
        const float sy = halfTile * playerScaleFactor;
        model = glm::scale(model, glm::vec3(sx, sy, 1.0f));

        glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
        glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));
        glUniform1f(uniformFlipX, inst.flipX);
        glm::vec4 tint(1.0f, 1.0f, 1.0f, 1.0f);
        glUniform4fv(uniformTintColor, 1, glm::value_ptr(tint));

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }

    glBindVertexArray(0);
    glUseProgram(0);
}

