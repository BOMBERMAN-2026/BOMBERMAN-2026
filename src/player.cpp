
#include "player.hpp"

Player::Player(glm::vec2 pos, glm::vec2 size, GLfloat velocity) {
    this->position = pos;
    this->speed = velocity;
    this->size = size;
}

void Player::UpdateSprite(Move mov) {
    switch (mov) {
        case MOVE_UP: this->position += glm::vec2(0, -0.010f); break;
        case MOVE_DOWN: this->position += glm::vec2(0, 0.010f); break;
        case MOVE_LEFT: this->position += glm::vec2(-0.010f, 0); break;
        case MOVE_RIGHT: this->position += glm::vec2(0.010f, 0); break;
    }
}
