
#include "player.hpp"
#include "game_map.hpp"
#include <algorithm>

/*
 * player.cpp
 * ----------
 * Implementación del `Player`.
 *
 * Nota:
 * - El movimiento real se aplica en `UpdateSprite` (según una dirección) y se valida
 *   contra el `GameMap` usando una combinación de sondas + `canMoveTo`.
 * - `Update`/`Draw` están como placeholders (la renderización se realiza desde Game).
 */

// ============================== Ctor / dtor ==============================

Player::Player(glm::vec2 pos, glm::vec2 size, GLfloat velocity)
    : Entity(pos, size, velocity) {}

Player::~Player() {}

// ============================== API base (placeholders) ==============================

void Player::Update() {
    // Lógica de actualización del jugador (a completar)
}

void Player::Draw() {
    // Lógica de renderizado del jugador (a completar)
}

// Aplica un paso de movimiento en NDC y realiza colisión contra el grid del mapa.
void Player::UpdateSprite(Move mov, const GameMap* map, float deltaTime) {
    const float step = this->speed * deltaTime;

    glm::vec2 newPos = this->position;
    switch (mov) {
        case MOVE_UP:    newPos.y += step; break;
        case MOVE_DOWN:  newPos.y -= step; break;
        case MOVE_LEFT:  newPos.x -= step; break;
        case MOVE_RIGHT: newPos.x += step; break;
        default: return;
    }

    float halfTile = map->getTileSize() / 2.0f;

    // Sondas extra en el borde visual del sprite.
    // Cada dirección usa la misma distancia que canMoveTo para ese eje,
    // así la distancia de parada es coherente en todas las direcciones.
    {
        int r, c;
        const float edgeUp    = halfTile * 0.45f; // igual que mY de canMoveTo
        const float edgeDown  = halfTile * 0.95f; // protege borde inferior del mapa
        const float edgeLR    = halfTile * 0.72f; // igual que mX de canMoveTo
        if (mov == MOVE_UP) {
            map->ndcToGrid({newPos.x, newPos.y + edgeUp}, r, c);
            if (!map->isWalkable(r, c)) return;
        }
        if (mov == MOVE_DOWN) {
            map->ndcToGrid({newPos.x, newPos.y - edgeDown}, r, c);
            if (!map->isWalkable(r, c)) return;
        }
        if (mov == MOVE_LEFT) {
            map->ndcToGrid({newPos.x - edgeLR, newPos.y}, r, c);
            if (!map->isWalkable(r, c)) return;
        }
        if (mov == MOVE_RIGHT) {
            map->ndcToGrid({newPos.x + edgeLR, newPos.y}, r, c);
            if (!map->isWalkable(r, c)) return;
        }
    }

    if (map->canMoveTo(newPos, halfTile)) {
        this->position = newPos;
    }
}
