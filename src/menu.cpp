#include "menu.hpp"
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

MenuScreen::MenuScreen()
    : menuBackgroundTexture(0), menuArrowTexture(0), menuSelection(0),
      menuArrowX(-1.1f), menuArrowY_Base(0.435f), menuArrowY_Offset(0.275f),
      menuArrowAnimTimer(0.0f), menuArrowAnimSpeed(0.1f),
      menuArrowSelected(false), menuArrowSelectedAnimSpeed(0.8f),
      menuSelectedWaitTimer(0.0f), shouldTransitionToGame(false),
      shouldExitGame(false), selectedGameMode(GameMode::HistoryOnePlayer) {
}

// Libera texturas de intro/menú (si se llegaron a crear).
MenuScreen::~MenuScreen() {
    if (menuBackgroundTexture != 0) glDeleteTextures(1, &menuBackgroundTexture);
    if (menuArrowTexture != 0) glDeleteTextures(1, &menuArrowTexture);
}

// ============================== MENU ==============================

// Inicializa/reinicia el menú (resetea flags y carga recursos si faltan).
void MenuScreen::initMenu() {
    // Reset de flags (evita auto-arranque al volver del juego).
    shouldTransitionToGame = false;
    selectedGameMode = GameMode::HistoryOnePlayer;

    // Cargar sólo si falta (evita recargar al volver al menú).
    if (menuBackgroundTexture == 0) {
        menuBackgroundTexture = LoadTexture(resolveAssetPath("resources/sprites/intro_menu/MenuScreenV3.jpg").c_str());
        if (menuBackgroundTexture == 0) {
            std::cerr << "Error cargando MenuScreenV3.jpg\n";
        }
    }

    // Reutilizar atlas si ya está cargado.
    if (menuBombAtlas.sprites.empty()) {
        const std::string menuBombAtlasPath = resolveAssetPath("resources/sprites/atlases/SpriteAtlasMenuBomb.json");
        if (!loadSpriteAtlasMinimal(menuBombAtlasPath, menuBombAtlas)) {
            std::cerr << "Error cargando SpriteAtlasMenuBomb.json\n";
        }
    }

    // Textura de la flecha (explosión animada).
    if (menuArrowTexture == 0 && !menuBombAtlas.imagePath.empty()) {
        menuArrowTexture = LoadTexture(resolveAssetPath(menuBombAtlas.imagePath).c_str());
        if (menuArrowTexture == 0) {
            std::cerr << "Error cargando SpriteBombaAtlas.png\n";
        }
    }

    menuSelection = 0;
    menuArrowSelected = false;
    menuSelectedWaitTimer = 0.0f;
    menuArrowAnimTimer = 0.0f;
}

void MenuScreen::updateMenu(float deltaTime) {
    if (menuArrowSelected) {
        // Alternar entre explosion_3 y explosion_4 lentamente
        menuArrowAnimTimer += deltaTime;
        if (menuArrowAnimTimer >= menuArrowSelectedAnimSpeed) {
            menuArrowAnimTimer = 0.0f;
        }

        // Después de que termina la animación, esperar y luego pasar al juego
        menuSelectedWaitTimer += deltaTime;
        if (menuSelectedWaitTimer >= MENU_SELECTED_WAIT_TIME) {
            // Mapear selección -> modo y transicionar al juego.
            if (menuSelection == 0) {
                selectedGameMode = GameMode::VsOnePlayer;
                shouldTransitionToGame = true;
            } else if (menuSelection == 1) {
                selectedGameMode = GameMode::VsTwoPlayers;
                shouldTransitionToGame = true;
            } else if (menuSelection == 2) {
                selectedGameMode = GameMode::HistoryOnePlayer;
                shouldTransitionToGame = true;
            } else if (menuSelection == 3) {
                selectedGameMode = GameMode::HistoryTwoPlayers;
                shouldTransitionToGame = true;
            }
            // TODO: Cargar otras opciones de juego
            // Las opciones Vs (0,1,4) no hacen nada aún
        }
    } else {
        // Alternar entre explosion_0 y explosion_1 rápidamente
        menuArrowAnimTimer += deltaTime;
        if (menuArrowAnimTimer >= menuArrowAnimSpeed) {
            menuArrowAnimTimer = 0.0f;
        }
    }
}

void MenuScreen::renderMenu(GLuint VAO, GLuint shader, GLuint uniformModel, GLuint uniformProjection,
                                  GLuint uniformTexture, GLuint uniformUvRect, GLuint uniformTintColor, GLuint uniformFlipX,
                                  int WIDTH, int HEIGHT) {
    glUseProgram(shader);
    glUniform1f(uniformFlipX, 0.0f);
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

void MenuScreen::processInputMenu(std::map<int, int>& keys, ControlsMenu& controls) {
    if (keys[controls.upKey_P1] == GLFW_PRESS) {
        if (!menuArrowSelected) {
            menuSelection = (menuSelection - 1 + NUM_MENU_OPTIONS) % NUM_MENU_OPTIONS;
            keys[controls.upKey_P1] = GLFW_REPEAT;
        }
    }
    if (keys[controls.downKey_P1] == GLFW_PRESS) {
        if (!menuArrowSelected) {
            menuSelection = (menuSelection + 1) % NUM_MENU_OPTIONS;
            keys[controls.downKey_P1] = GLFW_REPEAT;
        }
    }

    // Confirmar selección.
    if (keys[controls.selectKey] == GLFW_PRESS) {
        if (!menuArrowSelected) {
            // Solo permitir transición en opciones Historia (2, 3)
            if (menuSelection >= 2 && menuSelection <= 3) {
                menuArrowSelected = true;
                menuArrowAnimTimer = 0.0f;
                keys[controls.selectKey] = GLFW_REPEAT;
            }
            // TODO: Permitir selección de modos Vs (0, 1, 4) cuando estén implementados. Por ahora, solo modos Historia (2, 3) hacen algo al pulsar Enter.
            // Las opciones Vs (0, 1, 4) ignoran Enter por ahora
        }
    }

    // Escape para cerrar el juego desde el menú.
    if (keys[GLFW_KEY_ESCAPE] == GLFW_PRESS) {
        shouldExitGame = true;
        keys[GLFW_KEY_ESCAPE] = GLFW_REPEAT;
    }
}
