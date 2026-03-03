#ifndef bomberman_game
#define bomberman_game

#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <map>
#include <string>

enum GameState {
    GAME_PLAYING
};

class Game 
{
    private:
    public:

        std::map<GLint, GLint> keys; // <Key, {Release(0), Press(1)}>
        
        GameState state;
        
        GLuint WIDTH, HEIGHT;
        GLFWwindow* window;

        Game(GLFWwindow* window, GLuint width, GLuint height) : window(window), WIDTH(width), HEIGHT(height) {}
        ~Game();

        void init();

        // Procesar los inputs que se producen y sus consecuencias
        void processInput();
        // Modificar el estado del juego
        void update();
};





#endif //bomberman_game
