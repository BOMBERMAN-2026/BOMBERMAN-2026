
#include "player.hpp"
#include "game_map.hpp"
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

void Player::UpdateSprite(Move mov, const GameMap* map) {
    const float step = this->speed;

    glm::vec2 newPos = this->position;
    switch (mov) {
        case MOVE_UP:    newPos.y += step; break;
        case MOVE_DOWN:  newPos.y -= step; break;
        case MOVE_LEFT:  newPos.x -= step; break;
        case MOVE_RIGHT: newPos.x += step; break;
        default: return;
    }

    float halfTile = map->getTileSize() / 2.0f;
    if (map->canMoveTo(newPos, halfTile)) {
        this->position = newPos;
    }
}
