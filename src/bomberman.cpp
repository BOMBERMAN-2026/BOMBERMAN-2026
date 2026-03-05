#include "bomberman.hpp"
#include "player.hpp"
#include "sprite_atlas.hpp"
#include "game_map.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <string>
#include <cstdlib>

Player* player;
GameMap* gameMap;
GLuint mapTexture;

/**************************************************** Solo para que funcione de primeras ****************************************/

// Global variables for OpenGL
GLuint VAO, VBO, EBO, shader, uniformModel, uniformTexture, uniformTintColor, uniformUvRect, uniformFlipX;

GLuint texture;

// Vertex Shader
static const char* vShaderSrc = R"(
#version 330
layout (location = 0) in vec3 pos;
layout (location = 1) in vec2 texCoord;
out vec2 TexCoord;
uniform mat4 model;
uniform vec4 uvRect; // (u0, v0, u1, v1)
uniform float flipX; // 0.0 normal, 1.0 mirror horizontally
void main()
{
    gl_Position = model * vec4(pos, 1.0);
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
static std::string gCurrentSpriteName;
static float gFlipX = 0.0f;

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

/**************************************************** Solo para que funcione de primeras ****************************************/


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

    // Cargar textura de la sprite sheet del mapa
    const std::string mapTexPath = resolveAssetPath("resources/sprites/mapas/Stage1/sprites-Stage1.png");
    mapTexture = LoadTexture(mapTexPath.c_str());
    if (mapTexture == 0)
    {
        std::cerr << "Error cargando textura del mapa: " << mapTexPath << std::endl;
        std::exit(EXIT_FAILURE);
    }

    // Sprite inicial de prueba
    gCurrentSpriteName = "jugadorblanco.quieto.abajo.0";
    gFlipX = 0.0f;

    // Crear jugador en la posicion de spawn del mapa
    glm::vec2 spawnPos = gameMap->getSpawnPosition(0);
    player = new Player(spawnPos, glm::vec2(0.2f, 0.2f), 0.001f);
}

void Game::processInput() {
    
    if (this->state == GAME_PLAYING) {
        if (this->keys[GLFW_KEY_UP] >= GLFW_PRESS) {
            player->UpdateSprite(MOVE_UP, gameMap);
            gCurrentSpriteName = "jugadorblanco.quieto.arriba.0";
            gFlipX = 0.0f;
        }
        if (this->keys[GLFW_KEY_DOWN] >= GLFW_PRESS) {
            player->UpdateSprite(MOVE_DOWN, gameMap);
            gCurrentSpriteName = "jugadorblanco.quieto.abajo.0";
            gFlipX = 0.0f;
        }
        if (this->keys[GLFW_KEY_LEFT] >= GLFW_PRESS) {
            player->UpdateSprite(MOVE_LEFT, gameMap);
            gCurrentSpriteName = "jugadorblanco.quieto.derecha.0";
            gFlipX = 1.0f;
        }
        if (this->keys[GLFW_KEY_RIGHT] >= GLFW_PRESS) {
            player->UpdateSprite(MOVE_RIGHT, gameMap);
            gCurrentSpriteName = "jugadorblanco.quieto.derecha.0";
            gFlipX = 0.0f;
        }
    }

}

void Game::update() {}

void Game::render() {

    glUseProgram(shader);

    // Texture unit 0
    glUniform1i(uniformTexture, 0);

    // === 1. Renderizar mapa (fondo) ===
    gameMap->render(VAO, mapTexture, uniformModel, uniformUvRect, uniformTintColor, uniformFlipX);

    // === 2. Renderizar jugador (encima del mapa) ===
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);

    glBindVertexArray(VAO);

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(player->position, 0.0f));

    // Escalar el sprite del jugador para que ocupe ~1 tile del mapa
    float halfTile = gameMap->getTileSize() / 2.0f;

    // UV del sprite actual (si falla, se pinta la textura completa)
    glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
    getUvRectForSprite(gPlayerAtlas, gCurrentSpriteName, uvRect);

    // Ajuste simple de escala en función del tamaño del frame (referencia: 32x32)
    int frameW = 32;
    int frameH = 32;
    auto it = gPlayerAtlas.sprites.find(gCurrentSpriteName);
    if (it != gPlayerAtlas.sprites.end()) {
        frameW = (it->second.w > 0) ? it->second.w : 32;
        frameH = (it->second.h > 0) ? it->second.h : 32;
    }

    // Escala en NDC basada en el tamaño de tile del mapa
    const float sx = halfTile;
    const float sy = halfTile;
    model = glm::scale(model, glm::vec3(sx, sy, 1.0f));

    glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
    glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));
    glUniform1f(uniformFlipX, gFlipX);
    glm::vec4 tint(1.0f, 1.0f, 1.0f, 1.0f);
    glUniform4fv(uniformTintColor, 1, glm::value_ptr(tint));

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);
    glUseProgram(0);
}

