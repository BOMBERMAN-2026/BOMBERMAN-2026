#include "score_popup.hpp"

#include "game_map.hpp"
#include "sprite_atlas.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

extern GLuint LoadTexture(const char* filePath);

namespace {

struct Popup {
    glm::vec2 position;
    int value = 0;
    int multiplier = 1;
    bool boss = false;
    float age = 0.0f;
    float duration = 1.15f;
};

SpriteAtlas gAtlas;
GLuint gTexture = 0;
std::vector<Popup> gPopups;

static constexpr float kDurationSeconds = 1.15f;
static constexpr float kFadeSeconds = 0.22f;
static constexpr float kDigitSpacingPx = 1.0f;
static constexpr float kBlinkIntervalSeconds = 0.12f;

float alphaFor(const Popup& popup) {
    const float fadeStart = std::max(0.0f, popup.duration - kFadeSeconds);
    if (popup.age <= fadeStart) return 1.0f;

    const float t = (popup.duration - popup.age) / std::max(0.001f, kFadeSeconds);
    return std::max(0.0f, std::min(1.0f, t));
}

bool collectDigits(const Popup& popup,
                   std::vector<std::pair<std::string, SpriteFrame>>& out,
                   float& totalWidthPx)
{
    out.clear();
    totalWidthPx = 0.0f;

    const bool useRed = (static_cast<int>(popup.age / kBlinkIntervalSeconds) % 2) != 0;
    const std::string colorSuffix = useRed ? "Rojo" : "Negro";
    const std::string valueSuffix = popup.boss
        ? "_Tocho_" + colorSuffix
        : "_" + colorSuffix;

    const std::string text = std::to_string(std::max(0, popup.value));
    for (char ch : text) {
        if (ch < '0' || ch > '9') continue;

        const std::string spriteName = std::string(1, ch) + valueSuffix;
        auto it = gAtlas.sprites.find(spriteName);
        if (it == gAtlas.sprites.end()) {
            continue;
        }

        if (!out.empty()) {
            totalWidthPx += kDigitSpacingPx;
        }
        totalWidthPx += (float)it->second.w;
        out.push_back(std::make_pair(spriteName, it->second));
    }

    if (popup.multiplier > 1) {
        const std::string bonusSuffix = "_Bonus_" + colorSuffix;

        // Add "x"
        const std::string xSprite = "x" + bonusSuffix;
        auto itX = gAtlas.sprites.find(xSprite);
        if (itX != gAtlas.sprites.end()) {
            // Space before "x"
            if (!out.empty()) {
                totalWidthPx += kDigitSpacingPx * 3.0f; 
            }
            totalWidthPx += (float)itX->second.w;
            out.push_back(std::make_pair(xSprite, itX->second));

            // Add digits for multiplier
            const std::string multText = std::to_string(popup.multiplier);
            for (char ch : multText) {
                if (ch < '0' || ch > '9') continue;
                const std::string multSprite = std::string(1, ch) + bonusSuffix;
                auto itM = gAtlas.sprites.find(multSprite);
                if (itM != gAtlas.sprites.end()) {
                    totalWidthPx += kDigitSpacingPx;
                    totalWidthPx += (float)itM->second.w;
                    out.push_back(std::make_pair(multSprite, itM->second));
                }
            }
        }
    }

    return !out.empty() && totalWidthPx > 0.0f;
}

glm::vec3 ndcToWorld3D(const GameMap* map, const glm::vec2& ndc, float y)
{
    if (!map) {
        return glm::vec3(0.0f, y, 0.0f);
    }

    const glm::vec2 cell00 = map->gridToNDC(0, 0);
    const glm::vec2 cell01 = (map->getCols() > 1)
        ? map->gridToNDC(0, 1)
        : glm::vec2(cell00.x + map->getTileSize(), cell00.y);
    const glm::vec2 cell10 = (map->getRows() > 1)
        ? map->gridToNDC(1, 0)
        : glm::vec2(cell00.x, cell00.y - map->getTileSize());

    const float dx = (std::abs(cell01.x - cell00.x) > 0.0001f)
        ? (cell01.x - cell00.x)
        : map->getTileSize();
    const float dy = (std::abs(cell10.y - cell00.y) > 0.0001f)
        ? (cell10.y - cell00.y)
        : -map->getTileSize();

    const float col = (ndc.x - cell00.x) / dx;
    const float row = (ndc.y - cell00.y) / dy;

    const float worldX = col - ((float)map->getCols() * 0.5f) + 0.5f;
    const float worldZ = row - ((float)map->getRows() * 0.5f) + 0.5f;
    return glm::vec3(worldX, y, worldZ);
}

void drawBillboard3D(GLuint vao,
                     GLuint uniformModel,
                     GLuint uniformUvRect,
                     GLuint uniformTintColor,
                     GLuint uniformFlipX,
                     const glm::vec3& cameraPos,
                     const glm::vec4& uvRect,
                     const glm::vec3& feetPos,
                     float width,
                     float height,
                     float alpha)
{
    const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
    glm::vec3 toCamera = cameraPos - feetPos;
    if (glm::length(toCamera) < 0.0001f) {
        toCamera = glm::vec3(0.0f, 0.0f, 1.0f);
    }

    const glm::vec3 viewDir = glm::normalize(toCamera);
    glm::vec3 right = glm::cross(worldUp, viewDir);
    if (glm::length(right) < 0.0001f) {
        right = glm::vec3(1.0f, 0.0f, 0.0f);
    } else {
        right = glm::normalize(right);
    }
    const glm::vec3 forward = glm::normalize(glm::cross(right, worldUp));

    const float halfW = width * 0.5f;
    const float halfH = height * 0.5f;

    glm::mat4 model(1.0f);
    model[0] = glm::vec4(right * halfW, 0.0f);
    model[1] = glm::vec4(worldUp * halfH, 0.0f);
    model[2] = glm::vec4(forward * 0.5f, 0.0f);
    model[3] = glm::vec4(feetPos + worldUp * halfH, 1.0f);

    glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
    glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));
    glUniform1f(uniformFlipX, 0.0f);
    glUniform4fv(uniformTintColor, 1, glm::value_ptr(glm::vec4(1.0f, 1.0f, 1.0f, alpha)));
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

} // namespace

