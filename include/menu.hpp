#ifndef MENU_HPP
#define MENU_HPP

#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <map>

#include "sprite_atlas.hpp"

// Forward declaration to avoid circular dependency
enum class GameMode;

/**
 * MenuScreen
 * ---------------
 * Gestiona la pantalla de introducción (video intro) y el menú de selección de modo de juego.
 * Encapsula toda la lógica y rendering de estas pantallas, reduciendo la complejidad de bomberman.cpp
 */
class MenuScreen {
private:

    // ===== MENU GAMEMODE SELECTOR =====
    GLuint menuBackgroundTexture;
    GLuint menuArrowTexture;
    SpriteAtlas menuBombAtlas;
    int menuSelection;  // 0 = Vs 1P (no implementado), 1 = Vs 2P (no implementado), 2 = Historia 1P, 3 = Historia 2P
    const int NUM_MENU_OPTIONS = 4;

    // Posición de la flecha de selección
    float menuArrowX;
    float menuArrowY_Base;
    float menuArrowY_Offset;

    // Animación de la flecha del menú
    float menuArrowAnimTimer;
    float menuArrowAnimSpeed;
    bool menuArrowSelected;
    float menuArrowSelectedAnimSpeed;
    float menuSelectedWaitTimer;
    const float MENU_SELECTED_WAIT_TIME = 1.5f;

    // Estado interno
    bool shouldTransitionToGame;
    bool shouldExitGame;  // Flag para cerrar el juego desde el menú
    GameMode selectedGameMode;

public:
    MenuScreen();
    ~MenuScreen();

    // Inicialización
    void initMenu();

    // Actualización lógica
    void updateMenu(float deltaTime);

    // Renderizado
    void renderMenu(GLuint VAO, GLuint shader, GLuint uniformModel, GLuint uniformProjection,
                    GLuint uniformTexture, GLuint uniformUvRect, GLuint uniformTintColor, GLuint uniformFlipX,
                    int WIDTH, int HEIGHT);

    // Procesamiento de entrada
    void processInputMenu(std::map<int, int>& keys);

    // Getters
    int getMenuSelection() const { return menuSelection; }
    bool isMenuSelected() const { return menuArrowSelected; }
    bool shouldStartGame() const { return shouldTransitionToGame; }
    bool isExitRequested() const { return shouldExitGame; }
    GameMode getSelectedMode() const { return selectedGameMode; }
    void resetTransition() { shouldTransitionToGame = false; shouldExitGame = false; }
};

#endif // MENU_HPP
