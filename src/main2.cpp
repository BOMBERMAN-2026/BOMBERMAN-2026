// GL and GLFW
#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <iostream>

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

    // OpenGL configuration
    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    // Initialize blend to blend transparent to the background
    glEnable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    
    // inicializar juego, ticks?


    while (!glfwWindowShouldClose(mainWindow))
    {
        // Check and call events
        glfwPollEvents();

        /*
        */

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        // Swap buffers
		glfwSwapBuffers(mainWindow);
    }

    return 0;
}
