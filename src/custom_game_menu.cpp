#include "custom_game_menu.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cctype>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

// Reutilizamos utilidades globales ya existentes en el proyecto.
extern std::string resolveAssetPath(const std::string& assetPath);
extern GLuint LoadTexture(const char* filePath);
extern bool getUvRectForSprite(const SpriteAtlas& atlas, const std::string& spriteName, glm::vec4& uvRect);

namespace {
constexpr float kCanvasWidth = 1920.0f;
constexpr float kCanvasHeight = 1080.0f;

constexpr int kMenu1Rows = 5;
constexpr int kEnemyTypeCount = 11;
constexpr int kEnemyTotalMax = 12;

const std::array<float, kMenu1Rows> kMenu1ArrowXPixels = {
    210.0f,
    210.0f,
    210.0f,
    210.0f,
    210.0f
};

const std::array<float, kMenu1Rows> kMenu1ArrowYPixels = {
    220.0f,
    365.0f,
    510.0f,
    660.0f,
    940.0f
};

// Equivalencia con menu.cpp: scale 0.1 sobre quad de alto 2.0 -> 0.2 de alto pantalla.
constexpr float kMenuBombSizePixels = 108.0f;

const std::array<glm::vec2, kEnemyTypeCount> kEnemyCounterCenters = {
    glm::vec2(370.0f, 340.0f),
    glm::vec2(830.0f, 340.0f),
    glm::vec2(1350.0f, 340.0f),
    glm::vec2(1700.0f, 340.0f),
    glm::vec2(375.0f, 555.0f),
    glm::vec2(835.0f, 555.0f),
    glm::vec2(1400.0f, 555.0f),
    glm::vec2(1700.0f, 555.0f),
    glm::vec2(380.0f, 765.0f),
    glm::vec2(840.0f, 765.0f),
    glm::vec2(1450.0f, 765.0f)
};

const std::array<std::string, 5> kMapSpriteNames = {
    "Stage1-2",
    "Stage3-2",
    "Stage3-6",
    "Stage4-2",
    "Stage6-6"
};

glm::vec2 pixelsToOrtho(float centerXpx, float centerYpx, float aspect) {
    const float xNdc = (centerXpx / kCanvasWidth) * 2.0f - 1.0f;
    const float yNdc = 1.0f - (centerYpx / kCanvasHeight) * 2.0f;
    return glm::vec2(xNdc * aspect, yNdc);
}

glm::vec2 sizePixelsToOrthoHalfExtents(float widthPx, float heightPx, float aspect) {
    const float halfWidth = (widthPx / kCanvasWidth) * aspect;
    const float halfHeight = (heightPx / kCanvasHeight);
    return glm::vec2(halfWidth, halfHeight);
}

GLuint createSolidWhiteTexture() {
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    const unsigned char whitePixel[4] = { 255, 255, 255, 255 };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return texture;
}
} // namespace

CustomGameMenu::CustomGameMenu()
    : menu1BackgroundTexture(0),
      menu2BackgroundTexture(0),
      menu1AtlasTexture(0),
      menuArrowTexture(0),
    vocabTexture(0),
      solidWhiteTexture(0),
      menu1RowSelection(0),
      menuArrowAnimTimer(0.0f),
      menuArrowAnimSpeed(0.1f),
      menuArrowSelected(false),
      menuArrowSelectedAnimSpeed(0.8f),
      menuSelectedWaitTimer(0.0f),
      menuSelectedWaitTime(1.5f),
    menu2PlaySelected(false),
    menu2ArrowAnimTimer(0.0f),
    menu2SelectedWaitTimer(0.0f),
      enemyTypeSelection(0),
      enemyCounts({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}),
      shouldGoToMenu2(false),
      shouldReturnToMainMenu(false),
      shouldStartCustomGame(false),
      shouldReturnToMenu1(false) {
    resetToDefaults();
}

CustomGameMenu::~CustomGameMenu() {
    if (menu1BackgroundTexture != 0) {
        glDeleteTextures(1, &menu1BackgroundTexture);
        menu1BackgroundTexture = 0;
    }
    if (menu2BackgroundTexture != 0) {
        glDeleteTextures(1, &menu2BackgroundTexture);
        menu2BackgroundTexture = 0;
    }
    if (menu1AtlasTexture != 0) {
        glDeleteTextures(1, &menu1AtlasTexture);
        menu1AtlasTexture = 0;
    }
    if (menuArrowTexture != 0) {
        glDeleteTextures(1, &menuArrowTexture);
        menuArrowTexture = 0;
    }
    if (vocabTexture != 0) {
        glDeleteTextures(1, &vocabTexture);
        vocabTexture = 0;
    }
    if (solidWhiteTexture != 0) {
        glDeleteTextures(1, &solidWhiteTexture);
        solidWhiteTexture = 0;
    }
}

