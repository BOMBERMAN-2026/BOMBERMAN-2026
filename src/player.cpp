
#include "player.hpp"
#include "game_map.hpp"
#include <algorithm>
#include <cmath>

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
    const float step     = this->speed * deltaTime;
    const float halfTile = map->getTileSize() / 2.0f;

    // --- Snap perpendicular al movimiento hacia el centro del tile actual ---
    // Impide que el jugador se cuele por las esquinas en pasillos de 1 tile.
    {
        const float snapStrength = 0.25f;         // fracción de corrección por frame
        const float snapMaxDist  = halfTile * 0.45f; // umbral máximo para aplicar
        int tr, tc;
        map->ndcToGrid(this->position, tr, tc);
        glm::vec2 tileCenter = map->gridToNDC(tr, tc);

        if (mov == MOVE_LEFT || mov == MOVE_RIGHT) {
            float dy = tileCenter.y - this->position.y;
            if (std::abs(dy) <= snapMaxDist)
                this->position.y += dy * snapStrength;
        } else {
            float dx = tileCenter.x - this->position.x;
            if (std::abs(dx) <= snapMaxDist)
                this->position.x += dx * snapStrength;
        }
    }

    glm::vec2 newPos = this->position;
    switch (mov) {
        case MOVE_UP:    newPos.y += step; break;
        case MOVE_DOWN:  newPos.y -= step; break;
        case MOVE_LEFT:  newPos.x -= step; break;
        case MOVE_RIGHT: newPos.x += step; break;
        default: return;
    }

    // --- Sondas de colisión: dos esquinas en el borde frontal de la dirección ---
    // Cada dirección comprueba ambas esquinas del lado que avanza.
    {
        int r, c;
        const float eFront = halfTile;          // siempre cae en el tile vecino
        const float eSide  = halfTile * 0.60f; // semiancho del hitbox

        if (mov == MOVE_UP) {
            map->ndcToGrid({newPos.x - eSide, newPos.y + eFront}, r, c);
            if (!map->isWalkable(r, c)) return;
            map->ndcToGrid({newPos.x + eSide, newPos.y + eFront}, r, c);
            if (!map->isWalkable(r, c)) return;
        }
        if (mov == MOVE_DOWN) {
            map->ndcToGrid({newPos.x - eSide, newPos.y - eFront}, r, c);
            if (!map->isWalkable(r, c)) return;
            map->ndcToGrid({newPos.x + eSide, newPos.y - eFront}, r, c);
            if (!map->isWalkable(r, c)) return;
        }
        if (mov == MOVE_LEFT) {
            map->ndcToGrid({newPos.x - eFront, newPos.y - eSide}, r, c);
            if (!map->isWalkable(r, c)) return;
            map->ndcToGrid({newPos.x - eFront, newPos.y + eSide}, r, c);
            if (!map->isWalkable(r, c)) return;
        }
        if (mov == MOVE_RIGHT) {
            map->ndcToGrid({newPos.x + eFront, newPos.y - eSide}, r, c);
            if (!map->isWalkable(r, c)) return;
            map->ndcToGrid({newPos.x + eFront, newPos.y + eSide}, r, c);
            if (!map->isWalkable(r, c)) return;
        }
    }

    this->position = newPos;
}