namespace ScorePopup {

bool loadAssets()
{
    if (gTexture != 0) return true;

    const std::string atlasPath = resolveAssetPath("resources/sprites/atlases/SpriteAtlasVocPuntuaciones.json");
    if (!loadSpriteAtlasMinimal(atlasPath, gAtlas)) {
        std::cerr << "Error cargando atlas de puntuaciones: " << atlasPath << std::endl;
        return false;
    }

    const std::string texturePath = resolveAssetPath(gAtlas.imagePath);
    gTexture = LoadTexture(texturePath.c_str());
    if (gTexture == 0) {
        std::cerr << "Error cargando textura de puntuaciones: " << texturePath << std::endl;
        return false;
    }

    return true;
}

void shutdown()
{
    clear();
    if (gTexture != 0) {
        glDeleteTextures(1, &gTexture);
        gTexture = 0;
    }
    gAtlas = SpriteAtlas{};
}

void clear()
{
    gPopups.clear();
}

void spawn(const glm::vec2& position, int value, int multiplier, bool boss)
{
    if (value <= 0) return;

    Popup popup;
    popup.position = position;
    popup.value = value;
    popup.multiplier = multiplier;
    popup.boss = boss;
    popup.age = 0.0f;
    popup.duration = kDurationSeconds;
    gPopups.push_back(popup);
}

void update(float deltaTime)
{
    for (auto& popup : gPopups) {
        popup.age += deltaTime;
    }

    gPopups.erase(
        std::remove_if(gPopups.begin(), gPopups.end(),
                       [](const Popup& popup) {
                           return popup.age >= popup.duration;
                       }),
        gPopups.end());
}

void render2D(const GameMap* map,
              GLuint vao,
              GLuint uniformModel,
              GLuint uniformUvRect,
              GLuint uniformTintColor,
              GLuint uniformFlipX,
              GLuint uniformWhiteFlash)
{
    if (!map || gPopups.empty() || gTexture == 0 || vao == 0) return;

    const float pixelScale = map->getTileSize() / 64.0f;
    const float tileSize = map->getTileSize();
    std::vector<std::pair<std::string, SpriteFrame>> digits;

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gTexture);
    glBindVertexArray(vao);
    glUniform1f(uniformFlipX, 0.0f);
    glUniform1f(uniformWhiteFlash, 0.0f);

