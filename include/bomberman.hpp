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
#include "gamepad_input.hpp"
#include "in_game_menu.hpp"
#include "custom_game_menu.hpp"
#include "custom_game_mode.hpp"

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
    GAME_CUSTOM_MENU_1, // Configuración custom game (pantalla 1)
    GAME_CUSTOM_MENU_2, // Configuración custom game (pantalla 2)
    GAME_CINEMATIC,  // Cinematicas (video FFmpeg)
    GAME_RANKING,    // Pantalla de ranking tras Game Over
    GAME_PLAYING
};

enum class CinematicType {
    Intro,              // Intro al abrir el juego -> GAME_MENU
    HistoryStart,       // Introduccion modo historia -> GAME_PLAYING (Historia 1 o 2 jugadores)
    HistoryEnd,         // Final modo historia -> GAME_MENU (Historia 1 o 2 jugadores)
    LevelStart,         // Pantalla de inicio de cada nivel -> GAME_PLAYING (nivel siguiente)
    VsVictoryP1,        // Victoria en VS del jugador 1
    VsVictoryP2,        // Victoria en VS del jugador 2
    VsDraw,             // Empate en VS
    VsDefeat            // Derrota en VS
};

enum class GameMode {
    VsOnePlayer,
    VsTwoPlayers,
    HistoryOnePlayer,
    HistoryTwoPlayers,
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
    FirstPerson,
    FreeCamera
};

