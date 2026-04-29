#ifndef CUSTOM_GAME_MENU_HPP
#define CUSTOM_GAME_MENU_HPP

#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <array>
#include <map>
#include <string>

#include "controls_menu.hpp"
#include "cpu_bomberman_difficulty.hpp"
#include "sprite_atlas.hpp"

enum class CustomPlayersOption {
    OnePlayer,
    TwoPlayers,
    OnePlayerPlusCpu
};

enum class CustomTeamModeOption {
    Versus,
    Cooperative
};

enum class CustomTimeLimitOption {
    OneMinute,
    TwoMinutes,
    ThreeMinutes,
    Infinite
};

struct CustomGameSettings {
    CustomPlayersOption players = CustomPlayersOption::OnePlayer;
    CustomTeamModeOption teamMode = CustomTeamModeOption::Versus;
    CustomTimeLimitOption timeLimit = CustomTimeLimitOption::ThreeMinutes;
    int mapIndex = 0;

    CpuBomberman::Difficulty allyCpuDifficulty = CpuBomberman::Difficulty::Medium;
    std::array<CpuBomberman::Difficulty, 3> enemyBombermanDifficulties = {
        CpuBomberman::Difficulty::Easy,
        CpuBomberman::Difficulty::Medium,
        CpuBomberman::Difficulty::Hard
    };
};

class CustomGameMenu {
private:
    // Fondos y atlas.
    GLuint menu1BackgroundTexture;
    GLuint menu2BackgroundTexture;
    GLuint menu1AtlasTexture;
    GLuint menuArrowTexture;
    GLuint vocabTexture;
    GLuint solidWhiteTexture;
    SpriteAtlas menu1Atlas;
    SpriteAtlas menuBombAtlas;
    SpriteAtlas vocabAtlas;

    // Pantalla 1 (configuracion general).
    int menu1RowSelection; // 0 players, 1 mode, 2 time, 3 map, 4 next
    CustomGameSettings settings;
    float menuArrowAnimTimer;
    float menuArrowAnimSpeed;
    bool menuArrowSelected;
    float menuArrowSelectedAnimSpeed;
    float menuSelectedWaitTimer;
    float menuSelectedWaitTime;

    // Pantalla 2 (animación del botón Play).
    bool menu2PlaySelected;
    float menu2ArrowAnimTimer;
    float menu2SelectedWaitTimer;

    // Pantalla 2 (configuracion enemigos).
    int enemyTypeSelection;
    std::array<int, 9> enemyCounts;

    // Flujo entre pantallas/estados.
    bool shouldGoToMenu2;
    bool shouldReturnToMainMenu;
    bool shouldStartCustomGame;
    bool shouldReturnToMenu1;

    int getEnemyTotalCount() const;

    bool drawAtlasSprite(const std::string& spriteName,
                         float centerXpx,
                         float centerYpx,
                         float forcedWidthPx,
                         float forcedHeightPx,
                         float aspect,
                         GLuint uniformModel,
                         GLuint uniformUvRect,
                         GLuint uniformTintColor,
                         GLuint uniformFlipX,
                         const glm::vec4& tintColor);

    void drawSolidQuad(float centerXpx,
                       float centerYpx,
                       float widthPx,
                       float heightPx,
                       float aspect,
                       GLuint uniformModel,
                       GLuint uniformUvRect,
                       GLuint uniformTintColor,
                       GLuint uniformFlipX,
                       const glm::vec4& tintColor);

    void drawSelectionFrame(float centerXpx,
                            float centerYpx,
                            float widthPx,
                            float heightPx,
                            float thicknessPx,
                            float aspect,
                            GLuint uniformModel,
                            GLuint uniformUvRect,
                            GLuint uniformTintColor,
                            GLuint uniformFlipX,
                            const glm::vec4& color);

    bool drawVocabGlyph(char glyph,
                        bool yellow,
                        float centerXpx,
                        float centerYpx,
                        float glyphSizePx,
                        float aspect,
                        GLuint uniformModel,
                        GLuint uniformUvRect,
                        GLuint uniformTintColor,
                        GLuint uniformFlipX);

    void drawVocabTextCentered(const std::string& text,
                               bool yellow,
                               float centerXpx,
                               float centerYpx,
                               float glyphSizePx,
                               float spacingPx,
                               float aspect,
                               GLuint uniformModel,
                               GLuint uniformUvRect,
                               GLuint uniformTintColor,
                               GLuint uniformFlipX);

public:
    CustomGameMenu();
    ~CustomGameMenu();

    void resetToDefaults();
    void resetFlowFlags();

    void initMenu1();
    void initMenu2();

    void processInputMenu1(std::map<int, int>& keys, ControlsMenu& controls);
    void processInputMenu2(std::map<int, int>& keys, ControlsMenu& controls);

    void updateMenu1(float deltaTime);
    void updateMenu2(float deltaTime);

    void renderMenu1(GLuint VAO,
                     GLuint shader,
                     GLuint uniformModel,
                     GLuint uniformProjection,
                     GLuint uniformTexture,
                     GLuint uniformUvRect,
                     GLuint uniformTintColor,
                     GLuint uniformFlipX,
                     int WIDTH,
                     int HEIGHT);

    void renderMenu2(GLuint VAO,
                     GLuint shader,
                     GLuint uniformModel,
                     GLuint uniformProjection,
                     GLuint uniformTexture,
                     GLuint uniformUvRect,
                     GLuint uniformTintColor,
                     GLuint uniformFlipX,
                     int WIDTH,
                     int HEIGHT);

    bool shouldAdvanceToMenu2() const { return shouldGoToMenu2; }
    bool shouldBackToMainMenu() const { return shouldReturnToMainMenu; }
    bool shouldLaunchCustomGame() const { return shouldStartCustomGame; }
    bool shouldBackToMenu1() const { return shouldReturnToMenu1; }

    const CustomGameSettings& getSettings() const { return settings; }
    const std::array<int, 9>& getEnemyCounts() const { return enemyCounts; }
};

#endif // CUSTOM_GAME_MENU_HPP
