#ifndef bomberman_game
#define bomberman_game

#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <map>
#include <string>

/*
 * bomberman.hpp
 * ------------
 * Bucle principal del juego (estado + update/render) y entrada de usuario.
 *
 * Responsabilidades:
 * - Mantener estado global del juego (`GameState`) y el modo (`GameMode`).
 * - Recibir entradas (teclas) y transformar en acciones.
 * - Orquestar init/update/render (no dibuja aquí directamente: está en bomberman.cpp).
 *
 * Nota:
 * - Este proyecto mantiene un mapa `keys` con el estado de cada tecla.
 */

enum GameState {
    GAME_PLAYING
};

enum class GameMode {
    OnePlayer,
    TwoPlayers
};

class Game
{
    private:
    public:

        // Estado de teclas: Release(0), Press(1), Repeat(2)
        std::map<GLint, GLint> keys; // <Key, {Release(0), Press(1), Repeat(2)}>

        // Última flecha pulsada (para resolver varias teclas sin prioridad fija)
        GLint lastDirKey = GLFW_KEY_UNKNOWN;

        // Última tecla WASD pulsada (para el jugador 2)
        GLint lastDirKeyP2 = GLFW_KEY_UNKNOWN;
        
        GameState state;

        GameMode mode = GameMode::OnePlayer;
        
        GLuint WIDTH, HEIGHT;
        GLFWwindow* window;

        float deltaTime = 0.0f;  // Tiempo entre frames (segundos)

        Game(GLFWwindow* window, GLuint width, GLuint height) : window(window), WIDTH(width), HEIGHT(height) {state = GAME_PLAYING;}
        ~Game();

        // Selecciona modo de juego (1 o 2 jugadores). Se aplica al hacer `init()`.
        void setMode(GameMode m) { mode = m; }

        // Inicializa recursos (shaders, texturas, mapa) y crea jugadores.
        void init();

        // Procesa inputs y aplica movimiento/animación.
        void processInput();
        // Actualiza el estado del juego (timers/animaciones).
        void update();

        // Renderiza mapa + jugadores.
        void render();
};



#endif //bomberman_game
