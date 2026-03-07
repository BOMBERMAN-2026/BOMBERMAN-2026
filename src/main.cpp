// GL and GLFW
#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <iostream>

#include "bomberman.hpp"

Game* bomberman;

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);

int main() {

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

    // Screen dimensions
    int resolutionHeight = glfwGetVideoMode(glfwGetPrimaryMonitor())->height;

	const int SCREEN_HEIGHT = 0.9 * resolutionHeight;
    //const int SCREEN_HEIGHT = 300;
    const int SCREEN_WIDTH = SCREEN_HEIGHT;

    GLFWwindow* mainWindow = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Bomberman", nullptr, nullptr);
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

    bomberman = new Game(mainWindow, SCREEN_WIDTH, SCREEN_HEIGHT);
    bomberman->init();

    // Initialize blend to blend transparent to the background
    //glEnable(GL_BLEND);
    //glEnable(GL_DEPTH_TEST);
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // inicializar juego, ticks?

    while (!glfwWindowShouldClose(mainWindow))
    {
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

/*
* It is called when a key is pressed/released
*/
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode) {
    if (key >= 0 && key < 1024) {
        bomberman->keys[key] = action;
        //Game::lastKey = key;
    }
}
