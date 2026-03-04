
#include "player.hpp"
#include <algorithm>

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
        case MOVE_UP:    this->position += glm::vec2( 0.0f,    0.010f); break;
        case MOVE_DOWN:  this->position += glm::vec2( 0.0f,   -0.010f); break;
        case MOVE_LEFT:  this->position += glm::vec2(-0.010f,   0.0f  ); break;
        case MOVE_RIGHT: this->position += glm::vec2( 0.010f,   0.0f  ); break;
        default: break;
    }

    // Limitar la posición dentro de los bordes de la ventana (clip space -1..1)
    // El jugador se escala 0.05 sobre un quad de -1..1, así que su semitamaño es 0.05
    const float halfSize = 0.05f;
    this->position.x = std::max(-1.0f + halfSize, std::min(this->position.x, 1.0f - halfSize));
    this->position.y = std::max(-1.0f + halfSize, std::min(this->position.y, 1.0f - halfSize));
}