static std::vector<std::string> mapNumeration = {
    "1-2", "3-2", "3-6", "4-2", "6-6"
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
    void startVsRoundCinematic(CinematicType type, const std::string& videoPath, int winnerIndex);
    void renderVsVictoryStatsOverlay();
    void startContinueSequence();
    void updateContinueSequence(float deltaTime);
    void enterRankingScreen();
    void renderContinueOverlay(float aspect);

    // Secuencia TIME UP (se agotó el tiempo sin eliminar enemigos).
    void startTimeUpSequence();
    void updateTimeUpSequence(float deltaTime);
    void renderTimeUpOverlay(float aspect);

    // Cámara 3D avanzada (estado interno de control/cursor).
    float cameraOrbitYaw = 0.0f;
    float cameraOrbitPitch = -0.18f;
    float cameraOrbitDistance = 0.0f;
    float cameraFollowDistance = 6.8f;
    bool cameraOrbitDragging = false;
    double cameraOrbitLastMouseX = 0.0;
    double cameraOrbitLastMouseY = 0.0;

    float firstPersonYaw = 0.0f;
    float firstPersonPitch = -0.18f;
    bool firstPersonMouseInitialized = false;
    double firstPersonLastMouseX = 0.0;
    double firstPersonLastMouseY = 0.0;
    bool firstPersonCursorLocked = false;
    bool firstPersonMouseLeftPressedLastFrame = false;
    bool firstPersonMouseRightPressedLastFrame = false;
    int active3DViewportPlayerIndex = 0;

    // Cámara libre 3D: movimiento desacoplado del jugador.
    float freeCameraPosX = 0.0f;
    float freeCameraPosY = 7.5f;
    float freeCameraPosZ = 10.0f;
    float freeCameraYaw = -0.60f;
    float freeCameraPitch = -0.28f;
    float freeCameraRoll = 0.0f;
    bool freeCameraInitialized = false;
    bool freeCameraAnchored = false;

    void resetFreeCameraPose();

    // Fondo 3D sorpresa: se revela con doble pulsación de la tecla 9 en modo 3D.
    bool surpriseHorizonVisible3D = false;
    int surpriseKey3TapCount = 0;
    double surpriseKey3LastTapTime = -10.0;

    // Estado de recursos y progreso de partida.
    bool renderResourcesInitialized = false;
    bool gameplayAssetsLoaded = false;

    int currentLevelIndex = 0;
    int versusRoundNumber = 1;
    bool currentLevelHadEnemies = false;

    // VS: resultado de cinemática y acción a ejecutar al terminar.
    int vsCinematicWinnerIndex = -1;
    enum class VsCinematicPostAction {
        None,
        RestartCurrentLevel,
        AdvanceNextLevel,
        ReturnToMenu
    };
    VsCinematicPostAction vsCinematicPostAction = VsCinematicPostAction::None;
    bool vsCinematicSkipRequested = false;

    // VS: overlay de victoria (texto naranja) — valores en proporcion a la resolucion de referencia 1920x1080.
    float vsVictoryOverlayRightXRatio  = 1560.0f / 1920.0f;
    float vsVictoryOverlayTopYRatio    = 361.0f  / 1080.0f;
    float vsVictoryOverlayLineGapRatio = 108.0f  / 1080.0f;
    float vsVictoryOverlayGlyphWRatio  = 60.0f   / 1920.0f;
    float vsVictoryOverlayGlyphHRatio  = 67.0f   / 1080.0f;
    float vsVictoryOverlaySpacingRatio = 8.0f    / 1920.0f;
    float vsVictoryOverlaySpaceWidthFactor = 0.60f;

    // VS: tiempo vivo acumulado (segundos) para la partida actual.
    float vsAliveSeconds = 0.0f;

public:
    std::vector<int> playerScores;

private:
    std::vector<std::string> levelSequence = {
        "levels/level_01.txt",
        "levels/level_02.txt",
        "levels/level_03.txt",
        "levels/level_04.txt",
        "levels/level_05.txt"
    };
    std::vector<int> levelToStage = {
        1, 2, 2, 3, 4
    };
    std::vector<std::string> levelCinematicSequence = {
        "resources/video/levels/level_01.mp4",
        "resources/video/levels/level_02.mp4",
        "resources/video/levels/level_03.mp4",
        "resources/video/levels/level_04.mp4",
        "resources/video/levels/level_05.mp4"
    };

    bool pendingLevelAdvance = false;
    float levelAdvanceTimer = 0.0f;
    float levelAdvanceDelaySeconds = 1.0f;

    // Secuencia de fin de partida (CONTINUE -> GAME OVER -> RANKING).
    bool continueSequenceActive = false;
    bool continueShowingGameOver = false;
    float continueTimerSeconds = 0.0f;
    float continueProgress01 = 0.0f;
    int continueCountdownValue = 9;

    // Secuencia TIME UP.
    bool timeUpSequenceActive = false;
    float timeUpTimer = 0.0f;
    static constexpr float kTimeUpAnimDuration = 4.5f; // segundos hasta matar jugadores

    // Ranking tras Game Over.
    float rankingScreenTimer = 0.0f;
    float rankingAutoExitSeconds = 5.0f;
    bool isEnteringRankingName = false;
    int rankingEntryIndex = -1;
    std::string rankingPlayerName = "";
    int rankingCurrentVocabIndex = 0;
    int rankingPlayerOwner = 1;
    float rankingInputTimer = 0.0f;
    bool isRankingVs = false;

    // Cinemáticas de nivel: variables para rastrear transición a cinemática antes de cargar nivel.
    bool loadLevelPending = false;  // Flag para saber si después de la cinemática debe cargar un nivel
    bool pendingLoadPreserveLivesAndScore = false;
    float introCinematicElapsedSeconds = 0.0f;
    bool introExplosionPlayed = false;
    static constexpr float kIntroExplosionTriggerSeconds = 3.5f;
    bool fastMusicActive = false;

    // Progresión de niveles (uso interno).
    void loadLevel(int levelIndex, bool preserveLivesAndScore);
    void startNewRun(GameMode newMode);
    void advanceToNextLevel();
    void returnToMenuFromGame(bool resetRun);

public:

    // // Global variables for OpenGL
    // GLuint VAO, VBO, EBO, shader, uniformModel, uniformProjection, uniformTexture, uniformTintColor, uniformUvRect, uniformFlipX, uniformWhiteFlash;

    // GLuint texture;
    // Input
    std::map<GLint, GLint> keys;           // Estado: Release(0), Press(1), Repeat(2)
    GLint lastKeyPressed;
    GLint lastDirKey = GLFW_KEY_UNKNOWN;   // Última flecha pulsada (P1)
    GLint lastDirKeyP2 = GLFW_KEY_UNKNOWN; // Última WASD pulsada (P2)

    // Estado
    GameState state;
    GameMode mode = GameMode::HistoryOnePlayer;
    ViewMode viewMode = ViewMode::Mode2D;
    Camera3DType camera3DType = Camera3DType::PerspectiveFixed;

        // UI Screen
        MenuScreen menuScreen;              // Gestiona menú
        CustomGameMenu customGameMenu;      // Menús de partida personalizada
        CustomGameMode customGameMode;      // Runtime de partida personalizada
        CinematicPlayer cinematicPlayer;    // Reproductor de cinematicas (FFmpeg)
        CinematicType currentCinematicType = CinematicType::Intro;
        GameState nextStateAfterCinematic = GAME_INTRO;

        // In-game menu
        InGameMenu inGameMenu;

        // Ventana
        GLint WIDTH, HEIGHT;                 // Tamaño ventana (lógico: mantiene 16:9)
        int actualWindowWidth = 1920;       // Ancho real del framebuffer (puede variar en windowed)
        int actualWindowHeight = 1080;      // Alto real del framebuffer (puede variar en windowed)
        int windowedXPos = 100;
        int windowedYPos = 100;             // Posición de la ventana en modo windowed (para restaurar al salir de fullscreen)  
        GLFWwindow* window;                  // GLFW window

    // Timing
    float deltaTime = 0.0f;
    float levelTimeRemaining = 121.0f;
    std::string currentGameLevel = "5-5";
    std::string currentLevelVS = "2";
    

    Game(GLFWwindow* window, GLuint width, GLuint height)
        : state(GAME_INTRO), WIDTH(width), HEIGHT(height), actualWindowWidth(width), actualWindowHeight(height), window(window) {}
    ~Game();

    void setMode(GameMode m) { mode = m; }
    void toggleViewMode();
    void cycleCamera3DType();
    void setCamera3DType(Camera3DType newType);
    void onMouseScroll(double yOffset);
    bool is3DViewEnabled() const { return viewMode == ViewMode::Mode3D; }
    float getCameraOrbitYaw() const { return cameraOrbitYaw; }
    float getFirstPersonYaw() const { return firstPersonYaw; }
    float getFreeCameraYaw() const { return freeCameraYaw; }

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
