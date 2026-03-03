// System includes
#include <cstdio>
#include <cstdlib>

// OpenGL includes
#include <GL/glew.h>
#include <GLFW/glfw3.h>

// GLM includes
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Constants
const int WIDTH = 800;
const int HEIGHT = 600;

int main()
{
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

    GLFWwindow* mainWindow = glfwCreateWindow(WIDTH, HEIGHT, "P1.2_videojuegos", nullptr, nullptr);
    if (!mainWindow)
    {
        printf("GLFW window creation failed!\n");
        glfwTerminate();
        return 1;
    }
    int bufferWidth, bufferHeight;
    glfwGetFramebufferSize(mainWindow, &bufferWidth, &bufferHeight);
    glfwMakeContextCurrent(mainWindow);
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK)
    {
        printf("GLEW Initialization failed!\n");
        glfwTerminate();
        return 1;
    }
    glViewport(0, 0, bufferWidth, bufferHeight);
}