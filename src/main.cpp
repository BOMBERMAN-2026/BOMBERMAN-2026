// GL and GLFW
#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <algorithm>

#include "bomberman.hpp"
#include "audio_manager.hpp"

/*
 * main.cpp
 * -------
 * Punto de entrada.
 *
 * Responsabilidades:
 * - Inicializar GLFW/GLEW y crear la ventana.
 * - Construir `Game` y ejecutar el loop principal (input/update/render).
 * - Registrar el callback de teclado (ver README para controles).
 */

Game* bomberman;

extern int menuSelection;

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

#include <ctime>

int main() {
    srand(static_cast<unsigned int>(time(nullptr)));

    // ============================== Ventana / OpenGL ==============================

    // Initialize GLFW
    if (!glfwInit())
    {
        printf("GLFW Initialization failed!\n");
        return 1;
    }

    // Setup GLFW window properties
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    // Screen dimensions (Fullscreen)
    GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);
    
    // Configurar GLFW para usar la resolución actual del monitor en fullscreen
    const int SCREEN_WIDTH = mode->width;
    const int SCREEN_HEIGHT = mode->height;

    // Crear la ventana en modo fullscreen pasando el monitor
    GLFWwindow* mainWindow = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Bomberman", primaryMonitor, nullptr);
    if (!mainWindow)
    {
        printf("GLFW window creation failed!\n");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(mainWindow);
    glfwSetFramebufferSizeCallback(mainWindow, framebuffer_size_callback);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK)
    {
        printf("GLEW Initialization failed!\n");
        glfwTerminate();
        return 1;
    }

    glfwSetKeyCallback(mainWindow, key_callback);
    glfwSetScrollCallback(mainWindow, scroll_callback);

    // OpenGL configuration
    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    glClearDepth(1.0f);

    // ============================== Juego ==============================
    bomberman = new Game(mainWindow, SCREEN_WIDTH, SCREEN_HEIGHT);
    // Cambia a GameMode::HistoryOnePlayer si quieres modo 1 jugador.
    bomberman->setMode(GameMode::HistoryTwoPlayers);
    bomberman->init();

    // Initialize blend to blend transparent to the background
    //glEnable(GL_BLEND);
    //glEnable(GL_DEPTH_TEST);
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // inicializar juego, ticks?

    // ============================== Loop principal ==============================
    double lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(mainWindow))
    {
        double currentTime = glfwGetTime();
        const float frameDelta = static_cast<float>(currentTime - lastTime);
        bomberman->deltaTime = std::max(0.0f, std::min(frameDelta, 0.05f));
        lastTime = currentTime;

        // Check and call events
        glfwPollEvents();

        // Pollear gamepads y traducir a teclas equivalentes.
        pollGamepads(bomberman->keys, bomberman->lastDirKey, bomberman->lastDirKeyP2);

        // Deberia estar dentro de un bucle de ticks?
        bomberman->processInput();
        // Deberia estar dentro de un bucle de ticks?
        bomberman->update();

        bomberman->render();
        
        // Swap buffers
		glfwSwapBuffers(mainWindow);
    }

    delete bomberman;
    bomberman = nullptr;
    glfwDestroyWindow(mainWindow);
    glfwTerminate();

    return 0;
}

// Callback de teclado (GLFW). Guarda estado y recuerda “última dirección” por jugador.
// Controles completos: ver README.
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode) {
    if (bomberman == nullptr) {
        return;
    }

    if (key >= 0 && key < 1024) {
        bomberman->keys[key] = action;
        bomberman->lastKeyPressed = key;

        if (action == GLFW_PRESS) {
            // ========== INTRO: pasar al menú ==========
            if (bomberman->state == GAME_INTRO && key == GLFW_KEY_SPACE) {
                bomberman->state = GAME_MENU;
                bomberman->init(); // Cargar textura del menú
                return;
            }

            // ========== JUEGO NORMAL ==========
            if (bomberman->state == GAME_PLAYING) {
                if (key == GLFW_KEY_4 || key == GLFW_KEY_KP_4) {
                    AudioManager::get().registerPlaceBombSpecialTap();
                }
            }

            if (key == bomberman->inGameMenu.controlsMenu.upKey_P1 || key == bomberman->inGameMenu.controlsMenu.downKey_P1 || key == bomberman->inGameMenu.controlsMenu.leftKey_P1 || key == bomberman->inGameMenu.controlsMenu.rightKey_P1) {
                bomberman->lastDirKey = key;
            }

            if (key == bomberman->inGameMenu.controlsMenu.upKey_P2 || key == bomberman->inGameMenu.controlsMenu.downKey_P2 || key == bomberman->inGameMenu.controlsMenu.leftKey_P2 || key == bomberman->inGameMenu.controlsMenu.rightKey_P2) {
                bomberman->lastDirKeyP2 = key;
            }
        }
    }
}

// Ajusta el viewport cuando cambia el tamaño de la ventana.
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);

    if (bomberman != nullptr) {
        bomberman->onResize(width, height);
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    (void)window;
    (void)xoffset;

    if (bomberman != nullptr) {
        bomberman->onMouseScroll(yoffset);
    }
}
