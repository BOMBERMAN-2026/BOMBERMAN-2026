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

enum class ViewMode {
    Mode2D,
    Mode3D
};

enum class Camera3DType {
    OrthographicFixed,
    PerspectiveFixed,
    PerspectiveMobile,
    FirstPerson
};

class Game
{
    private:
    public:

        // Input
        std::map<GLint, GLint> keys;          // Estado: Release(0), Press(1), Repeat(2)
        GLint lastDirKey = GLFW_KEY_UNKNOWN;  // Última flecha pulsada (P1)
        GLint lastDirKeyP2 = GLFW_KEY_UNKNOWN; // Última WASD pulsada (P2)

        // Estado
        GameState state;
        GameMode mode = GameMode::OnePlayer;  // Se aplica al hacer init()
        ViewMode viewMode = ViewMode::Mode2D;
        Camera3DType camera3DType = Camera3DType::PerspectiveFixed;

        // Ventana
        GLint WIDTH, HEIGHT;                 // Tamaño ventana
        int windowedXPos, windowedYPos;      // Posición de la ventana en modo windowed (para restaurar al salir de fullscreen)  
        GLFWwindow* window;                  // GLFW window

        // Timing
        float deltaTime = 0.0f;               // Tiempo entre frames (segundos)

        Game(GLFWwindow* window, GLuint width, GLuint height) : window(window), WIDTH(width), HEIGHT(height) {state = GAME_PLAYING;}
        ~Game();

        void setMode(GameMode m) { mode = m; } // Selecciona modo (se aplica al hacer init()).
        void toggleViewMode();
        void cycleCamera3DType();
        bool is3DViewEnabled() const { return viewMode == ViewMode::Mode3D; }

        // Init
        void init(); // Carga recursos y crea entidades iniciales.

        // Loop
        void processInput(); // Lee teclas y aplica acciones (movimiento/bombas).
        void update();       // Tick de lógica (IA, bombas, colisiones).
        void render();       // Renderiza el frame.

        // Alternar entre pantalla completa y ventana
        void toggleFullscreen(GLFWwindow* window);
};



#endif //bomberman_game
