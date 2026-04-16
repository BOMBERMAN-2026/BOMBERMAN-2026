#include "in_game_menu.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <string>
#include <cctype>

static constexpr float scaleUsualHud = 0.0015f;

InGameMenu::InGameMenu() : showInGameMenu(false), menuOptionPos(0.0f, 0.0f), menuArrowTexture(0), blackTexture(0), 
                           inGameMenuHeight(0.55f), inGameMenuWidth(0.30f) {


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

void InGameMenu::renderTextString(const std::string& text, glm::vec2 startPos, float scale,
                                  const SpriteAtlas& atlas, GLuint atlasTexture, GLuint vao,
                                  GLuint uniformModel, GLuint uniformUvRect, bool useNaranja) {
    if (text.empty()) {
        return;
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlasTexture);
    glBindVertexArray(vao);

    float currentX = startPos.x;
    float y = startPos.y;
    const float interSeparation = 0.02f * scale;

    for (char c : text) {
        std::string spriteName(1, c);
        if (useNaranja) {
            spriteName = spriteName + "_Nar";
        } else {
            spriteName = spriteName + "_Ama";
        }
        glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);

        if (!getUvRectForSprite(atlas, spriteName, uvRect)) {
            std::cerr << "Advertencia: Sprite '" << spriteName << "' no encontrado en atlas de vocabulario amarillo\n";
            continue;
        }

        auto it = atlas.sprites.find(spriteName);
        float spriteWidth = (it != atlas.sprites.end()) ? static_cast<float>(it->second.w) * scale : 20.0f * scale;
        float spriteHeight = (it != atlas.sprites.end()) ? static_cast<float>(it->second.h) * scale : 23.0f * scale;

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(currentX + spriteWidth * 0.5f, y, 0.0f));
        model = glm::scale(model, glm::vec3(spriteWidth * 0.5f, spriteHeight * 0.5f, 1.0f));

        glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
        glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        currentX += spriteWidth + interSeparation;
    }
}

void InGameMenu::renderInGameMenu(GLuint VAO, GLuint shader, GLuint uniformModel, GLuint uniformProjection, GLuint uniformUvRect, 
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

    renderTextString("PAUSE", menuOptionPos + glm::vec2(0.10f, 0.0f), scaleUsualHud * 2.25f, gVocabNaranjaAtlas, vocabNaranjaTexture, VAO, uniformModel, uniformUvRect, true);
    
    // Renderizar texto dentro del menú
    for (int i=0 ; i < menuOptions.size(); i++) {
        renderTextString(menuOptions[i], menuOptionPos + glm::vec2(0.0f, 0.05f - i * 0.125f), scaleUsualHud, gVocabAmarilloAtlas, vocabAmarilloTexture, VAO, uniformModel, uniformUvRect, false);
    }
    //renderTextString("EXIT", menuOptionPos + glm::vec2(-0.18f, 0.05f), scaleUsualHud, gVocabAmarilloAtlas, vocabAmarilloTexture, VAO, uniformModel, uniformUvRect);

    // ===== RENDERIZAR FLECHA DE SELECCIÓN =====
    // glBindTexture(GL_TEXTURE_2D, menuArrowTexture);

    // float arrowY = menuArrowY_Base - (menuSelection * menuArrowY_Offset);
    // float arrowX = menuArrowX;
    // float arrowScale = 0.1f;

    // model = glm::mat4(1.0f);
    // model = glm::translate(model, glm::vec3(arrowX, arrowY, 0.0f));
    // model = glm::scale(model, glm::vec3(arrowScale, arrowScale, 1.0f));
    // glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));

    // glUniform4fv(uniformTintColor, 1, glm::value_ptr(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)));

    // uvRect = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
    // if (!menuBombAtlas.sprites.empty()) {
    //     std::string spriteNameToUse;

    //     if (menuArrowSelected) {
    //         if (menuArrowAnimTimer < 0.05f) {
    //             spriteNameToUse = "explosion_2";
    //         } else {
    //             spriteNameToUse = (menuArrowAnimTimer < menuArrowSelectedAnimSpeed * 0.5f) ? "explosion_3" : "explosion_4";
    //         }
    //     } else {
    //         spriteNameToUse = (menuArrowAnimTimer < menuArrowAnimSpeed * 0.5f) ? "explosion_0" : "explosion_1";
    //     }

    //     getUvRectForSprite(menuBombAtlas, spriteNameToUse, uvRect);
    // }
    // glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));
    // glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);
    //glUseProgram(0);
}