void CustomGameMenu::resetToDefaults() {
    settings.players = CustomPlayersOption::OnePlayer;
    settings.teamMode = CustomTeamModeOption::Versus;
    settings.timeLimit = CustomTimeLimitOption::ThreeMinutes;
    settings.mapIndex = 0;

    menu1RowSelection = 0;
    menuArrowSelected = false;
    menuArrowAnimTimer = 0.0f;
    menuSelectedWaitTimer = 0.0f;
    menu2PlaySelected = false;
    menu2ArrowAnimTimer = 0.0f;
    menu2SelectedWaitTimer = 0.0f;
    enemyTypeSelection = 0;
    enemyCounts = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    resetFlowFlags();
}

void CustomGameMenu::resetFlowFlags() {
    shouldGoToMenu2 = false;
    shouldReturnToMainMenu = false;
    shouldStartCustomGame = false;
    shouldReturnToMenu1 = false;
}

void CustomGameMenu::initMenu1() {
    resetFlowFlags();

    if (menu1BackgroundTexture == 0) {
        menu1BackgroundTexture = LoadTexture(resolveAssetPath("resources/sprites/custom_game/Menu1CustomGame.jpg").c_str());
        if (menu1BackgroundTexture == 0) {
            std::cerr << "Error cargando Menu1CustomGame.jpg\n";
        }
    }

    if (menu1Atlas.sprites.empty()) {
        const std::string atlasPath = resolveAssetPath("resources/sprites/atlases/SpriteAtlasCustomGameMenu1.json");
        if (!loadSpriteAtlasMinimal(atlasPath, menu1Atlas)) {
            std::cerr << "Error cargando SpriteAtlasCustomGameMenu1.json\n";
        }
    }

    if (menu1AtlasTexture == 0 && !menu1Atlas.imagePath.empty()) {
        menu1AtlasTexture = LoadTexture(resolveAssetPath(menu1Atlas.imagePath).c_str());
        if (menu1AtlasTexture == 0) {
            std::cerr << "Error cargando AtlasMenu1.png\n";
        }
    }

    if (menuBombAtlas.sprites.empty()) {
        const std::string menuBombAtlasPath = resolveAssetPath("resources/sprites/atlases/SpriteAtlasMenuBomb.json");
        if (!loadSpriteAtlasMinimal(menuBombAtlasPath, menuBombAtlas)) {
            std::cerr << "Error cargando SpriteAtlasMenuBomb.json\n";
        }
    }

    if (menuArrowTexture == 0 && !menuBombAtlas.imagePath.empty()) {
        menuArrowTexture = LoadTexture(resolveAssetPath(menuBombAtlas.imagePath).c_str());
        if (menuArrowTexture == 0) {
            std::cerr << "Error cargando SpriteBombaAtlas.png\n";
        }
    }

    menuArrowSelected = false;
    menuArrowAnimTimer = 0.0f;
    menuSelectedWaitTimer = 0.0f;

    if (solidWhiteTexture == 0) {
        solidWhiteTexture = createSolidWhiteTexture();
    }
}

void CustomGameMenu::initMenu2() {
    resetFlowFlags();

    if (menu2BackgroundTexture == 0) {
        menu2BackgroundTexture = LoadTexture(resolveAssetPath("resources/sprites/custom_game/Menu2CustomGamev2.jpg").c_str());
        if (menu2BackgroundTexture == 0) {
            std::cerr << "Error cargando Menu2CustomGamev2.jpg\n";
        }
    }

    if (menuBombAtlas.sprites.empty()) {
        const std::string menuBombAtlasPath = resolveAssetPath("resources/sprites/atlases/SpriteAtlasMenuBomb.json");
        if (!loadSpriteAtlasMinimal(menuBombAtlasPath, menuBombAtlas)) {
            std::cerr << "Error cargando SpriteAtlasMenuBomb.json\n";
        }
    }

    if (menuArrowTexture == 0 && !menuBombAtlas.imagePath.empty()) {
        menuArrowTexture = LoadTexture(resolveAssetPath(menuBombAtlas.imagePath).c_str());
        if (menuArrowTexture == 0) {
            std::cerr << "Error cargando SpriteBombaAtlas.png\n";
        }
    }

    if (vocabAtlas.sprites.empty()) {
        const std::string vocabAtlasPath = resolveAssetPath("resources/sprites/atlases/SpriteAtlasVocAmarilloPeq.json");
        if (!loadSpriteAtlasMinimal(vocabAtlasPath, vocabAtlas)) {
            std::cerr << "Error cargando SpriteAtlasVocAmarilloPeq.json\n";
        }
    }

    if (vocabTexture == 0 && !vocabAtlas.imagePath.empty()) {
        vocabTexture = LoadTexture(resolveAssetPath(vocabAtlas.imagePath).c_str());
        if (vocabTexture == 0) {
            std::cerr << "Error cargando AtlasAmarillosPequenos.png\n";
        }
    }

    menu2PlaySelected = false;
    menu2ArrowAnimTimer = 0.0f;
    menu2SelectedWaitTimer = 0.0f;

    if (solidWhiteTexture == 0) {
        solidWhiteTexture = createSolidWhiteTexture();
    }
}

