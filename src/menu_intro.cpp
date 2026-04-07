#include "menu_intro.hpp"
#include "bomberman.hpp"
#include "sprite_atlas.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <cstdlib>
#include <map>

// Forward declaration
extern std::string resolveAssetPath(const std::string& assetPath);
extern GLuint LoadTexture(const char* filePath);
extern bool getUvRectForSprite(const SpriteAtlas& atlas, const std::string& spriteName, glm::vec4& uvRect);

MenuIntroScreen::MenuIntroScreen()
    : introVideoTexture(0), currentIntroFrame(0), introFrameTimer(0.0f),
      menuBackgroundTexture(0), menuArrowTexture(0), menuSelection(0),
      menuArrowX(-1.4f), menuArrowY_Base(-0.255f), menuArrowY_Offset(0.275f),
      menuArrowAnimTimer(0.0f), menuArrowAnimSpeed(0.1f),
      menuArrowSelected(false), menuArrowSelectedAnimSpeed(0.8f),
      menuSelectedWaitTimer(0.0f), shouldTransitionToGame(false),
      selectedGameMode(GameMode::OnePlayer) {
}

MenuIntroScreen::~MenuIntroScreen() {
    // Limpiar texturas si es necesario
    if (introVideoTexture != 0) glDeleteTextures(1, &introVideoTexture);
    if (menuBackgroundTexture != 0) glDeleteTextures(1, &menuBackgroundTexture);
    if (menuArrowTexture != 0) glDeleteTextures(1, &menuArrowTexture);
}

// ============================== INTRO ==============================

void MenuIntroScreen::initIntro() {
    // Cargar atlas JSON
    const std::string introAtlasPath = resolveAssetPath("resources/sprites/atlases/SpriteAtlasIntro.json");
    if (!loadSpriteAtlasMinimal(introAtlasPath, introAtlas)) {
        std::cerr << "Error cargando SpriteAtlasIntro.json\n";
    }

    // Cargar texture (solo 1 PNG)
    const std::string introTexPath = resolveAssetPath(introAtlas.imagePath);
    introVideoTexture = LoadTexture(introTexPath.c_str());
    if (introVideoTexture == 0) {
        std::cerr << "Error cargando intro video texture\n";
    }

    currentIntroFrame = 0;
    introFrameTimer = 0.0f;
}

bool MenuIntroScreen::updateIntro(float deltaTime) {
    introFrameTimer += deltaTime;
    if (introFrameTimer >= 1.0f / INTRO_VIDEO_FPS) {
        currentIntroFrame++;
        introFrameTimer = 0.0f;

        if (currentIntroFrame >= INTRO_FRAME_COUNT) {
            // Video terminó, señalar transición a menú
            currentIntroFrame = 0;
            introFrameTimer = 0.0f;
            return true;  // Indica que el intro terminó
        }
    }
    return false;  // Intro todavía en reproducción
}

void MenuIntroScreen::renderIntro(GLuint VAO, GLuint shader, GLuint uniformModel, GLuint uniformProjection,
                                   GLuint uniformTexture, GLuint uniformUvRect, GLuint uniformTintColor, GLuint uniformFlipX,
                                   int WIDTH, int HEIGHT) {
    glUseProgram(shader);

    float windowAspect = (float)WIDTH / (float)HEIGHT;
    float imageAspect = 640.0f / 360.0f;  // 16:9

    glm::mat4 projection;

    if (windowAspect > imageAspect) {
        float scale = windowAspect / imageAspect;
        projection = glm::ortho(-scale, scale, -1.0f, 1.0f, -1.0f, 1.0f);
    } else {
        float scale = imageAspect / windowAspect;
        projection = glm::ortho(-1.0f, 1.0f, -scale, scale, -1.0f, 1.0f);
    }

    glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, glm::value_ptr(projection));
    glUniform1i(uniformTexture, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, introVideoTexture);
    glBindVertexArray(VAO);

    std::string frameName = "intro_frame_" + std::to_string(currentIntroFrame);
    glm::vec4 uvRect;

    if (getUvRectForSprite(introAtlas, frameName, uvRect)) {
        glm::mat4 model = glm::mat4(1.0f);
        glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
        glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));
        glUniform4f(uniformTintColor, 1.0f, 1.0f, 1.0f, 1.0f);
        glUniform1i(uniformFlipX, 0);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }

    glBindVertexArray(0);
    glUseProgram(0);
}

// ============================== MENU ==============================

void MenuIntroScreen::initMenu() {
    menuBackgroundTexture = LoadTexture(resolveAssetPath("resources/sprites/intro_menu/MenuScreen.png").c_str());
    if (menuBackgroundTexture == 0) {
        std::cerr << "Error cargando MenuScreen.png\n";
    }

    const std::string menuBombAtlasPath = resolveAssetPath("resources/sprites/atlases/SpriteAtlasMenuBomb.json");
    if (!loadSpriteAtlasMinimal(menuBombAtlasPath, menuBombAtlas)) {
        std::cerr << "Error cargando SpriteAtlasMenuBomb.json\n";
    }

    menuArrowTexture = LoadTexture(resolveAssetPath(menuBombAtlas.imagePath).c_str());
    if (menuArrowTexture == 0) {
        std::cerr << "Error cargando SpriteBombaAtlas.png\n";
    }

    menuSelection = 0;
    menuArrowSelected = false;
    menuSelectedWaitTimer = 0.0f;
    menuArrowAnimTimer = 0.0f;
}

