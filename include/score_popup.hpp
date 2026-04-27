#ifndef SCORE_POPUP_HPP
#define SCORE_POPUP_HPP

#define GLEW_STATIC
#include <GL/glew.h>
#include <glm/glm.hpp>

class GameMap;

namespace ScorePopup {

bool loadAssets();
void shutdown();
void clear();
void spawn(const glm::vec2& position, int value, int multiplier = 1);
void update(float deltaTime);

void render2D(const GameMap* map,
              GLuint vao,
              GLuint uniformModel,
              GLuint uniformUvRect,
              GLuint uniformTintColor,
              GLuint uniformFlipX,
              GLuint uniformWhiteFlash);

void render3D(const GameMap* map,
              const glm::mat4& viewProjection,
              const glm::vec3& cameraPos,
              GLuint vao,
              GLuint uniformProjection,
              GLuint uniformModel,
              GLuint uniformUvRect,
              GLuint uniformTintColor,
              GLuint uniformFlipX,
              GLuint uniformWhiteFlash);

} // namespace ScorePopup

#endif // SCORE_POPUP_HPP