bool CustomGameMenu::drawAtlasSprite(const std::string& spriteName,
                                     float centerXpx,
                                     float centerYpx,
                                     float forcedWidthPx,
                                     float forcedHeightPx,
                                     float aspect,
                                     GLuint uniformModel,
                                     GLuint uniformUvRect,
                                     GLuint uniformTintColor,
                                     GLuint uniformFlipX,
                                     const glm::vec4& tintColor) {
    if (menu1AtlasTexture == 0) {
        return false;
    }

    glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
    if (!getUvRectForSprite(menu1Atlas, spriteName, uvRect)) {
        return false;
    }

    float widthPx = forcedWidthPx;
    float heightPx = forcedHeightPx;
    auto it = menu1Atlas.sprites.find(spriteName);
    if (it != menu1Atlas.sprites.end()) {
        if (widthPx <= 0.0f) widthPx = static_cast<float>(it->second.w);
        if (heightPx <= 0.0f) heightPx = static_cast<float>(it->second.h);
    }
    if (widthPx <= 0.0f) widthPx = 100.0f;
    if (heightPx <= 0.0f) heightPx = 50.0f;

    const glm::vec2 center = pixelsToOrtho(centerXpx, centerYpx, aspect);
    const glm::vec2 halfExtents = sizePixelsToOrthoHalfExtents(widthPx, heightPx, aspect);

    glm::mat4 model(1.0f);
    model = glm::translate(model, glm::vec3(center.x, center.y, 0.0f));
    model = glm::scale(model, glm::vec3(halfExtents.x, halfExtents.y, 1.0f));

    glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
    glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));
    glUniform4fv(uniformTintColor, 1, glm::value_ptr(tintColor));
    glUniform1f(uniformFlipX, 0.0f);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, menu1AtlasTexture);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    return true;
}

void CustomGameMenu::drawSolidQuad(float centerXpx,
                                   float centerYpx,
                                   float widthPx,
                                   float heightPx,
                                   float aspect,
                                   GLuint uniformModel,
                                   GLuint uniformUvRect,
                                   GLuint uniformTintColor,
                                   GLuint uniformFlipX,
                                   const glm::vec4& tintColor) {
    if (solidWhiteTexture == 0) {
        return;
    }

    const glm::vec2 center = pixelsToOrtho(centerXpx, centerYpx, aspect);
    const glm::vec2 halfExtents = sizePixelsToOrthoHalfExtents(widthPx, heightPx, aspect);

    glm::mat4 model(1.0f);
    model = glm::translate(model, glm::vec3(center.x, center.y, 0.0f));
    model = glm::scale(model, glm::vec3(halfExtents.x, halfExtents.y, 1.0f));

    const glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
    glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
    glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));
    glUniform4fv(uniformTintColor, 1, glm::value_ptr(tintColor));
    glUniform1f(uniformFlipX, 0.0f);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, solidWhiteTexture);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void CustomGameMenu::drawSelectionFrame(float centerXpx,
                                        float centerYpx,
                                        float widthPx,
                                        float heightPx,
                                        float thicknessPx,
                                        float aspect,
                                        GLuint uniformModel,
                                        GLuint uniformUvRect,
                                        GLuint uniformTintColor,
                                        GLuint uniformFlipX,
                                        const glm::vec4& color) {
    const float halfW = widthPx * 0.5f;
    const float halfH = heightPx * 0.5f;

    drawSolidQuad(centerXpx, centerYpx - halfH + thicknessPx * 0.5f,
                  widthPx, thicknessPx, aspect,
                  uniformModel, uniformUvRect, uniformTintColor, uniformFlipX, color);
    drawSolidQuad(centerXpx, centerYpx + halfH - thicknessPx * 0.5f,
                  widthPx, thicknessPx, aspect,
                  uniformModel, uniformUvRect, uniformTintColor, uniformFlipX, color);
    drawSolidQuad(centerXpx - halfW + thicknessPx * 0.5f, centerYpx,
                  thicknessPx, heightPx, aspect,
                  uniformModel, uniformUvRect, uniformTintColor, uniformFlipX, color);
    drawSolidQuad(centerXpx + halfW - thicknessPx * 0.5f, centerYpx,
                  thicknessPx, heightPx, aspect,
                  uniformModel, uniformUvRect, uniformTintColor, uniformFlipX, color);
}

int CustomGameMenu::getEnemyTotalCount() const {
    return std::accumulate(enemyCounts.begin(), enemyCounts.end(), 0);
}

