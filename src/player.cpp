
#include "player.hpp"

Player::Player(glm::vec2 pos, glm::vec2 size, GLfloat velocity)
    : Entity(pos, size, velocity) {}

Player::~Player() {}

void Player::Update() {
    // Lógica de actualización del jugador (a completar)
}

void Player::Draw() {
    // Lógica de renderizado del jugador (a completar)
}

void Player::UpdateSprite(Move mov) {
    switch (mov) {
        case MOVE_UP:    this->position += glm::vec2( 0.0f,    -0.010f); break;
        case MOVE_DOWN:  this->position += glm::vec2( 0.0f,     0.010f); break;
        case MOVE_LEFT:  this->position += glm::vec2(-0.010f,   0.0f  ); break;
        case MOVE_RIGHT: this->position += glm::vec2( 0.010f,   0.0f  ); break;
        default: break;
    }
}