void MenuIntroScreen::updateMenu(float deltaTime) {
    if (menuArrowSelected) {
        // Alternar entre explosion_3 y explosion_4 lentamente
        menuArrowAnimTimer += deltaTime;
        if (menuArrowAnimTimer >= menuArrowSelectedAnimSpeed) {
            menuArrowAnimTimer = 0.0f;
        }

        // Después de que termina la animación, esperar y luego pasar al juego
        menuSelectedWaitTimer += deltaTime;
        if (menuSelectedWaitTimer >= MENU_SELECTED_WAIT_TIME) {
            // Señalar transición al juego
            selectedGameMode = (menuSelection == 0) ? GameMode::OnePlayer : GameMode::TwoPlayers;
            shouldTransitionToGame = true;
        }
    } else {
        // Alternar entre explosion_0 y explosion_1 rápidamente
        menuArrowAnimTimer += deltaTime;
        if (menuArrowAnimTimer >= menuArrowAnimSpeed) {
            menuArrowAnimTimer = 0.0f;
        }
    }
}

void MenuIntroScreen::renderMenu(GLuint VAO, GLuint shader, GLuint uniformModel, GLuint uniformProjection,
                                  GLuint uniformTexture, GLuint uniformUvRect, GLuint uniformTintColor, GLuint uniformFlipX,
                                  int WIDTH, int HEIGHT) {
    glUseProgram(shader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, menuBackgroundTexture);
    glBindVertexArray(VAO);

    float aspect = (float)WIDTH / (float)HEIGHT;
    glm::mat4 projection = glm::ortho(-aspect, aspect, -1.0f, 1.0f, -1.0f, 1.0f);
    glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, glm::value_ptr(projection));

    // Renderizar fondo del menú
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
    model = glm::scale(model, glm::vec3(aspect, 1.0f, 1.0f));
    glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));

    glm::vec4 tintColor(1.0f, 1.0f, 1.0f, 1.0f);
    glUniform4fv(uniformTintColor, 1, glm::value_ptr(tintColor));

    glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
    glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    // ===== RENDERIZAR FLECHA DE SELECCIÓN =====
    glBindTexture(GL_TEXTURE_2D, menuArrowTexture);

    float arrowY = menuArrowY_Base - (menuSelection * menuArrowY_Offset);
    float arrowX = menuArrowX;
    float arrowScale = 0.1f;

    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(arrowX, arrowY, 0.0f));
    model = glm::scale(model, glm::vec3(arrowScale, arrowScale, 1.0f));
    glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));

    glUniform4fv(uniformTintColor, 1, glm::value_ptr(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)));

    uvRect = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
    if (!menuBombAtlas.sprites.empty()) {
        std::string spriteNameToUse;

        if (menuArrowSelected) {
            if (menuArrowAnimTimer < 0.05f) {
                spriteNameToUse = "explosion_2";
            } else {
                spriteNameToUse = (menuArrowAnimTimer < menuArrowSelectedAnimSpeed * 0.5f) ? "explosion_3" : "explosion_4";
            }
        } else {
            spriteNameToUse = (menuArrowAnimTimer < menuArrowAnimSpeed * 0.5f) ? "explosion_0" : "explosion_1";
        }

        getUvRectForSprite(menuBombAtlas, spriteNameToUse, uvRect);
    }
    glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);
    glUseProgram(0);
}

// ============================== INPUT ==============================

void MenuIntroScreen::processInputMenu(std::map<int, int>& keys) {
    if (keys[GLFW_KEY_UP] == GLFW_PRESS || keys[GLFW_KEY_W] == GLFW_PRESS) {
        if (!menuArrowSelected) {
            menuSelection = (menuSelection - 1 + NUM_MENU_OPTIONS) % NUM_MENU_OPTIONS;
            keys[GLFW_KEY_UP] = GLFW_REPEAT;
        }
    }
    if (keys[GLFW_KEY_DOWN] == GLFW_PRESS || keys[GLFW_KEY_S] == GLFW_PRESS) {
        if (!menuArrowSelected) {
            menuSelection = (menuSelection + 1) % NUM_MENU_OPTIONS;
            keys[GLFW_KEY_DOWN] = GLFW_REPEAT;
        }
    }

    // Enter para seleccionar/confirmar
    if (keys[GLFW_KEY_ENTER] == GLFW_PRESS) {
        if (!menuArrowSelected) {
            menuArrowSelected = true;
            menuArrowAnimTimer = 0.0f;
            keys[GLFW_KEY_ENTER] = GLFW_REPEAT;
        }
    }
}