bool CustomGameMenu::drawVocabGlyph(char glyph,
                                    bool yellow,
                                    float centerXpx,
                                    float centerYpx,
                                    float glyphSizePx,
                                    float aspect,
                                    GLuint uniformModel,
                                    GLuint uniformUvRect,
                                    GLuint uniformTintColor,
                                    GLuint uniformFlipX) {
    if (vocabTexture == 0 || vocabAtlas.sprites.empty()) {
        return false;
    }

    const char upperGlyph = static_cast<char>(std::toupper(static_cast<unsigned char>(glyph)));
    std::string spriteName;

    if (upperGlyph >= '0' && upperGlyph <= '9') {
        if (yellow) {
            spriteName = std::string(1, upperGlyph) + "_Ama";
        } else {
            spriteName = std::string(1, upperGlyph) + "_Bla";
            if (vocabAtlas.sprites.find(spriteName) == vocabAtlas.sprites.end()) {
                spriteName = std::string(1, upperGlyph) + "_AmaBla";
                if (vocabAtlas.sprites.find(spriteName) == vocabAtlas.sprites.end()) {
                    spriteName = std::string(1, upperGlyph) + "_Ama";
                }
            }
        }
    } else if (upperGlyph >= 'A' && upperGlyph <= 'Z') {
        if (yellow) {
            spriteName = std::string(1, upperGlyph) + "_Ama";
        } else {
            spriteName = std::string(1, upperGlyph) + "_Bla";
            if (vocabAtlas.sprites.find(spriteName) == vocabAtlas.sprites.end()) {
                spriteName = std::string(1, upperGlyph) + "_AmaBla";
            }
            if (vocabAtlas.sprites.find(spriteName) == vocabAtlas.sprites.end()) {
                spriteName = std::string(1, upperGlyph) + "_Ama";
            }
        }
    } else if (upperGlyph == '-') {
        spriteName = yellow ? "-_Ama" : "-_Bla";
        if (vocabAtlas.sprites.find(spriteName) == vocabAtlas.sprites.end()) {
            spriteName = "-_Ama";
        }
    } else if (upperGlyph == '/') {
        spriteName = yellow ? "/_Ama" : "/_Bla";
        if (vocabAtlas.sprites.find(spriteName) == vocabAtlas.sprites.end()) {
            spriteName = "/_Bla";
        }
    } else {
        return false;
    }

    glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
    if (!getUvRectForSprite(vocabAtlas, spriteName, uvRect)) {
        return false;
    }

    float glyphWidthPx = glyphSizePx;
    auto spriteIt = vocabAtlas.sprites.find(spriteName);
    if (spriteIt != vocabAtlas.sprites.end() && spriteIt->second.h > 0) {
        const float ratio = static_cast<float>(spriteIt->second.w) / static_cast<float>(spriteIt->second.h);
        glyphWidthPx = glyphSizePx * ratio;
    }

    const glm::vec2 center = pixelsToOrtho(centerXpx, centerYpx, aspect);
    const glm::vec2 halfExtents = sizePixelsToOrthoHalfExtents(glyphWidthPx, glyphSizePx, aspect);

    glm::mat4 model(1.0f);
    model = glm::translate(model, glm::vec3(center.x, center.y, 0.0f));
    model = glm::scale(model, glm::vec3(halfExtents.x, halfExtents.y, 1.0f));

    glm::vec4 tint(1.0f, 1.0f, 1.0f, 1.0f);
    if (!yellow && spriteName.size() >= 4 && spriteName.substr(spriteName.size() - 4) == "_Ama") {
        // Fallback: si no hay variante blanca, aclaramos la variante amarilla.
        tint = glm::vec4(0.88f, 0.88f, 0.88f, 1.0f);
    }

    glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
    glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));
    glUniform4fv(uniformTintColor, 1, glm::value_ptr(tint));
    glUniform1f(uniformFlipX, 0.0f);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, vocabTexture);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    return true;
}

void CustomGameMenu::drawVocabTextCentered(const std::string& text,
                                           bool yellow,
                                           float centerXpx,
                                           float centerYpx,
                                           float glyphSizePx,
                                           float spacingPx,
                                           float aspect,
                                           GLuint uniformModel,
                                           GLuint uniformUvRect,
                                           GLuint uniformTintColor,
                                           GLuint uniformFlipX) {
    if (text.empty()) {
        return;
    }

    std::vector<float> glyphWidths;
    glyphWidths.reserve(text.size());

    float totalWidthPx = 0.0f;
    for (size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        const float width = (c == ' ') ? glyphSizePx * 0.60f : glyphSizePx;
        glyphWidths.push_back(width);
        totalWidthPx += width;
        if (i + 1 < text.size()) {
            totalWidthPx += spacingPx;
        }
    }

    float currentCenterX = centerXpx - (totalWidthPx * 0.5f) + (glyphWidths.front() * 0.5f);
    for (size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c != ' ') {
            drawVocabGlyph(c,
                           yellow,
                           currentCenterX,
                           centerYpx,
                           glyphSizePx,
                           aspect,
                           uniformModel,
                           uniformUvRect,
                           uniformTintColor,
                           uniformFlipX);
        }

        if (i + 1 < text.size()) {
            currentCenterX += glyphWidths[i] * 0.5f + spacingPx + glyphWidths[i + 1] * 0.5f;
        }
    }
}