    for (const auto& popup : gPopups) {
        float totalWidthPx = 0.0f;
        if (!collectDigits(popup, digits, totalWidthPx)) continue;

        const float alpha = alphaFor(popup);
        const float progress = std::min(1.0f, popup.age / std::max(0.001f, popup.duration));
        const float startX = popup.position.x - totalWidthPx * pixelScale * 0.5f;
        const float centerY = popup.position.y + tileSize * (0.30f + 0.10f * progress);

        float cursorPx = 0.0f;
        for (const auto& digit : digits) {
            glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
            if (!getUvRectForSprite(gAtlas, digit.first, uvRect)) continue;

            const SpriteFrame& frame = digit.second;
            const float halfW = (float)frame.w * pixelScale * 0.5f;
            const float halfH = (float)frame.h * pixelScale * 0.5f;
            const float centerX = startX + cursorPx * pixelScale + halfW;

            glm::mat4 model(1.0f);
            model = glm::translate(model, glm::vec3(centerX, centerY, 0.0f));
            model = glm::scale(model, glm::vec3(halfW, halfH, 1.0f));

            glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
            glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));
            glUniform4fv(uniformTintColor, 1, glm::value_ptr(glm::vec4(1.0f, 1.0f, 1.0f, alpha)));
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

            cursorPx += (float)frame.w + kDigitSpacingPx;
        }
    }

    glUniform4fv(uniformTintColor, 1, glm::value_ptr(glm::vec4(1.0f)));
    glBindVertexArray(0);
}

void render3D(const GameMap* map,
              const glm::mat4& viewProjection,
              const glm::vec3& cameraPos,
              GLuint vao,
              GLuint uniformProjection,
              GLuint uniformModel,
              GLuint uniformUvRect,
              GLuint uniformTintColor,
              GLuint uniformFlipX,
              GLuint uniformWhiteFlash)
{
    if (!map || gPopups.empty() || gTexture == 0 || vao == 0) return;

    std::vector<std::pair<std::string, SpriteFrame>> digits;
    const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
    const float digitWorldScale = 0.0245f;

    glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, glm::value_ptr(viewProjection));
    glUniform1f(uniformWhiteFlash, 0.0f);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gTexture);
    glBindVertexArray(vao);

    for (const auto& popup : gPopups) {
        float totalWidthPx = 0.0f;
        if (!collectDigits(popup, digits, totalWidthPx)) continue;

        const float progress = std::min(1.0f, popup.age / std::max(0.001f, popup.duration));
        const glm::vec3 baseFeet = ndcToWorld3D(map, popup.position, 1.18f + 0.10f * progress);
        glm::vec3 toCamera = cameraPos - baseFeet;
        if (glm::length(toCamera) < 0.0001f) {
            toCamera = glm::vec3(0.0f, 0.0f, 1.0f);
        }

        const glm::vec3 viewDir = glm::normalize(toCamera);
        glm::vec3 right = glm::cross(worldUp, viewDir);
        if (glm::length(right) < 0.0001f) {
            right = glm::vec3(1.0f, 0.0f, 0.0f);
        } else {
            right = glm::normalize(right);
        }

        const float alpha = alphaFor(popup);
        const float totalWidth = totalWidthPx * digitWorldScale;
        float cursorPx = 0.0f;

        for (const auto& digit : digits) {
            glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
            if (!getUvRectForSprite(gAtlas, digit.first, uvRect)) continue;

            const SpriteFrame& frame = digit.second;
            const float digitWidth = (float)frame.w * digitWorldScale;
            const float digitHeight = (float)frame.h * digitWorldScale;
            const float centerOffset = -totalWidth * 0.5f + cursorPx * digitWorldScale + digitWidth * 0.5f;
            const glm::vec3 digitFeet = baseFeet + right * centerOffset;

            drawBillboard3D(vao,
                            uniformModel,
                            uniformUvRect,
                            uniformTintColor,
                            uniformFlipX,
                            cameraPos,
                            uvRect,
                            digitFeet,
                            digitWidth,
                            digitHeight,
                            alpha);

            cursorPx += (float)frame.w + kDigitSpacingPx;
        }
    }

    glUniform4fv(uniformTintColor, 1, glm::value_ptr(glm::vec4(1.0f)));
    glBindVertexArray(0);
}

} // namespace ScorePopup
