#include "controls_menu.hpp"
#include "in_game_menu.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <string>
#include <cctype>

static constexpr float scaleUsualHud = 0.0010f;
static constexpr float scaleMiniTitle = 0.000675f;

static constexpr glm::vec2 bomberman_1_Pos(-0.465f, 0.40f);
static constexpr glm::vec2 bomberman_2_Pos(0.04875f, 0.40f);
static constexpr glm::vec2 globalKeyPos(-0.465f, -0.20);
static constexpr glm::vec2 saveResetExitPos(0.355f, -0.275);

const std::vector<std::string> bombermanKeyStrings = {
    "BOMB",
    "DETONATE",
    "UP", 
    "LEFT",
    "DOWN",
    "RIGHT"
};

const std::vector<std::string> generalKeyStrings = {
    "2D-3D",
    "CAMERA",
    "WINDOW",
    "SELECT"
};

const std::vector<std::string> saveResetExitStrings = {
    "SAVE",
    "RESET",
    "EXIT"
};

// ============================== INITS / DESTROYERS ==============================

static GLuint createBlackTexture() {
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    unsigned char blackPixel[4] = { 0, 0, 0, 255 };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, blackPixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return texture;
}

// TODO, leer de fichero las teclas e introducirlas en las variables
ControlsMenu::ControlsMenu() : showControlsMenu(true) {

}

// TODO, limpiar 
ControlsMenu::~ControlsMenu() {}

// TODO, leer de fichero linea por linea los controles e introducirlos en las variables
void ControlsMenu::readFromFile() {

}
       
// TODO, guardar los cambios a fichero
void ControlsMenu::saveToFile() {

}

// TODO, cargar los controles default del fichero, usar readFromFile
void ControlsMenu::loadDefaultControls() {
    
}

// ============================== RENDER ==============================

// TODO, hacer una masa negra igual que el menu anterior para introducir las opciones
void ControlsMenu::renderControlsMenu(const SpriteAtlas& atlas, GLuint atlasTexture, 
                                      const SpriteAtlas& atlas2, GLuint atlasTexture2,  
                                      GLuint vao, GLuint uniformModel, GLuint uniformUvRect) {

    renderTextString("BOMBERMAN 1", bomberman_1_Pos + glm::vec2(0.0f, 0.1), scaleMiniTitle, atlas2, atlasTexture2, vao, uniformModel, uniformUvRect, 0);
    renderTextString("BOMBERMAN 2", bomberman_2_Pos + glm::vec2(0.0f, 0.1), scaleMiniTitle, atlas2, atlasTexture2, vao, uniformModel, uniformUvRect, 0);

    for (int i=0; i < bombermanKeyStrings.size(); i++) {
        renderTextString(bombermanKeyStrings[i], bomberman_1_Pos + glm::vec2(0.0f, -i * 0.075f), scaleUsualHud, atlas, atlasTexture, vao, uniformModel, uniformUvRect, 1);
        renderTextString(bombermanKeyStrings[i], bomberman_2_Pos + glm::vec2(0.0f, -i * 0.075f), scaleUsualHud, atlas, atlasTexture, vao, uniformModel, uniformUvRect, 1);
    }

    renderTextString("GLOBAL", globalKeyPos + glm::vec2(0.0f, 0.1), scaleMiniTitle, atlas2, atlasTexture2, vao, uniformModel, uniformUvRect, 0);
    
    for (int i=0; i < generalKeyStrings.size(); i++) {
        float separacion = (i == 0 ? 0.1f : 0.075f);
        renderTextString(generalKeyStrings[i], globalKeyPos + glm::vec2(0.0f, -i * 0.075f), scaleUsualHud, atlas, atlasTexture, vao, uniformModel, uniformUvRect, 1);
    }

    for (int i=0; i < saveResetExitStrings.size(); i++) {
        renderTextString(saveResetExitStrings[i], saveResetExitPos + glm::vec2(0.0f, -i * 0.075f), scaleUsualHud, atlas, atlasTexture, vao, uniformModel, uniformUvRect, 1);
    }

}