void CustomGameMenu::processInputMenu1(std::map<int, int>& keys, ControlsMenu& controls) {
    if (menuArrowSelected) {
        if (keys[GLFW_KEY_ESCAPE] == GLFW_PRESS) {
            shouldReturnToMainMenu = true;
            keys[GLFW_KEY_ESCAPE] = GLFW_REPEAT;
        }
        return;
    }

    if (keys[controls.upKey_P1] == GLFW_PRESS) {
        menu1RowSelection = (menu1RowSelection - 1 + kMenu1Rows) % kMenu1Rows;
        keys[controls.upKey_P1] = GLFW_REPEAT;
    }

    if (keys[controls.downKey_P1] == GLFW_PRESS) {
        menu1RowSelection = (menu1RowSelection + 1) % kMenu1Rows;
        keys[controls.downKey_P1] = GLFW_REPEAT;
    }

    if (keys[controls.selectKey] == GLFW_PRESS) {
        switch (menu1RowSelection) {
            case 0: {
                if (settings.players == CustomPlayersOption::OnePlayer) {
                    settings.players = CustomPlayersOption::TwoPlayers;
                } else if (settings.players == CustomPlayersOption::TwoPlayers) {
                    settings.players = CustomPlayersOption::OnePlayerPlusCpu;
                } else {
                    settings.players = CustomPlayersOption::OnePlayer;
                }

                if (settings.players == CustomPlayersOption::OnePlayer) {
                    settings.teamMode = CustomTeamModeOption::Versus;
                }
                break;
            }
            case 1: {
                if (settings.players != CustomPlayersOption::OnePlayer) {
                    settings.teamMode = (settings.teamMode == CustomTeamModeOption::Versus)
                        ? CustomTeamModeOption::Cooperative
                        : CustomTeamModeOption::Versus;
                }
                break;
            }
            case 2: {
                if (settings.timeLimit == CustomTimeLimitOption::OneMinute) {
                    settings.timeLimit = CustomTimeLimitOption::TwoMinutes;
                } else if (settings.timeLimit == CustomTimeLimitOption::TwoMinutes) {
                    settings.timeLimit = CustomTimeLimitOption::ThreeMinutes;
                } else if (settings.timeLimit == CustomTimeLimitOption::ThreeMinutes) {
                    settings.timeLimit = CustomTimeLimitOption::Infinite;
                } else {
                    settings.timeLimit = CustomTimeLimitOption::OneMinute;
                }
                break;
            }
            case 3: {
                settings.mapIndex = (settings.mapIndex + 1) % static_cast<int>(kMapSpriteNames.size());
                break;
            }
            case 4:
                menuArrowSelected = true;
                menuArrowAnimTimer = 0.0f;
                menuSelectedWaitTimer = 0.0f;
                break;
            default:
                break;
        }

        keys[controls.selectKey] = GLFW_REPEAT;
    }

    if (keys[GLFW_KEY_ESCAPE] == GLFW_PRESS) {
        shouldReturnToMainMenu = true;
        keys[GLFW_KEY_ESCAPE] = GLFW_REPEAT;
    }
}

void CustomGameMenu::processInputMenu2(std::map<int, int>& keys, ControlsMenu& controls) {
    if (menu2PlaySelected) {
        if (keys[GLFW_KEY_ESCAPE] == GLFW_PRESS) {
            shouldReturnToMenu1 = true;
            keys[GLFW_KEY_ESCAPE] = GLFW_REPEAT;
        }
        return;
    }

    if (keys[GLFW_KEY_LEFT] == GLFW_PRESS) {
        enemyTypeSelection = (enemyTypeSelection - 1 + kEnemyTypeCount) % kEnemyTypeCount;
        keys[GLFW_KEY_LEFT] = GLFW_REPEAT;
    }

    if (keys[GLFW_KEY_RIGHT] == GLFW_PRESS) {
        enemyTypeSelection = (enemyTypeSelection + 1) % kEnemyTypeCount;
        keys[GLFW_KEY_RIGHT] = GLFW_REPEAT;
    }

    if (keys[GLFW_KEY_UP] == GLFW_PRESS) {
        const int totalNow = getEnemyTotalCount();
        if (totalNow < kEnemyTotalMax) {
            enemyCounts[enemyTypeSelection] += 1;
        }
        keys[GLFW_KEY_UP] = GLFW_REPEAT;
    }

    if (keys[GLFW_KEY_DOWN] == GLFW_PRESS) {
        enemyCounts[enemyTypeSelection] = std::max(0, enemyCounts[enemyTypeSelection] - 1);
        keys[GLFW_KEY_DOWN] = GLFW_REPEAT;
    }

    if (keys[controls.selectKey] == GLFW_PRESS) {
        if (getEnemyTotalCount() > 0) {
            menu2PlaySelected = true;
            menu2ArrowAnimTimer = 0.0f;
            menu2SelectedWaitTimer = 0.0f;
        }
        keys[controls.selectKey] = GLFW_REPEAT;
    }

    if (keys[GLFW_KEY_ESCAPE] == GLFW_PRESS) {
        //shouldReturnToMainMenu = true;
        shouldReturnToMenu1 = true;
        keys[GLFW_KEY_ESCAPE] = GLFW_REPEAT;
    }
}

