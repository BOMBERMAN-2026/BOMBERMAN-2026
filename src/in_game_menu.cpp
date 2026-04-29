#include "in_game_menu.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <string>
#include <cctype>

// ============================== GLOBAL VARIABLES ==============================

static constexpr float scaleUsualHud = 0.0015f;

static constexpr glm::vec2 menuOptionPos(0.0f, 0.0f);
static constexpr glm::vec2 pausePos(-0.45f, 0.45f);
static constexpr glm::vec2 initMenuOptionsPos(-0.45f, 0.27f);
static constexpr glm::vec2 currentOptionsSelectedPos(0.05f, 0.145f);

const std::vector<std::string> inGameMenuOptions = {
    "CONTINUE",
    "MUSIC",
    "SOUNDS",
    "GRAPHICS",
    "CAMERA",
    "CONTROLS",
    "EXIT"
};

std::vector<std::string> currentOptionsSelected;

// ============================== INITS / DESTROYERS ==============================

InGameMenu::InGameMenu() : showInGameMenu(false), menuArrowTexture(0), blackTexture(0), 
                           inGameMenuHeight(0.55f), inGameMenuWidth(0.30f), posSeleccion(0) {
    currentOptionsSelected = {
        "ON",
        "ON",
        "2D",
        "LOCKED"
    };

}

InGameMenu::~InGameMenu() {
    if (blackTexture != 0) {
        glDeleteTextures(1, &blackTexture);
        blackTexture = 0;
    }
}

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

// ============================== RENDER ==============================

/** Render del inGameMenu
 *  El valor de colorUse esta entre 0 (color naranja), 1 (color amarillo) y 2 (color amarillo blanquecino) 
 */

void InGameMenu::renderTextString(const std::string& text, glm::vec2 startPos, float scale,
                                  const SpriteAtlas& atlas, GLuint atlasTexture, GLuint vao,
                                  GLuint uniformModel, GLuint uniformProjection, GLuint uniformUvRect, int colorUse) {
    if (text.empty()) {
        return;
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlasTexture);
    glBindVertexArray(vao);

    glm::mat4 orthoProj = glm::ortho(-2.0f, 2.0f, -1.0f, 1.0f);
    glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, glm::value_ptr(orthoProj));

    float currentX = startPos.x;
    float y = startPos.y;
    const float interSeparation = 0.02f * scale;

    for (char c : text) {
        std::string spriteName(1, c);
        if (spriteName == " ") {
            currentX += 0.054f;
            continue;
        }

        if (colorUse == 0)
            spriteName = spriteName + "_Nar";
        else if (colorUse == 1)
            spriteName = spriteName + "_Ama";
        else 
            spriteName = spriteName + "_AmaBla";
            
        glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);

        if (!getUvRectForSprite(atlas, spriteName, uvRect)) {
            std::cerr << "Advertencia: Sprite '" << spriteName << "' no encontrado en atlas de vocabulario amarillo\n";
            continue;
        }

        auto it = atlas.sprites.find(spriteName);
        float spriteWidth = (it != atlas.sprites.end()) ? static_cast<float>(it->second.w) * scale : 20.0f * scale;
        float spriteHeight = (it != atlas.sprites.end()) ? static_cast<float>(it->second.h) * scale : 23.0f * scale;

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(currentX + spriteWidth * 0.5f, y, 0.1f)); // 0.1f en Z para que se pueda mostrar en el 3D
        model = glm::scale(model, glm::vec3(spriteWidth * 0.5f, spriteHeight * 0.5f, 1.0f));

        glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
        glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        currentX += spriteWidth + interSeparation;
    }
}

