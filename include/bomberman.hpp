#ifndef bomberman_game
#define bomberman_game

#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <map>
#include <string>
#include <vector>

#include "menu.hpp"
#include "cinematic_player.hpp"

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
    GAME_INTRO,      // Pantalla de intro
    GAME_MENU,       // Pantalla selección de modo de juego
    GAME_CINEMATIC,  // Cinematicas (video FFmpeg)
    GAME_PLAYING
};

enum class CinematicType {
    Intro,              // Intro al abrir el juego -> GAME_MENU
    HistoryStart,       // Introduccion modo historia -> GAME_PLAYING (Historia 1 o 2 jugadores)
    HistoryEnd          // Final modo historia -> GAME_MENU (Historia 1 o 2 jugadores)
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
    void render2D();
    void render3D();
    void refreshWindowTitle() const;

    // Carga y reutilización de recursos.
    void ensureRenderResources();
    void ensureGameplayAssets();
    void cleanupGameplayEntities();

    // Helpers de progresión.
    bool allPlayersOutOfLives() const;
    bool allEnemiesCleared() const;

    // Cámara 3D avanzada (estado interno de control/cursor).
    float cameraOrbitYaw = 0.0f;
    bool cameraOrbitDragging = false;
    double cameraOrbitLastMouseX = 0.0;

    float firstPersonYaw = 0.0f;
    float firstPersonPitch = -0.18f;
    bool firstPersonMouseInitialized = false;
    double firstPersonLastMouseX = 0.0;
    double firstPersonLastMouseY = 0.0;
    bool firstPersonCursorLocked = false;

    // Estado de recursos y progreso de partida.
    bool renderResourcesInitialized = false;
    bool gameplayAssetsLoaded = false;

    int currentLevelIndex = 0;
    bool currentLevelHadEnemies = false;
    std::vector<int> playerScores;
    std::vector<std::string> levelSequence = {
        "levels/level_01.txt",
        "levels/level_02.txt",
        "levels/level_03.txt",
        "levels/level_04.txt",
        "levels/level_05.txt"
    };

    bool pendingLevelAdvance = false;
    float levelAdvanceTimer = 0.0f;
    float levelAdvanceDelaySeconds = 1.0f;

    // Progresión de niveles (uso interno).
    void loadLevel(int levelIndex, bool preserveLivesAndScore);
    void startNewRun(GameMode newMode);
    void advanceToNextLevel();
    void returnToMenuFromGame(bool resetRun);

public:
    // Input
    std::map<GLint, GLint> keys;           // Estado: Release(0), Press(1), Repeat(2)
    GLint lastDirKey = GLFW_KEY_UNKNOWN;   // Última flecha pulsada (P1)
    GLint lastDirKeyP2 = GLFW_KEY_UNKNOWN; // Última WASD pulsada (P2)

    // Estado
    GameState state;
    GameMode mode = GameMode::OnePlayer;
    ViewMode viewMode = ViewMode::Mode2D;
    Camera3DType camera3DType = Camera3DType::PerspectiveFixed;

        // UI Screen
        MenuScreen menuScreen;              // Gestiona menú
        CinematicPlayer cinematicPlayer;    // Reproductor de cinematicas (FFmpeg)
        CinematicType currentCinematicType = CinematicType::Intro;
        GameState nextStateAfterCinematic = GAME_INTRO;

        // Ventana
        GLint WIDTH, HEIGHT;                 // Tamaño ventana
        int windowedXPos = 100;
        int windowedYPos = 100;             // Posición de la ventana en modo windowed (para restaurar al salir de fullscreen)  
        GLFWwindow* window;                  // GLFW window

    // Timing
    float deltaTime = 0.0f;

    Game(GLFWwindow* window, GLuint width, GLuint height)
        : state(GAME_INTRO), WIDTH(width), HEIGHT(height), window(window) {}
    ~Game();

    void setMode(GameMode m) { mode = m; }
    void toggleViewMode();
    void cycleCamera3DType();
    bool is3DViewEnabled() const { return viewMode == ViewMode::Mode3D; }
    float getCameraOrbitYaw() const { return cameraOrbitYaw; }
    float getFirstPersonYaw() const { return firstPersonYaw; }

    // Init / loop
    void init();
    void processInput();
    void update();
    void render();

    // Ventana
    void toggleFullscreen(GLFWwindow* window);
    void onResize(int width, int height);
};



#endif //bomberman_game