void CustomGameMenu::updateMenu1(float deltaTime) {
    if (menuArrowSelected) {
        menuArrowAnimTimer += deltaTime;
        if (menuArrowAnimTimer >= menuArrowSelectedAnimSpeed) {
            menuArrowAnimTimer = 0.0f;
        }

        menuSelectedWaitTimer += deltaTime;
        if (menuSelectedWaitTimer >= menuSelectedWaitTime) {
            shouldGoToMenu2 = true;
        }
        return;
    }

    menuArrowAnimTimer += deltaTime;
    if (menuArrowAnimTimer >= menuArrowAnimSpeed) {
        menuArrowAnimTimer = 0.0f;
    }
}

void CustomGameMenu::updateMenu2(float deltaTime) {
    if (menu2PlaySelected) {
        menu2ArrowAnimTimer += deltaTime;
        if (menu2ArrowAnimTimer >= menuArrowSelectedAnimSpeed) {
            menu2ArrowAnimTimer = 0.0f;
        }

        menu2SelectedWaitTimer += deltaTime;
        if (menu2SelectedWaitTimer >= menuSelectedWaitTime) {
            shouldStartCustomGame = true;
        }
        return;
    }

    menu2ArrowAnimTimer += deltaTime;
    if (menu2ArrowAnimTimer >= menuArrowAnimSpeed) {
        menu2ArrowAnimTimer = 0.0f;
    }
}