void InGameMenu::renderInGameMenu(GLuint VAO, GLuint shader, GLuint uniformModel, GLuint uniformProjection, GLuint uniformUvRect, GLuint uniformFlipX,
                                  SpriteAtlas gVocabAmarilloAtlas, GLuint vocabAmarilloTexture,
                                  SpriteAtlas gVocabNaranjaAtlas, GLuint vocabNaranjaTexture) {

    glUseProgram(shader);
    glActiveTexture(GL_TEXTURE0);
    if (blackTexture == 0) {
        blackTexture = createBlackTexture();
    }
    glBindTexture(GL_TEXTURE_2D, blackTexture);
    glBindVertexArray(VAO);

    //float aspect = (float)inGameMenuWidth / (float)inGameMenuHeight;
    // glm::mat4 projection = glm::ortho(-aspect, aspect, -1.0f, 1.0f, -1.0f, 1.0f);
    // glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, glm::value_ptr(projection));

    glUniform1f(uniformFlipX, 0.0f);

    glm::mat4 orthoProj = glm::ortho(-2.0f, 2.0f, -1.0f, 1.0f);
    glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, glm::value_ptr(orthoProj));

    // Renderizar fondo del menú
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(menuOptionPos, 0.0f)); // Le damos la posición fixeada del menú
    model = glm::scale(model, glm::vec3(inGameMenuHeight, inGameMenuHeight, 1.0f));
    glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));

    // glm::vec4 tintColor(1.0f, 1.0f, 1.0f, 1.0f);
    // glUniform4fv(uniformTintColor, 1, glm::value_ptr(tintColor));

    glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
    glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    if (controlsMenu.showControlsMenu) {
        controlsMenu.renderControlsMenu(gVocabAmarilloAtlas, vocabAmarilloTexture, gVocabNaranjaAtlas, vocabNaranjaTexture, VAO, uniformModel, uniformProjection, uniformUvRect);
    }
    else {
        renderTextString("PAUSE", pausePos, scaleUsualHud * 1.50f, gVocabNaranjaAtlas, vocabNaranjaTexture, VAO, uniformModel, uniformProjection, uniformUvRect, 0);
    
        // Renderizar texto dentro del menú
            // Parte de la izq
        for (int i=0 ; i < inGameMenuOptions.size(); i++) {
            if (i == posSeleccion) renderTextString(inGameMenuOptions[i], initMenuOptionsPos + glm::vec2(0.0f, -i * 0.125f), scaleUsualHud, gVocabAmarilloAtlas, vocabAmarilloTexture, VAO, uniformModel, uniformProjection, uniformUvRect, 2);
            else renderTextString(inGameMenuOptions[i], initMenuOptionsPos + glm::vec2(0.0f, -i * 0.125f), scaleUsualHud, gVocabAmarilloAtlas, vocabAmarilloTexture, VAO, uniformModel, uniformProjection, uniformUvRect, 1);
        }

            // Parte de la derecha
        for (int i=0; i < currentOptionsSelected.size();  i++) {
            renderTextString(currentOptionsSelected[i], currentOptionsSelectedPos + glm::vec2(0.0f, -i * 0.125f), scaleUsualHud, gVocabAmarilloAtlas, vocabAmarilloTexture, VAO, uniformModel, uniformProjection, uniformUvRect, 1);
        }
    }
    
    glBindVertexArray(0);
}

// ============================== INPUT ==============================

int InGameMenu::processInputInGameMenu(std::map<int, int>& keys) {

    // Devolvemos un valor entre los siguientes
    // -1 -> no hay que hacer nada en bomberman.cpp
    //  1 -> hay que silenciar la musica
    //  2 -> hay que silenciar los efectos de sonido
    //  3 -> hay que alternar entre 2D y 3D
    //  4 -> hay que cambiar la camara
    //  6 -> hay que volver al menu de seleccion de juego
    int result = -1;

    if (keys[controlsMenu.downKey_P1] == GLFW_PRESS) {
        posSeleccion >= inGameMenuOptions.size() - 1 ? posSeleccion = 0 : posSeleccion += 1;
        keys[controlsMenu.downKey_P1] = GLFW_REPEAT;
    }
    
    if (keys[controlsMenu.upKey_P1] == GLFW_PRESS) {
        posSeleccion <= 0 ? posSeleccion = inGameMenuOptions.size() - 1 : posSeleccion -= 1;
        keys[controlsMenu.upKey_P1] = GLFW_REPEAT;
    }

    if (keys[controlsMenu.selectKey] == GLFW_PRESS) {
        
        // Comportamiento condicionado al indice en el vector currentOptionsSelected 
        switch (posSeleccion) {
            // CONTINUE 
            case 0:
                showInGameMenu = false;
                posSeleccion = 0;
                break;
            // MUSIC
            case 1:
                result = 1;
                currentOptionsSelected[posSeleccion - 1] == "ON" ? currentOptionsSelected[posSeleccion - 1] = "OFF" : currentOptionsSelected[posSeleccion - 1] = "ON";
                break;

            // SOUNDS
            case 2:
                result = 2;
                currentOptionsSelected[posSeleccion - 1] == "ON" ? currentOptionsSelected[posSeleccion - 1] = "OFF" : currentOptionsSelected[posSeleccion - 1] = "ON";
                break;

            // GRAPHICS
            case 3:
                result = 3;
                currentOptionsSelected[posSeleccion - 1] == "2D" ? currentOptionsSelected[posSeleccion - 1] = "3D" : currentOptionsSelected[posSeleccion - 1] = "2D";
                break;

            // CAMERA
            case 4:
                result = 4;
                // LOCKED -> FREE -> BOMBERMAN -> LOCKED -> ...
                if (currentOptionsSelected[posSeleccion - 1] == "LOCKED") currentOptionsSelected[posSeleccion - 1] = "FREE";
                else if (currentOptionsSelected[posSeleccion - 1] == "FREE") currentOptionsSelected[posSeleccion - 1] = "BOMBERMAN";
                else currentOptionsSelected[posSeleccion - 1] = "LOCKED";
                break;

            // CONTROLS
            case 5:
                controlsMenu.showControlsMenu = true;
                break;
            
            // EXIT
            case 6:
                result = 6;
                showInGameMenu = false;
                posSeleccion = 0;
                break;
            default:
                break;
        }

        keys[controlsMenu.selectKey] = GLFW_REPEAT;
    }

    return result;

}

