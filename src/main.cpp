// GL and GLFW
#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <iostream>

#include "bomberman.hpp"

/*
 * main.cpp
 * -------
 * Punto de entrada.
 *
 * Responsabilidades:
 * - Inicializar GLFW/GLEW y crear la ventana.
 * - Construir `Game` y ejecutar el loop principal (input/update/render).
 * - Registrar el callback de teclado (flechas para P1, WASD para P2).
 */

Game* bomberman;

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);

int main() {

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

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK)
    {
        printf("GLEW Initialization failed!\n");
        glfwTerminate();
        return 1;
    }

    glfwSetKeyCallback(mainWindow, key_callback);

    // OpenGL configuration
    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    // ============================== Juego ==============================
    bomberman = new Game(mainWindow, SCREEN_WIDTH, SCREEN_HEIGHT);
    // Cambia a GameMode::OnePlayer si quieres modo 1 jugador.
    bomberman->setMode(GameMode::TwoPlayers);
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
        bomberman->deltaTime = static_cast<float>(currentTime - lastTime);
        lastTime = currentTime;

        // Check and call events
        glfwPollEvents();

        // Deberia estar dentro de un bucle de ticks?
        bomberman->processInput();
        // Deberia estar dentro de un bucle de ticks?
        bomberman->update();

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        bomberman->render();
        
        // Swap buffers
		glfwSwapBuffers(mainWindow);
    }

    return 0;
}

// Callback de teclado (GLFW). Guarda estado y recuerda “última dirección” por jugador.
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode) {
    if (key >= 0 && key < 1024) {
        bomberman->keys[key] = action;

        if (action == GLFW_PRESS) {
            if (key == GLFW_KEY_UP || key == GLFW_KEY_DOWN || key == GLFW_KEY_LEFT || key == GLFW_KEY_RIGHT) {
                bomberman->lastDirKey = key;
            }

            if (key == GLFW_KEY_W || key == GLFW_KEY_A || key == GLFW_KEY_S || key == GLFW_KEY_D) {
                bomberman->lastDirKeyP2 = key;
            }
        }
    }
}