void CustomGameMenu::renderMenu1(GLuint VAO,
                                 GLuint shader,
                                 GLuint uniformModel,
                                 GLuint uniformProjection,
                                 GLuint uniformTexture,
                                 GLuint uniformUvRect,
                                 GLuint uniformTintColor,
                                 GLuint uniformFlipX,
                                 int WIDTH,
                                 int HEIGHT) {
    glUseProgram(shader);
    glBindVertexArray(VAO);

    const float safeHeight = std::max(1, HEIGHT);
    const float aspect = static_cast<float>(WIDTH) / static_cast<float>(safeHeight);
    const glm::mat4 projection = glm::ortho(-aspect, aspect, -1.0f, 1.0f, -1.0f, 1.0f);
    glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, glm::value_ptr(projection));
    glUniform1i(uniformTexture, 0);

    // Fondo.
    if (menu1BackgroundTexture != 0) {
        glm::mat4 model(1.0f);
        model = glm::scale(model, glm::vec3(aspect, 1.0f, 1.0f));
        const glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
        const glm::vec4 tintColor(1.0f, 1.0f, 1.0f, 1.0f);
        glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
        glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));
        glUniform4fv(uniformTintColor, 1, glm::value_ptr(tintColor));
        glUniform1f(uniformFlipX, 0.0f);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, menu1BackgroundTexture);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }

    const glm::vec4 whiteColor(1.0f, 1.0f, 1.0f, 1.0f);

    // Grupo de jugadores.
    const bool is1P = (settings.players == CustomPlayersOption::OnePlayer);
    const bool is2P = (settings.players == CustomPlayersOption::TwoPlayers);
    const bool is1PComp = (settings.players == CustomPlayersOption::OnePlayerPlusCpu);

    drawAtlasSprite(is1P ? "1P_Ama" : "1P_Bla", 720.0f, 210.0f, 0.0f, 0.0f, aspect,
                    uniformModel, uniformUvRect, uniformTintColor, uniformFlipX, whiteColor);
    drawAtlasSprite(is2P ? "2P_Ama" : "2P_Bla", 980.0f, 210.0f, 0.0f, 0.0f, aspect,
                    uniformModel, uniformUvRect, uniformTintColor, uniformFlipX, whiteColor);
    drawAtlasSprite(is1PComp ? "1P1Comp_Ama" : "1P1Comp_Bla", 1390.0f, 210.0f, 0.0f, 0.0f, aspect,
                    uniformModel, uniformUvRect, uniformTintColor, uniformFlipX, whiteColor);

    // Grupo versus/cooperativo.
    const bool modeEditable = (settings.players != CustomPlayersOption::OnePlayer);
    const bool vsSelected = modeEditable && (settings.teamMode == CustomTeamModeOption::Versus);
    const bool coopSelected = modeEditable && (settings.teamMode == CustomTeamModeOption::Cooperative);

    drawAtlasSprite(vsSelected ? "Vs_Ama" : "Vs_Bla", 800.0f, 365.0f, 0.0f, 0.0f, aspect,
                    uniformModel, uniformUvRect, uniformTintColor, uniformFlipX,
                    modeEditable ? whiteColor : glm::vec4(0.65f, 0.65f, 0.65f, 1.0f));
    drawAtlasSprite(coopSelected ? "Coop_Ama" : "Coop_Bla", 1240.0f, 365.0f, 0.0f, 0.0f, aspect,
                    uniformModel, uniformUvRect, uniformTintColor, uniformFlipX,
                    modeEditable ? whiteColor : glm::vec4(0.65f, 0.65f, 0.65f, 1.0f));

    // Grupo de tiempo limite.
    const bool t1 = (settings.timeLimit == CustomTimeLimitOption::OneMinute);
    const bool t2 = (settings.timeLimit == CustomTimeLimitOption::TwoMinutes);
    const bool t3 = (settings.timeLimit == CustomTimeLimitOption::ThreeMinutes);
    const bool tInf = (settings.timeLimit == CustomTimeLimitOption::Infinite);

    drawAtlasSprite(t1 ? "1min_Ama" : "1min_Bla", 660.0f, 520.0f, 0.0f, 0.0f, aspect,
                    uniformModel, uniformUvRect, uniformTintColor, uniformFlipX, whiteColor);
    drawAtlasSprite(t2 ? "2min_Ama" : "2min_Bla", 860.0f, 520.0f, 0.0f, 0.0f, aspect,
                    uniformModel, uniformUvRect, uniformTintColor, uniformFlipX, whiteColor);
    drawAtlasSprite(t3 ? "3min_Ama" : "3min_Bla", 1060.0f, 520.0f, 0.0f, 0.0f, aspect,
                    uniformModel, uniformUvRect, uniformTintColor, uniformFlipX, whiteColor);
    drawAtlasSprite(tInf ? "Infmin_Ama" : "Infmin_Bla", 1285.0f, 520.0f, 0.0f, 0.0f, aspect,
                    uniformModel, uniformUvRect, uniformTintColor, uniformFlipX, whiteColor);

    // Preview del mapa.
    const int clampedMapIndex = std::max(0, std::min(settings.mapIndex, static_cast<int>(kMapSpriteNames.size()) - 1));
    drawAtlasSprite(kMapSpriteNames[clampedMapIndex], 900.0f, 750.0f, 726.0f, 236.0f, aspect,
                    uniformModel, uniformUvRect, uniformTintColor, uniformFlipX, whiteColor);

    // Flecha de seleccion con el atlas de la bomba, como en menu.cpp.
    if (menuArrowTexture != 0) {
        const int row = std::max(0, std::min(menu1RowSelection, kMenu1Rows - 1));
        const float arrowCenterX = kMenu1ArrowXPixels[row];
        const float arrowCenterY = kMenu1ArrowYPixels[row];

        std::string spriteNameToUse = "explosion_0";
        if (!menuBombAtlas.sprites.empty()) {
            if (menuArrowSelected && row == (kMenu1Rows - 1)) {
                if (menuArrowAnimTimer < 0.05f) {
                    spriteNameToUse = "explosion_2";
                } else {
                    spriteNameToUse = (menuArrowAnimTimer < menuArrowSelectedAnimSpeed * 0.5f)
                        ? "explosion_3"
                        : "explosion_4";
                }
            } else {
                spriteNameToUse = (menuArrowAnimTimer < menuArrowAnimSpeed * 0.5f)
                    ? "explosion_0"
                    : "explosion_1";
            }
        }

        glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
        getUvRectForSprite(menuBombAtlas, spriteNameToUse, uvRect);

        const glm::vec2 center = pixelsToOrtho(arrowCenterX, arrowCenterY, aspect);
        const glm::vec2 halfExtents = sizePixelsToOrthoHalfExtents(kMenuBombSizePixels, kMenuBombSizePixels, aspect);
        glm::mat4 model(1.0f);
        model = glm::translate(model, glm::vec3(center.x, center.y, 0.0f));
        model = glm::scale(model, glm::vec3(halfExtents.x, halfExtents.y, 1.0f));

        glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
        glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));
        glUniform4fv(uniformTintColor, 1, glm::value_ptr(whiteColor));
        glUniform1f(uniformFlipX, 0.0f);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, menuArrowTexture);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }

    glBindVertexArray(0);
    glUseProgram(0);
}

void CustomGameMenu::renderMenu2(GLuint VAO,
                                 GLuint shader,
                                 GLuint uniformModel,
                                 GLuint uniformProjection,
                                 GLuint uniformTexture,
                                 GLuint uniformUvRect,
                                 GLuint uniformTintColor,
                                 GLuint uniformFlipX,
                                 int WIDTH,
                                 int HEIGHT) {
    glUseProgram(shader);
    glBindVertexArray(VAO);

    const float safeHeight = std::max(1, HEIGHT);
    const float aspect = static_cast<float>(WIDTH) / static_cast<float>(safeHeight);
    const glm::mat4 projection = glm::ortho(-aspect, aspect, -1.0f, 1.0f, -1.0f, 1.0f);
    glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, glm::value_ptr(projection));
    glUniform1i(uniformTexture, 0);

    // Fondo pantalla 2.
    if (menu2BackgroundTexture != 0) {
        glm::mat4 model(1.0f);
        model = glm::scale(model, glm::vec3(aspect, 1.0f, 1.0f));
        const glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
        const glm::vec4 tintColor(1.0f, 1.0f, 1.0f, 1.0f);
        glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
        glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));
        glUniform4fv(uniformTintColor, 1, glm::value_ptr(tintColor));
        glUniform1f(uniformFlipX, 0.0f);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, menu2BackgroundTexture);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }

    // Números de enemigos en distribución 3x3: amarillo si está seleccionado, blanco si no.
    for (int i = 0; i < kEnemyTypeCount; ++i) {
        const bool selected = (i == enemyTypeSelection);
        drawVocabTextCentered(std::to_string(enemyCounts[i]),
                              selected,
                              kEnemyCounterCenters[i].x,
                              kEnemyCounterCenters[i].y,
                              51.0f,
                              8.0f,
                              aspect,
                              uniformModel,
                              uniformUvRect,
                              uniformTintColor,
                              uniformFlipX);
    }

    // Texto superior con total actual y límite. Si llega al límite, el total actual va en amarillo.
    const int totalEnemies = getEnemyTotalCount();
    const bool totalAtLimit = (totalEnemies >= kEnemyTotalMax);

    drawVocabTextCentered(std::to_string(totalEnemies),
                          totalAtLimit,
                          1410.0f,
                          186.0f,
                          40.0f,
                          6.0f,
                          aspect,
                          uniformModel,
                          uniformUvRect,
                          uniformTintColor,
                          uniformFlipX);

    drawVocabTextCentered(std::to_string(kEnemyTotalMax),
                          false,
                          1555.0f,
                          186.0f,
                          40.0f,
                          6.0f,
                          aspect,
                          uniformModel,
                          uniformUvRect,
                          uniformTintColor,
                          uniformFlipX);

    // Flecha Play: misma bomba y mismas coordenadas que la fila NEXT del Menu 1.
    if (menuArrowTexture != 0) {
        const float arrowCenterX = kMenu1ArrowXPixels[kMenu1Rows - 1];
        const float arrowCenterY = kMenu1ArrowYPixels[kMenu1Rows - 1] + 10;

        std::string spriteNameToUse = "explosion_0";
        if (!menuBombAtlas.sprites.empty()) {
            if (menu2PlaySelected) {
                if (menu2ArrowAnimTimer < 0.05f) {
                    spriteNameToUse = "explosion_2";
                } else {
                    spriteNameToUse = (menu2ArrowAnimTimer < menuArrowSelectedAnimSpeed * 0.5f)
                        ? "explosion_3"
                        : "explosion_4";
                }
            } else {
                spriteNameToUse = (menu2ArrowAnimTimer < menuArrowAnimSpeed * 0.5f)
                    ? "explosion_0"
                    : "explosion_1";
            }
        }

        glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
        getUvRectForSprite(menuBombAtlas, spriteNameToUse, uvRect);

        const glm::vec2 center = pixelsToOrtho(arrowCenterX, arrowCenterY, aspect);
        const glm::vec2 halfExtents = sizePixelsToOrthoHalfExtents(kMenuBombSizePixels, kMenuBombSizePixels, aspect);
        glm::mat4 model(1.0f);
        model = glm::translate(model, glm::vec3(center.x, center.y, 0.0f));
        model = glm::scale(model, glm::vec3(halfExtents.x, halfExtents.y, 1.0f));

        const glm::vec4 whiteColor(1.0f, 1.0f, 1.0f, 1.0f);
        glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
        glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));
        glUniform4fv(uniformTintColor, 1, glm::value_ptr(whiteColor));
        glUniform1f(uniformFlipX, 0.0f);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, menuArrowTexture);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }

    glBindVertexArray(0);
    glUseProgram(0);
}
